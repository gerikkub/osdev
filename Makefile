##########################################################################################################################
# File automatically-generated by tool: [projectgenerator] version: [3.1.0] date: [Sun Mar 03 11:47:26 PST 2019] 
##########################################################################################################################

# ------------------------------------------------
# Generic Makefile (based on gcc)
#
# ChangeLog :
#	2017-02-10 - Several enhancements + project update mode
#   2015-07-22 - first version
# ------------------------------------------------

######################################
# target
######################################
TARGET = kernel


######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og

#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

SOURCE_DIR := $(shell pwd)

TOOLS_DIR = $(SOURCE_DIR)/../tools

COMP_DIR = $(TOOLS_DIR)/compiler/bin
#COMP_DIR = /usr/local/bin

SYSTEMS_DIR = system

MODULES_BUILD = $(SYSTEMS_DIR)/build

QEMU_BIN = /usr/local/share/qemu/bin/qemu-system-aarch64
#QEMU_BIN = /usr/local/bin/qemu-system-aarch64

GENEXT2FS_BIN = /home/gerik/projects/osdev-monolithic/tools/genext2fs/bin/genext2fs

######################################
# source
######################################

C_SRC_BOOTSTRAP = \
bootstrap/main_bootstrap.c \
bootstrap/vmem_bootstrap.c

C_SRC_DRIVERS = \
drivers/pl011_uart.c \
drivers/qemu_fw_cfg.c \
drivers/pcie/pcie.c \
drivers/virtio_pci_blk/virtio_pci_blk.c \
drivers/console/console_dev.c \
drivers/gicv3/gicv3.c \
drivers/aarch64/aarch64.c

C_SRC_KERNEL = \
kernel/main.c \
kernel/console.c \
kernel/exception.c \
kernel/exception_handler_table.c \
kernel/kmalloc.c \
kernel/panic.c \
kernel/vmem.c \
kernel/pagefault.c \
kernel/gtimer.c \
kernel/task.c \
kernel/syscall.c \
kernel/memoryspace.c \
kernel/kernelspace.c \
kernel/modules.c \
kernel/elf.c \
kernel/messages.c \
kernel/dtb.c \
kernel/drivers.c \
kernel/vfs.c \
kernel/sys_device.c \
kernel/fs_manager.c \
kernel/fd.c \
kernel/exec.c \
kernel/interrupt/interrupt.c

C_SRC_KERNEL_LIBS = \
kernel/lib/libdtb.c \
kernel/lib/libpci.c \
kernel/lib/libvirtio.c \
kernel/lib/vmalloc.c \
kernel/lib/llist.c \
kernel/lock/lock.c \
kernel/lock/mutex.c \

C_SRC_KERNEL_FS = \
kernel/fs/ext2.c \
kernel/fs/ext2_helpers.c

C_SRC_LIBS = \
stdlib/string.c \
stdlib/printf.c \
stdlib/bitutils.c \
stdlib/linalloc.c \
stdlib/malloc.c \
stdlib/bitalloc.c


# C sources
C_SOURCES = ${C_SRC_BOOTSTRAP} ${C_SRC_KERNEL} ${C_SRC_DRIVERS} ${C_SRC_LIBS} ${C_SRC_KERNEL_LIBS}
C_SOURCES += ${C_SRC_KERNEL_FS}

# ASM sources
ASM_SOURCES =  \
bootstrap/start.s \
kernel/high_start.s \
kernel/exception_asm.s

SYS_MODS = \
$(SYSTEMS_DIR)/cat \
$(SYSTEMS_DIR)/echo \
$(SYSTEMS_DIR)/gsh

MODULES = $(foreach MOD,$(notdir $(SYS_MODS)),$(SYSTEMS_DIR)/$(BUILD_DIR)/$(MOD)/$(MOD).elf)

DISKIMG = drive_ext2.img

#######################################
# binaries
#######################################
PREFIX = aarch64-none-elf
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
CC = $(COMP_DIR)/$(PREFIX)-gcc
AS = $(COMP_DIR)/$(PREFIX)-gcc -x assembler-with-cpp
CP = $(COMP_DIR)/$(PREFIX)-objcopy
SZ = $(COMP_DIR)/$(PREFIX)-size
LD = $(COMP_DIR)/$(PREFIX)-gcc
BIN = $(CP) -O binary -S
 
#######################################
# CFLAGS
#######################################
# cpu
CPU = 

# fpu
# NONE for Cortex-M0/M0+/M3

# float-abi


# mcu
MCU = $(CPU) $(FPU) $(FLOAT-ABI)

# macros for gcc
# AS defines
AS_DEFS = 

# C defines
C_DEFS = 


# AS includes
AS_INCLUDES = 

# C includes
C_INCLUDES = -I . -I stdlib
#-I $(TOOLS_DIR)/aarch64-none-elf/include

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) \
	     -ffreestanding -Wall -Werror \
		 -mno-pc-relative-literal-loads \
		 -mgeneral-regs-only \
		 -march=armv8.2-a

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif


# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)" -DKERNEL_BUILD


SYSTEMS_CFLAGS = $(CFLAGS) 

#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = kernel.ld

# libraries
LIBS = 
LIBDIR = 
LDFLAGS = $(MCU) -Wl,-T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map -ffreestanding -nostdlib
 #-specs=$(TOOLS_DIR)/aarch64-none-elf/lib/nosys.specs
# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf


#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
# list of modules
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(MODULES:.elf=.elf.o)))
vpath %.elf $(sort $(dir $(MODULES)))

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.elf.o: %.elf | $(BUILD_DIR)
	$(CP) --input-target=binary --output-target=elf64-littleaarch64 --binary-architecture aarch64 $< $@
	#$(COMP_DIR)/bin/$(PREFIX)-ld -r -b binary $< -o $@

$(MODULES_BUILD)/%.elf: FORCE
	make MOD=$(basename $(notdir $@)) -f $(SYSTEMS_DIR)/Makefile

FORCE: ;

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(MODULES) $(LD_SCRIPT)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	mkdir $@		

$(DISKIMG): $(MODULES)
	rm -rf diskdata
	mkdir -p diskdata/bin
	cp $(MODULES) diskdata/bin/
	cp -r diskextras/* diskdata/
	$(GENEXT2FS_BIN) -b 1440 -d diskdata/ $@
#	rm -rf diskdata

print-% : ; @echo $* = $($*)


#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)
  
#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)


run: $(BUILD_DIR)/$(TARGET).elf $(DISKIMG)
	$(QEMU_BIN) -M virt,gic-version=3 -cpu cortex-a57 -nographic -s -kernel $< \
	-drive file=drive_ext2.img,id=disk0,if=none -device virtio-blk-pci,drive=disk0,disable-legacy=on

debug: $(BUILD_DIR)/$(TARGET).elf $(DISKIMG)
	$(QEMU_BIN) -M virt,gic-version=3 -cpu cortex-a57 -nographic -S -s -kernel $< \
	-drive file=drive_ext2.img,id=disk0,if=none -device virtio-blk-pci,drive=disk0,disable-legacy=on

dts:
	$(QEMU_BIN) -M virt,gic-version=3 -cpu cortex-a57 -nographic -S -s \
	-drive file=drive_ext2.img,id=disk0,if=none -device virtio-blk-pci,drive=disk0,disable-legacy=on \
	-machine dumpdtb=dtb.dtb
	dtc -I dtb -O dts -o tools/dts.txt dtb.dtb

.PHONY: run debug dts

# *** EOF ***
