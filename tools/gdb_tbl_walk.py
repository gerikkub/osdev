

class PageTableWalkCmd(gdb.Command):
    """Walks a page table"""

    PT_USER_READ = 1
    PT_USER_WRITE = 2
    PT_USER_EXEC = 4
    PT_PRIV_READ = 8
    PT_PRIV_WRITE = 16
    PT_PRIV_EXEC = 32

    PT_INVALID = 1
    PT_BLOCK = 2
    PT_TABLE = 3
    PT_PAGE = 4
    
    ATTR_UXN = 54
    ATTR_PXN = 53
    ATTR_AP = 6

    pt_type = gdb.lookup_type('_vmem_entry_t').pointer()
    pt_entry_type = gdb.lookup_type('_vmem_entry_t')
    pt_page = gdb.lookup_type('uint8_t').pointer()

    def __init__(self):
        super(PageTableWalkCmd, self).__init__(
            "pt_walk", gdb.COMMAND_USER
        )
    
    def parse_pt_entry(self, entry, level):
        attributes = 0
        kind = 0
        address = 0
        if (entry & 1) == 0:
            kind = self.PT_INVALID
        else:
            if level < 3:
                if (entry & 2) == 0:
                    # Block
                    kind = self.PT_BLOCK
                    # TODO: Implement block attributes
                else:
                    # Table
                    kind = self.PT_TABLE
                    address = entry & 0xFFFFFFFFF000
            else:
                # Page
                kind = self.PT_PAGE
                address = entry & 0xFFFFFFFFF000
                if (entry & (1 << self.ATTR_UXN)) == 0:
                    attributes |= self.PT_USER_EXEC

                if (entry & (1 << self.ATTR_PXN)) == 0:
                    attributes |= self.PT_PRIV_EXEC
                
                ap = (entry & (3 << self.ATTR_AP)) >> self.ATTR_AP
                if ap == 0:
                    attributes |= self.PT_PRIV_READ
                    attributes |= self.PT_PRIV_WRITE
                elif ap == 1:
                    attributes |= self.PT_PRIV_READ
                    attributes |= self.PT_PRIV_WRITE
                    attributes |= self.PT_USER_READ
                    attributes |= self.PT_USER_WRITE
                elif ap == 2:
                    attributes |= self.PT_PRIV_READ
                elif ap == 3:
                    attributes |= self.PT_PRIV_READ
                    attributes |= self.PT_USER_READ

        return (kind, attributes, address)

    def level_lookup(self, table, idx, level):

        entry = int((table + idx).dereference())
        #print(hex(entry))
        kind, attr, addr = self.parse_pt_entry(entry, level)
        #print("{} : {} : {}".format(kind, attr, addr))


        if kind == self.PT_INVALID:
            return "l{} lookup invalid".format(level)
        else:
            if level < 3:
                if kind == self.PT_BLOCK:
                    return "l{} lookup block".format(level)
        
        if level < 3:
            table = gdb.Value(addr + 0xFFFF000000000000).cast(self.pt_type)
            return (table, attr)
        else:
            page = gdb.Value(addr).cast(self.pt_page)
            return (page, attr)
            
    def print_table(self, result, table, level):
        return result + "Level {}: {}\n".format(level, table)

    def _pt_walk(self, table_va, address):

        result = "Looking up {} in PT {}\n".format(hex(int(address)), hex(int(table_va)))

        addr_int = int(address)
        l0_idx = (addr_int >> 39) & 0x1FF
        l1_idx = (addr_int >> 30) & 0x1FF
        l2_idx = (addr_int >> 21) & 0x1FF
        l3_idx = (addr_int >> 12) & 0x1FF
        page_offset = addr_int & 0xFFF

        l0_table = table_va.cast(self.pt_type)
        self.print_table(result, l0_table, 0)

        # l0 lookup
        l0_res = self.level_lookup(l0_table, l0_idx, 0)
        if type(l0_res) is str:
            return result + l0_res
        l1_table, l0_attr = l0_res
        result = self.print_table(result, l1_table, 1)

        # l1 lookup
        l1_res = self.level_lookup(l1_table, l1_idx, 1)
        if type(l1_res) is str:
            return result + l1_res
        l2_table, l1_attr = l1_res
        result = self.print_table(result, l2_table, 2)

        # l2 lookup
        l2_res = self.level_lookup(l2_table, l2_idx, 2)
        if type(l2_res) is str:
            return result + l2_res
        l3_table, l2_attr = l2_res
        result = self.print_table(result, l3_table, 3)

        # l3 lookup
        l3_res = self.level_lookup(l3_table, l3_idx, 3)
        if type(l3_res) is str:
            return result + l3_res
        l3_page, l3_attr = l3_res

        user_attr = ""
        if l3_attr & self.PT_USER_READ:
            user_attr += 'R'
        else:
            user_attr += '-'
        if l3_attr & self.PT_USER_WRITE:
            user_attr += 'W'
        else:
            user_attr += '-'
        if l3_attr & self.PT_USER_EXEC:
            user_attr += 'X'
        else:
            user_attr += '-'

        priv_attr = ""
        if l3_attr & self.PT_PRIV_READ:
            priv_attr += 'R'
        else:
            priv_attr += '-'
        if l3_attr & self.PT_PRIV_WRITE:
            priv_attr += 'W'
        else:
            priv_attr += '-'
        if l3_attr & self.PT_PRIV_EXEC:
            priv_attr += 'X'
        else:
            priv_attr += '-'

        result += "l3 page address: {}\n".format(hex(l3_page))
        result += "Priv: {}\nUser: {}\n".format(priv_attr, user_attr)


        return result
    
    def arg_to_val(self, arg):

        val = gdb.parse_and_eval(arg)
        if arg[0] == '$':
            # This is a register argument
            val_int = int(val)
            val_fixed = (val_int & 0xFFFFFFFFFFFF) | 0xFFFF000000000000
            val = gdb.Value(val_fixed).cast(self.pt_type)
        else:
            val = gdb.parse_and_eval(arg)
            if val.type.code != gdb.TYPE_CODE_PTR:
                print("{} is not a pointer {} {}".format(arg, val.type.code, int(val)))
                return None
        
        return val
    
    def complete(self, text, word):
        return gdb.COMPLETE_SYMBOL
    
    def invoke(self, args_str, from_tty):

        args = gdb.string_to_argv(args_str)
        if len(args) != 2:
            print("Expected args: TableVA Address")
            return

        table_va = self.arg_to_val(args[0])
        addr = self.arg_to_val(args[1])

        addr_int = int(addr)
        addr_fixed = (addr_int & 0xFFFFFFFFFFFF) | 0xFFFF000000000000
        addr_va = gdb.Value(addr_fixed).cast(self.pt_page)

        if table_va is None or addr is None:
            print("Invalid arguments")
            return
        
        print(self._pt_walk(table_va, addr))

PageTableWalkCmd()