# Bare-metal build of Willy the Worm.
#
#   make              build willy.elf (the kernel)
#   make run          boot it in QEMU/KVM
#   make iso          bootable GRUB CD/USB image (willy.iso)
#   make floppy       1.44 MB boot floppy (willy-floppy.img)
#   make efi          willy.efi, a UEFI application
#   make run-efi      boot willy.efi under OVMF
#
# ARCH=x86_64 (default) builds the 64-bit kernel, with UEFI support.
# ARCH=i386 builds a 32-bit one for a 386 or later with a 387 (the game uses
# floating point throughout); its outputs are willy-i386.elf, willy-i386.iso
# and willy-i386-floppy.img. It asks for 1024x768 at 8 bits per pixel first,
# which 1 MB VESA cards of the time can do, and needs g++-multilib.
#
# The game itself (../cpp/wopr_willy.cpp, highscores.cpp, miniz) is compiled
# unchanged against the normal libstdc++ headers; overrides/ stands in for
# SDL2 and the file streams, and runtime.cpp/stdhooks.cpp/rbtree.cpp supply
# the few library symbols those headers need. Nothing from libc/libstdc++
# is linked.

ARCH     ?= x86_64
CXX      ?= g++
CC       ?= gcc
GAME     := ../cpp
BUILD    ?= build/$(ARCH)

GRUB_MKIMAGE  ?= $(shell command -v grub2-mkimage || command -v grub-mkimage)
GRUB_MKRESCUE ?= $(shell command -v grub2-mkrescue || command -v grub-mkrescue)
GRUB_I386     ?= $(firstword $(wildcard /usr/lib/grub/i386-pc /usr/lib/grub2/i386-pc))
MKFS_FAT      ?= $(shell command -v mkfs.fat || command -v mkfs.vfat || echo /usr/sbin/mkfs.fat)

ifeq ($(ARCH),x86_64)
MFLAG    := -m64
SUFFIX   :=
BOOT_S   := boot.S
USB_OBJ  := usb
LDEMU    :=
# PIE=1 builds the position-independent kernel that willy.efi embeds.
ifeq ($(PIE),1)
CODEGEN  := -fpie
BOOTDEFS := -DEFI
else
CODEGEN  := -fno-pic -fno-pie -mcmodel=kernel
BOOTDEFS :=
endif
ARCHFLAGS := $(CODEGEN) -mno-red-zone -msse2
else ifeq ($(ARCH),i386)
MFLAG    := -m32
SUFFIX   := -i386
BOOT_S   := boot32.S
# No xHCI before PCI Express, and its driver's 2 MB DMA pool would be a
# quarter of a typical 386's memory.
USB_OBJ  := usb_none
LDEMU    := -m elf_i386
FBDEFS   := -DFB_W=1024 -DFB_H=768 -DFB_BPP=8
BOOTDEFS := -march=i386 -DCPU_386 $(FBDEFS)
ARCHFLAGS := -fno-pic -fno-pie -march=i386 -mtune=i486 -mfpmath=387 -mno-sse $(FBDEFS)
else
$(error ARCH must be x86_64 or i386)
endif

LIBGCC    := $(shell $(CXX) $(MFLAG) -print-libgcc-file-name)

BASE     := $(MFLAG) $(ARCHFLAGS) -O2 -fno-stack-protector -fno-asynchronous-unwind-tables \
            -fno-builtin-printf -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG
CXXFLAGS := $(BASE) -std=gnu++17 -fno-rtti -fno-threadsafe-statics -D_GLIBCXX_ASSERTIONS \
            -Ioverrides -I$(GAME) -I. -Wno-write-strings
# Interrupt handlers never touch SSE (the stubs only save integer registers).
IRQFLAGS := $(filter-out -msse2,$(CXXFLAGS)) -mgeneral-regs-only
CFLAGS   := $(BASE) -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_APIS -I$(GAME) -w
LDFLAGS  := $(LDEMU) -nostdlib -static -no-pie -T linker.ld -z max-page-size=0x1000 --no-warn-rwx-segments

KERNEL := willy$(SUFFIX).elf
ISO    := willy$(SUFFIX).iso
FLOPPY := willy$(SUFFIX)-floppy.img

GAME_CXX := wopr_willy highscores
GAME_C   := miniz miniz_tdef miniz_tinfl miniz_zip
PLATFORM := kernel render runtime stdhooks rbtree audio pci $(USB_OBJ) floppy fat12 storage

OBJS := $(BUILD)/boot.o $(BUILD)/irq.o \
        $(addprefix $(BUILD)/,$(addsuffix .o,$(PLATFORM))) \
        $(addprefix $(BUILD)/game/,$(addsuffix .o,$(GAME_CXX) $(GAME_C)))

HDRS := $(wildcard *.hpp) font.h dejavu20.h overrides/SDL2/SDL.h overrides/stream_shim.h \
        $(GAME)/wopr.h $(GAME)/wopr_render.h $(GAME)/highscores.h

