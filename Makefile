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

#COMP_DIR = $(TOOLS_DIR)/compiler/bin
COMP_DIR = /usr/local/bin

SYSTEMS_DIR = system

MODULES_BUILD = $(SYSTEMS_DIR)/build

######################################
# source
######################################

C_SRC_BOOTSTRAP = \
bootstrap/main_bootstrap.c \
bootstrap/vmem_bootstrap.c

C_SRC_DRIVERS = \
drivers/cortex_a57_gic.c \
drivers/pl011_uart.c

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
kernel/gic.c \
kernel/task.c \
kernel/syscall.c \
kernel/memoryspace.c \
kernel/kernelspace.c \
kernel/modules.c \
kernel/elf.c \
kernel/messages.c

C_SRC_LIBS = \
stdlib/string.c


# C sources
C_SOURCES = ${C_SRC_BOOTSTRAP} ${C_SRC_KERNEL} ${C_SRC_DRIVERS} ${C_SRC_LIBS}

# ASM sources
ASM_SOURCES =  \
bootstrap/start.s \
kernel/high_start.s \
kernel/exception_asm.s

SYS_MODS = \
$(SYSTEMS_DIR)/ext2 \
$(SYSTEMS_DIR)/vfs

MODULES = $(foreach MOD,$(notdir $(SYS_MODS)),$(SYSTEMS_DIR)/$(BUILD_DIR)/$(MOD)/$(MOD).elf)

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
		 -mgeneral-regs-only

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif


# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"


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

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.elf.o: %.elf Makefile | $(BUILD_DIR)
	$(CP) --input-target=binary --output-target=elf64-littleaarch64 --binary-architecture aarch64 $< $@
	#$(COMP_DIR)/bin/$(PREFIX)-ld -r -b binary $< -o $@

$(MODULES_BUILD)/%.elf: FORCE
	make MOD=$(basename $(notdir $@)) -f $(SYSTEMS_DIR)/Makefile

FORCE: ;

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(MODULES) Makefile $(LD_SCRIPT)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	mkdir $@		

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


run: $(BUILD_DIR)/$(TARGET).elf
	qemu-system-aarch64 -M virt -cpu cortex-a57 -nographic -s -kernel $<

debug: $(BUILD_DIR)/$(TARGET).elf
	qemu-system-aarch64 -M virt -cpu cortex-a57 -nographic -S -s -kernel $<

.PHONY: run debug

# *** EOF ***
