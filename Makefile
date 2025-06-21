# Compiler and assembler settings
CC = gcc
ASM = nasm
LD = ld

# Flags
CC_FLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -c
ASM_FLAGS = -f elf32
LD_FLAGS = -m elf_i386 -T link.ld -nostdlib

# Directories
SRC = src
INCLUDE = include
OBJ = obj
ISO = iso
BOOT = $(ISO)/boot
GRUB = $(ISO)/boot/grub

# Source files
SRC_FILES = $(shell find $(SRC) -name '*.c')
ASM_FILES = $(shell find $(SRC) -name '*.asm')
OBJ_FILES = $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRC_FILES)) \
            $(patsubst $(SRC)/%.asm, $(OBJ)/%.o, $(ASM_FILES))

# Target
KERNEL = $(BOOT)/kernel.bin
ISO_IMAGE = IntrenOS.iso

.PHONY: all clean iso

all: $(KERNEL)

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) -I$(INCLUDE) $< -o $@

$(OBJ)/%.o: $(SRC)/%.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASM_FLAGS) $< -o $@

$(KERNEL): $(OBJ_FILES)
	@mkdir -p $(BOOT)
	$(LD) $(LD_FLAGS) $(OBJ_FILES) -o $@

iso: $(KERNEL)
	@mkdir -p $(GRUB)
	@cp grub.cfg $(GRUB)/
	grub-mkrescue -o $(ISO_IMAGE) $(ISO)

clean:
	rm -rf $(OBJ) $(ISO) $(ISO_IMAGE) 