all: $(KERNEL)

$(BUILD)/boot.o: $(BOOT_S)
	@mkdir -p $(BUILD)
	$(CXX) $(MFLAG) $(BOOTDEFS) -c $< -o $@

$(BUILD)/irq.o: irq.cpp hw.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(IRQFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/game/%.o: $(GAME)/%.cpp $(HDRS) $(wildcard $(GAME)/wopr_willy_assets*.h)
	@mkdir -p $(BUILD)/game
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/game/%.o: $(GAME)/%.c
	@mkdir -p $(BUILD)/game
	$(CC) $(CFLAGS) -c $< -o $@

ifeq ($(ARCH),x86_64)
# Link as ELF64, then wrap as ELF32 so QEMU's -kernel and GRUB's multiboot
# loader both accept it. The 32-bit entry stub switches to long mode.
$(KERNEL): $(OBJS) linker.ld
	ld $(LDFLAGS) $(OBJS) $(LIBGCC) -o $(BUILD)/willy64.elf
	objcopy -O elf32-i386 $(BUILD)/willy64.elf $@
else
# Already ELF32; the entry stub stays in 32-bit protected mode.
$(KERNEL): $(OBJS) linker.ld
	ld $(LDFLAGS) $(OBJS) $(LIBGCC) -o $@
endif

# ---------------------------------------------------------------- UEFI
# willy.efi = a small UEFI loader (gnu-efi) with the kernel embedded.
EFI_INC  ?= $(firstword $(wildcard /usr/include/efi))
EFI_CRT0 ?= $(firstword $(shell find /usr/lib /usr/lib64 \( -name 'crt0-efi-x86_64.o' -o -path '*gnuefi*x64*crt0.o' \) 2>/dev/null))
EFI_LDS  ?= $(firstword $(shell find /usr/lib /usr/lib64 \( -name 'elf_x86_64_efi.lds' -o -name 'elf_x64_efi.lds' \) 2>/dev/null))
EFI_LIBS ?= $(shell find /usr/lib /usr/lib64 \( -name libgnuefi.a -o -name libefi.a \) 2>/dev/null | sort -u)
EFI_BUILD := build/efi
EFI_CFLAGS := -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_BUILD) -fpic -ffreestanding -fno-stack-protector \
              -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -O2 -Wall

efi: willy.efi

# The kernel again, position-independent, built in its own directory.
$(EFI_BUILD)/kernel.elf: FORCE
	$(MAKE) --no-print-directory PIE=1 BUILD=$(EFI_BUILD)/k $(EFI_BUILD)/k/kernel-pie.elf
	cp $(EFI_BUILD)/k/kernel-pie.elf $@
FORCE:

$(BUILD)/kernel-pie.elf: $(OBJS) efi/kernel.ld
	ld -nostdlib -pie --no-dynamic-linker -z notext -z max-page-size=0x1000 --no-warn-rwx-segments \
	   -T efi/kernel.ld $(OBJS) $(LIBGCC) -o $@

$(EFI_BUILD)/kernel.bin $(EFI_BUILD)/kernel_layout.h: $(EFI_BUILD)/kernel.elf
	objcopy -O binary $< $(EFI_BUILD)/kernel.bin
	@nm $< | awk '/ __kernel_end$$/ {print "#define KERNEL_MEM_SIZE 0x" $$1} \
	              / efi_entry64$$/  {print "#define KERNEL_ENTRY 0x" $$1} \
	              / __rela_start$$/ {print "#define KERNEL_RELA_START 0x" $$1} \
	              / __rela_end$$/   {print "#define KERNEL_RELA_END 0x" $$1}' > $(EFI_BUILD)/kernel_layout.h
	@test $$(grep -c define $(EFI_BUILD)/kernel_layout.h) = 4 || { echo "kernel layout symbols missing"; exit 1; }

willy.efi: efi/loader.c efi/image.S $(EFI_BUILD)/kernel.bin $(EFI_BUILD)/kernel_layout.h
	@test -n "$(EFI_CRT0)" -a -n "$(EFI_LDS)" || { echo "gnu-efi not found (Debian: gnu-efi, Fedora: gnu-efi-devel)"; exit 1; }
	gcc $(EFI_CFLAGS) -c efi/loader.c -o $(EFI_BUILD)/loader.o
	gcc -c -DKERNEL_BIN='"$(EFI_BUILD)/kernel.bin"' efi/image.S -o $(EFI_BUILD)/image.o
	ld -nostdlib -znocombreloc -shared -Bsymbolic -T $(EFI_LDS) $(EFI_CRT0) \
	   $(EFI_BUILD)/loader.o $(EFI_BUILD)/image.o $(EFI_LIBS) $(EFI_LIBS) -o $(EFI_BUILD)/willy.so
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela \
	        -j '.rel.*' -j '.rela.*' -j .reloc -O efi-app-x86_64 --subsystem=10 $(EFI_BUILD)/willy.so $@

