# Toolchain
AS = as
CXX = g++
LD = ld
QEMU = qemu-system-i386

# Compilation Flags (-fno-stack-protector disabled SSP for bare metal)
ASFLAGS = --32
CXXFLAGS = -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -Ikernel
LDFLAGS = -m elf_i386 -T kernel/linker.ld

# Object Files
KERNEL_OBJS = kernel/boot.o kernel/tty.o kernel/kernel.o kernel/task.o memory/pmm.o memory/vmm.o interrupts/idt.o interrupts/idt_asm.o drivers/keyboard.o
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

memory/pmm.o : memory/pmm.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

memory/vmm.o : memory/vmm.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

interrupts/idt.o : interrupts/idt.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
 
interrupts/idt_asm.o : interrupts/idt_asm.s
	$(AS) $(ASFLAGS) $< -o $@

drivers/keyboard.o : drivers/keyboard.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@



$(KERNEL_BIN): $(KERNEL_OBJS) $(MODULE_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS) $(MODULE_OBJS)
# Build and Launch QEMU
run: $(KERNEL_BIN) 
	$(QEMU) -kernel $(KERNEL_BIN) 

# Clean Build Files
clean:
	rm -f kernel/*.o memory/*.o modules/*.o modules/*/*.o interrupts/*.o drivers/*.o $(KERNEL_BIN)

.PHONY: all run clean
