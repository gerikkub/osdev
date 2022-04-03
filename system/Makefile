

SOURCE_DIR := $(shell pwd)/

TOOLS_DIR = $(SOURCE_DIR)/../tools

COMP_DIR = $(TOOLS_DIR)/compiler/bin
#COMP_DIR = /usr/local/bin

SYSTEMS_DIR = $(SOURCE_DIR)/system
STDLIB_DIR = $(SOURCE_DIR)/stdlib

BUILD_DIR = $(SYSTEMS_DIR)/build


TARGET = $(BUILD_DIR)/$(MOD)/$(MOD).elf

MOD_SRC_DIR = $(SYSTEMS_DIR)/$(MOD)

#C_SOURCES = $(foreach SRC,$(notdir $(shell echo $(MOD_SRC_DIR)/*.c)),$(MOD)/$(SRC))
C_SOURCES = $(foreach SRC,$(notdir $(wildcard $(MOD_SRC_DIR)/*.c)),$(MOD)/$(SRC))
C_SYS_SOURCES = $(foreach SRC,$(notdir $(wildcard $(SYSTEMS_DIR)/lib/*.c)),$(SYSTEMS_DIR)/lib/$(SRC))
C_SYS_SOURCES += $(foreach SRC,$(notdir $(wildcard $(STDLIB_DIR)/*.c)),$(STDLIB_DIR)/$(SRC))

#ASM_SOURCES = $(foreach SRC,$(notdir $(shell echo $(MOD_SRC_DIR)/*.s)),$(MOD)/$(SRC))
ASM_SOURCES = $(foreach SRC,$(notdir $(wildcard $(MOD_SRC_DIR)/*.s)),$(MOD)/$(SRC))

ASM_SOURCES += system_entry.s


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

DEBUG = 1

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
C_INCLUDES = -I $(SOURCE_DIR) -I $(TOOLS_DIR)/aarch64-none-elf/include

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
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = $(SYSTEMS_DIR)/system.ld

# libraries
LIBS = 
LIBDIR = -L$(TOOLS_DIR)/aarch64-none-elf/lib/
LDFLAGS = $(MCU) -Wl,-T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(TARGET).map -ffreestanding -nostdlib

# default action: build all
.PHONY: all
all: $(TARGET)

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/$(MOD)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(MOD_SRC_DIR)
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(C_SYS_SOURCES:.c=.o)))
vpath %.c $(SYSTEMS_DIR)/lib
vpath %.c $(STDLIB_DIR)
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/$(MOD)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(MOD_SRC_DIR)
vpath %.s $(SYSTEMS_DIR)/lib



$(TARGET): $(OBJECTS) Makefile $(LD_SCRIPT) $(BUILD_DIR)/$(MOD)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/$(MOD)/%.o: %.c Makefile | $(BUILD_DIR)/$(MOD)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/$(MOD)/%.o: %.s Makefile | $(BUILD_DIR)/$(MOD)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(MOD): $(BUILD_DIR)
	mkdir -p $@

$(BUILD_DIR):
	mkdir -p $@		


print-% : ; @echo $* = $($*)
#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)/$(MOD)
  
#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)