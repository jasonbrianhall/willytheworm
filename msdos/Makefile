# Willy the Worm for MS-DOS / FreeDOS (DJGPP, 32-bit DPMI)
#
#   make            willy.exe with CWSDPMI embedded (runs on its own)
#   make msdos_original
#                   willy.exe that needs CWSDPMI.EXE next to it (or any
#                   other DPMI host: Windows 9x, HDPMI32, ...)
#   make run        run it in DOSBox
#   make clean
#
# The compiler: a local DJGPP cross compiler (i586-pc-msdosdjgpp-g++ on the
# PATH) when there is one, otherwise the djfdyuruiry/djgpp Docker image.
# Force either with DOCKER=1 / DOCKER=0.
#
# The game (../cpp/wopr_willy.cpp, highscores.cpp, miniz) is compiled
# unchanged, drawing through ../baremetal/render.cpp with the bare-metal
# build's SDL2 stand-in; dos_main.cpp replaces the bare-metal kernel
# (VESA/VGA video, BIOS keyboard, PC speaker on a sped-up IRQ0).
#
# Needs a 386 with a 387 (or EMU387.DXE from DJGPP v2misc next to it).
# WILLY /NOSOUND, WILLY /VGA; SET WILLYVID=800x600x16 picks a VESA mode.
GAME    := ../cpp
BM      := ../baremetal

DOS_TARGET      = willy.exe
DOS_COFF        = willy
COFF_TARGET     = willy.coff
EMBEDDED_TARGET = embedded.exe

DJGPP_IMAGE = djfdyuruiry/djgpp
CSDPMI_URL  = http://na.mirror.garr.it/mirrors/djgpp/current/v2misc/csdpmi7b.zip

# The container writes into this folder, so it runs as the folder's owner
# (you, normally; root if the folder was created with sudo).
USER_ID  = $(shell stat -c %u .)
GROUP_ID = $(shell stat -c %g .)

CROSS ?= i586-pc-msdosdjgpp-
ifeq ($(origin DOCKER),undefined)
DOCKER := $(if $(shell command -v $(CROSS)g++ 2>/dev/null),0,1)
endif

ifeq ($(DOCKER),1)
# The whole repository is mounted, so ../cpp and ../baremetal are reachable.
RUN      = docker run --rm -v $(abspath ..):/src:z -w /src/msdos -u $(USER_ID):$(GROUP_ID) $(DJGPP_IMAGE) /bin/sh -c
CXX_DOS  = g++
CC_DOS   = gcc
EXE2COFF = exe2coff
else
RUN      = /bin/sh -c
CXX_DOS  = $(CROSS)g++
CC_DOS   = $(CROSS)gcc
EXE2COFF = $(shell command -v $(CROSS)exe2coff 2>/dev/null || echo $(dir $(shell command -v $(CROSS)g++))../i586-pc-msdosdjgpp/bin/exe2coff)
endif

ARCH     = -O2 -march=i386 -mtune=i586
CXXFLAGS = $(ARCH) -std=gnu++17 -w -Iinclude -I$(BM) -I$(GAME) -DMSDOS
CFLAGS   = $(ARCH) -w -I$(GAME) -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_APIS
LDFLAGS  = -s -Wl,--gc-sections

CXX_SRCS = dos_main.cpp $(BM)/render.cpp $(GAME)/wopr_willy.cpp $(GAME)/highscores.cpp
C_SRCS   = $(addprefix $(GAME)/,miniz.c miniz_tdef.c miniz_tinfl.c miniz_zip.c)
HDRS     = $(wildcard $(GAME)/*.h) $(BM)/video.hpp $(BM)/font.h $(BM)/dejavu20.h \
           $(BM)/overrides/SDL2/SDL.h include/SDL2/SDL.h

BUILD_CMD = mkdir -p build && \
	for f in $(C_SRCS); do $(CC_DOS) $(CFLAGS) -c \$$f -o build/\$$(basename \$$f .c).o || exit 1; done && \
	$(CXX_DOS) $(CXXFLAGS) $(CXX_SRCS) build/miniz*.o $(LDFLAGS) -o $(DOS_TARGET)

all: msdos_embedded

pull-djgpp:
ifeq ($(DOCKER),1)
	docker pull $(DJGPP_IMAGE)
endif

get-csdpmi:
	@if [ ! -d "csdpmi" ]; then \
		wget $(CSDPMI_URL); \
		unzip -o csdpmi7b.zip -d csdpmi; \
	fi

$(COFF_TARGET): $(CXX_SRCS) $(C_SRCS) $(HDRS) | pull-djgpp
	$(RUN) "$(BUILD_CMD) && $(EXE2COFF) $(DOS_TARGET) && mv $(DOS_COFF) $(COFF_TARGET)"

# Needs CWSDPMI.EXE (or another DPMI host) at run time.
msdos_original: $(COFF_TARGET) get-csdpmi
	$(RUN) "$(BUILD_CMD)"
	cp csdpmi/bin/CWSDPMI.EXE .

# CWSDPMI's stub in front: one file that runs on plain DOS.
msdos_embedded: $(COFF_TARGET) get-csdpmi
	@if [ ! -f "csdpmi/bin/CWSDSTUB.EXE" ]; then \
		echo "ERROR: CWSDSTUB.EXE not found in csdpmi/bin directory!"; \
		exit 1; \
	fi
	cat csdpmi/bin/CWSDSTUB.EXE $(COFF_TARGET) > $(EMBEDDED_TARGET)
	mv $(EMBEDDED_TARGET) $(DOS_TARGET)
	chmod +x $(DOS_TARGET)
	@echo "Created $(DOS_TARGET) with embedded DPMI extender"

msdos: msdos_embedded

run: msdos_embedded
	@mkdir -p build/dosc
	cp $(DOS_TARGET) build/dosc/
	printf '[cpu]\ncore=dynamic\ncputype=auto\ncycles=max\n[dosbox]\nmachine=svga_s3\nmemsize=32\n[speaker]\npcspeaker=true\n' > build/dosbox.conf
	dosbox -conf build/dosbox.conf -c "mount c build/dosc" -c "c:" -c "$(DOS_TARGET)"

clean:
	rm -rf build $(DOS_TARGET) $(COFF_TARGET) $(EMBEDDED_TARGET) $(DOS_COFF) CWSDPMI.EXE || true
	rm -rf csdpmi csdpmi7b.zip csdpmi7b.zip.* || true

.PHONY: all pull-djgpp get-csdpmi msdos msdos_original msdos_embedded run clean
