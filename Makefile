# Toolchain
AS = as
CC = gcc
CXX = g++
LD = ld
QEMU = qemu-system-x86_64

# Compilation Flags (-fno-stack-protector disabled SSP for bare metal)
ASFLAGS = --64
CFLAGS = -m64 -ffreestanding -fno-exceptions -fno-stack-protector -mno-red-zone -Ikernel
CXXFLAGS = -m64 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -mno-red-zone -Ikernel
LDFLAGS = -m elf_x86_64 -T kernel/linker.ld

# Compiler Objects
COMPILER_OBJS = compiler/compiler_compat.o compiler/lexer.o compiler/parser.o compiler/sema.o compiler/codegen_x86.o compiler/compiler.o

# Object Files
KERNEL_OBJS = kernel/boot.o kernel/tty.o kernel/kernel.o kernel/task.o kernel/shell.o kernel/module_swap.o memory/heap.o memory/pmm.o memory/vmm.o interrupts/idt.o interrupts/idt_asm.o drivers/keyboard.o storage/spmm.o editor/editor.o $(COMPILER_OBJS)
MODULE_OBJS = $(patsubst %.cpp, %.o, $(wildcard modules/*.cpp modules/*/*.cpp))
KERNEL_BIN = kernel/kernel.bin

# Default Rule: Build kernel.bin
all: $(KERNEL_BIN)

kernel/boot.o: kernel/boot.s
	$(AS) $(ASFLAGS) $< -o $@

kernel/tty.o: kernel/tty.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel/kernel.o: kernel/kernel.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel/task.o: kernel/task.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel/shell.o: kernel/shell.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel/module_swap.o: kernel/module_swap.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

memory/pmm.o : memory/pmm.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

memory/vmm.o : memory/vmm.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

memory/heap.o: memory/heap.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

interrupts/idt.o : interrupts/idt.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
 
interrupts/idt_asm.o : interrupts/idt_asm.s
	$(AS) $(ASFLAGS) $< -o $@

drivers/keyboard.o : drivers/keyboard.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

storage/spmm.o : storage/spmm.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

editor/editor.o : editor/editor.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@



compiler/%.o: compiler/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(KERNEL_OBJS) $(MODULE_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS) $(MODULE_OBJS)
	objcopy -O elf32-i386 $@ $@
disk.img:
	qemu-img create -f raw disk.img 64M

# Build and Launch QEMU
run: $(KERNEL_BIN) disk.img
	$(QEMU) -kernel $(KERNEL_BIN) -drive file=disk.img,format=raw,index=0,media=disk

# Clean Build Files (keeps disk.img intact)
clean:
	rm -f kernel/*.o memory/*.o modules/*.o modules/*/*.o interrupts/*.o drivers/*.o storage/*.o compiler/*.o editor/*.o $(KERNEL_BIN)

.PHONY: all run clean