# ---------------------------------------------------------------- images
iso: $(ISO)
$(ISO): $(KERNEL) grub.cfg
	@rm -rf $(BUILD)/iso && mkdir -p $(BUILD)/iso/boot/grub
	cp $(KERNEL) $(BUILD)/iso/boot/willy.elf
	cp grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(BUILD)/iso 2>/dev/null

# A FAT12 floppy: GRUB's boot sector (keeping the FAT header) in sector 0,
# GRUB's core image in the reserved sectors after it, and the kernel
# (gzipped; GRUB unpacks it) and grub.cfg as files. The high-score table is
# kept in /willy on the same disk (storage.cpp). Needs mtools and dosfstools.
floppy: $(FLOPPY)
$(FLOPPY): $(KERNEL) grub.cfg
	$(GRUB_MKIMAGE) -O i386-pc -o $(BUILD)/core.img -p /boot/grub \
	    biosdisk fat gzio normal configfile multiboot vbe video video_fb boot
	gzip -9 -n -c $(KERNEL) > $(BUILD)/kernel.gz
	rm -f $@
	$(MKFS_FAT) -C -F 12 -f 2 -r 224 -R $$(( $$(stat -c %s $(BUILD)/core.img) / 512 + 2 )) -n WILLY $@ 1440 >/dev/null
	mmd -i $@ ::/boot ::/boot/grub ::/willy
	mcopy -i $@ $(BUILD)/kernel.gz ::/boot/willy.elf
	mcopy -i $@ grub.cfg ::/boot/grub/grub.cfg
	cp $(GRUB_I386)/boot.img $(BUILD)/boot.img
	dd if=$@ of=$(BUILD)/boot.img bs=1 skip=3 seek=3 count=59 conv=notrunc status=none
	dd if=$(BUILD)/boot.img of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(BUILD)/core.img of=$@ bs=512 seek=1 conv=notrunc status=none
	@echo "$@: GRUB $$(( $$(stat -c %s $(BUILD)/core.img) / 1024 )) KB + kernel $$(( $$(stat -c %s $(BUILD)/kernel.gz) / 1024 )) KB (gzipped); $$(( $$(mdir -i $@ :: | sed -n 's/ *\([0-9 ]*\) bytes free/\1/p' | tr -d ' ') / 1024 )) KB free"

# ---------------------------------------------------------------- QEMU
# SOUND=hda (default), ac97 or none. ARGS adds kernel options, e.g. ARGS=debug.
AUDIODEV ?= $(if $(filter Darwin,$(shell uname)),coreaudio,pa)
QSOUND_ac97 := -audiodev $(AUDIODEV),id=snd0 -device AC97,audiodev=snd0
QSOUND_hda  := -audiodev $(AUDIODEV),id=snd0 -device intel-hda -device hda-output,audiodev=snd0
QSOUND_none :=
QUSB_xhci   := -device qemu-xhci -device usb-kbd
QUSB_none   :=
SOUND    ?= hda
USB      ?= xhci
KVM      ?= $(if $(wildcard /dev/kvm),-enable-kvm,)
ifeq ($(ARCH),x86_64)
QEMU     := qemu-system-x86_64 $(KVM) -m 256 -serial stdio $(QSOUND_$(SOUND)) $(QUSB_$(USB))
else
# The oldest CPU QEMU models is a 486; 8 MB.
QEMU     := qemu-system-i386 -cpu 486 -m 8 -serial stdio $(QSOUND_$(SOUND))
endif

run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -append "audio=$(SOUND) $(ARGS)"

run-iso: $(ISO)
	$(QEMU) -cdrom $(ISO)

run-floppy: $(FLOPPY)
	$(QEMU) -fda $(FLOPPY) -boot a

OVMF ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd \
          /usr/share/edk2/ovmf/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.4m.fd /usr/share/qemu/ovmf-x86_64.bin))
OVMF_VARS ?= $(firstword $(wildcard $(subst CODE,VARS,$(OVMF))))
run-efi: willy.efi
	@test -n "$(OVMF)" -a -n "$(OVMF_VARS)" || { echo "OVMF firmware not found (Debian: ovmf, Fedora: edk2-ovmf); set OVMF= and OVMF_VARS="; exit 1; }
	@rm -rf $(BUILD)/esp && mkdir -p $(BUILD)/esp/EFI/BOOT
	cp willy.efi $(BUILD)/esp/EFI/BOOT/BOOTX64.EFI
	cp $(OVMF_VARS) $(BUILD)/ovmf_vars.fd
	$(QEMU) -drive if=pflash,format=raw,readonly=on,file=$(OVMF) \
	        -drive if=pflash,format=raw,file=$(BUILD)/ovmf_vars.fd -drive format=raw,file=fat:rw:$(BUILD)/esp

clean:
	rm -rf build willy.elf willy.iso willy-floppy.img willy.efi willy-i386.elf willy-i386.iso willy-i386-floppy.img

.PHONY: all iso floppy efi run run-iso run-floppy run-efi clean FORCE
