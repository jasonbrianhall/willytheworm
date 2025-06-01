# Makefile for Willy the Worm (GTK+ C Version) with Windows and Linux Support
# Based on Tetrimone Makefile structure

# Compiler settings
CC_LINUX = gcc
CC_WIN = x86_64-w64-mingw32-gcc
CFLAGS_COMMON = -std=c99 -Wall -Wextra -s

# Debug flags
DEBUG_FLAGS = -g -DDEBUG

# Source files (keeping .cpp extension for compatibility)
SRCS_COMMON = willy.cpp loadlevels.cpp sound.cpp highscores.cpp spriteloader.cpp willy_main.cpp
SRCS_EDITOR = edwilly.cpp loadlevels.cpp spriteloader.cpp willy.cpp sound.cpp highscores.cpp

# GTK flags for Linux
GTK_CFLAGS_LINUX := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS_LINUX := $(shell pkg-config --libs gtk+-3.0)

# GTK flags for Windows (MinGW)
GTK_CFLAGS_WIN := $(shell mingw64-pkg-config --cflags gtk+-3.0 2>/dev/null || echo "-I/usr/x86_64-w64-mingw32/include/gtk-3.0 -I/usr/x86_64-w64-mingw32/include/glib-2.0")
GTK_LIBS_WIN := $(shell mingw64-pkg-config --libs gtk+-3.0 2>/dev/null || echo "-lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0 -lcairo -lpango-1.0 -lpangocairo-1.0")

# SDL flags for Linux
SDL_CFLAGS_LINUX := $(shell pkg-config --cflags sdl2 SDL2_mixer)
SDL_LIBS_LINUX := $(shell pkg-config --libs sdl2 SDL2_mixer)

# SDL flags for Windows
SDL_CFLAGS_WIN := $(shell mingw64-pkg-config --cflags sdl2 SDL2_mixer 2>/dev/null || echo "-I/usr/x86_64-w64-mingw32/include/SDL2")
SDL_LIBS_WIN := $(shell mingw64-pkg-config --libs sdl2 SDL2_mixer 2>/dev/null || echo "-lSDL2 -lSDL2_mixer")

# Platform-specific settings
CFLAGS_LINUX = $(CFLAGS_COMMON) $(GTK_CFLAGS_LINUX) $(SDL_CFLAGS_LINUX) -DLINUX -pthread
CFLAGS_WIN = $(CFLAGS_COMMON) $(GTK_CFLAGS_WIN) $(SDL_CFLAGS_WIN) -DWIN32 -mwindows

# Editor-specific flags (includes SDL since it needs to run the game)
CFLAGS_EDITOR_LINUX = $(CFLAGS_COMMON) $(GTK_CFLAGS_LINUX) $(SDL_CFLAGS_LINUX) -DLINUX -DEDITOR_BUILD -pthread
CFLAGS_EDITOR_WIN = $(CFLAGS_COMMON) $(GTK_CFLAGS_WIN) $(SDL_CFLAGS_WIN) -DWIN32 -mwindows -DEDITOR_BUILD

# Debug-specific flags
CFLAGS_LINUX_DEBUG = $(CFLAGS_LINUX) $(DEBUG_FLAGS)
CFLAGS_WIN_DEBUG = $(CFLAGS_WIN) $(DEBUG_FLAGS)
CFLAGS_EDITOR_LINUX_DEBUG = $(CFLAGS_EDITOR_LINUX) $(DEBUG_FLAGS)
CFLAGS_EDITOR_WIN_DEBUG = $(CFLAGS_EDITOR_WIN) $(DEBUG_FLAGS)

# Linker flags
LDFLAGS_LINUX = $(GTK_LIBS_LINUX) $(SDL_LIBS_LINUX) -pthread -lm
LDFLAGS_EDITOR_LINUX = $(GTK_LIBS_LINUX) $(SDL_LIBS_LINUX) -pthread -lm

# Windows linker flags
LDFLAGS_WIN = $(GTK_LIBS_WIN) $(SDL_LIBS_WIN) -lwinmm -lm
LDFLAGS_EDITOR_WIN = $(GTK_LIBS_WIN) $(SDL_LIBS_WIN) -lwinmm -lm

# Object files
OBJS_LINUX = $(SRCS_COMMON:.cpp=.o)
OBJS_WIN = $(SRCS_COMMON:.cpp=.win.o)
OBJS_LINUX_DEBUG = $(SRCS_COMMON:.cpp=.debug.o)
OBJS_WIN_DEBUG = $(SRCS_COMMON:.cpp=.win.debug.o)

# Editor object files
OBJS_EDITOR_LINUX = $(SRCS_EDITOR:.cpp=.editor.o)
OBJS_EDITOR_WIN = $(SRCS_EDITOR:.cpp=.editor.win.o)
OBJS_EDITOR_LINUX_DEBUG = $(SRCS_EDITOR:.cpp=.editor.debug.o)
OBJS_EDITOR_WIN_DEBUG = $(SRCS_EDITOR:.cpp=.editor.win.debug.o)

# Target executables
TARGET_LINUX = willy
TARGET_WIN = willy.exe
TARGET_LINUX_DEBUG = willy_debug
TARGET_WIN_DEBUG = willy_debug.exe

# Editor target executables
TARGET_EDITOR_LINUX = edwilly
TARGET_EDITOR_WIN = edwilly.exe
TARGET_EDITOR_LINUX_DEBUG = edwilly_debug
TARGET_EDITOR_WIN_DEBUG = edwilly_debug.exe

# Build directories
BUILD_DIR = build
BUILD_DIR_LINUX = $(BUILD_DIR)/linux
BUILD_DIR_WIN = $(BUILD_DIR)/windows
BUILD_DIR_LINUX_DEBUG = $(BUILD_DIR)/linux_debug
BUILD_DIR_WIN_DEBUG = $(BUILD_DIR)/windows_debug

# Windows DLL settings
DLL_SOURCE_DIR = /usr/x86_64-w64-mingw32/sys-root/mingw/bin

# Data files
DATA_FILES = levels.json willy.chr

# Create necessary directories
$(shell mkdir -p $(BUILD_DIR_LINUX) $(BUILD_DIR_WIN) $(BUILD_DIR_LINUX_DEBUG) $(BUILD_DIR_WIN_DEBUG))

# Default target - build for Linux
.PHONY: all
all: linux

# OS-specific builds
.PHONY: windows
windows: willy-windows edwilly-windows

.PHONY: linux
linux: willy-linux edwilly-linux

# Debug targets
.PHONY: debug
debug: willy-linux-debug willy-windows-debug edwilly-linux-debug edwilly-windows-debug

.PHONY: linux-debug
linux-debug: willy-linux-debug edwilly-linux-debug

.PHONY: windows-debug
windows-debug: willy-windows-debug edwilly-windows-debug

#
# Linux build targets (Game)
#
.PHONY: willy-linux
willy-linux: $(BUILD_DIR_LINUX)/$(TARGET_LINUX) copy-data-linux

$(BUILD_DIR_LINUX)/$(TARGET_LINUX): $(addprefix $(BUILD_DIR_LINUX)/,$(OBJS_LINUX))
	@echo "Linking Linux executable..."
	$(CC_LINUX) $^ -o $@ $(LDFLAGS_LINUX)
	@echo "Linux build completed: $@"

# Generic compilation rules for Linux
$(BUILD_DIR_LINUX)/%.o: %.cpp
	@echo "Compiling $< for Linux..."
	$(CC_LINUX) $(CFLAGS_LINUX) -c $< -o $@

#
# Linux Editor build targets
#
.PHONY: edwilly-linux
edwilly-linux: $(BUILD_DIR_LINUX)/$(TARGET_EDITOR_LINUX) copy-data-linux

$(BUILD_DIR_LINUX)/$(TARGET_EDITOR_LINUX): $(addprefix $(BUILD_DIR_LINUX)/,$(OBJS_EDITOR_LINUX))
	@echo "Linking Linux editor executable..."
	$(CC_LINUX) $^ -o $@ $(LDFLAGS_EDITOR_LINUX)
	@echo "Linux editor build completed: $@"

# Generic compilation rules for Linux editor
$(BUILD_DIR_LINUX)/%.editor.o: %.cpp
	@echo "Compiling $< for Linux editor..."
	$(CC_LINUX) $(CFLAGS_EDITOR_LINUX) -c $< -o $@

#
# Linux debug targets (Game)
#
.PHONY: willy-linux-debug
willy-linux-debug: $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_DEBUG) copy-data-linux-debug

$(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_DEBUG): $(addprefix $(BUILD_DIR_LINUX_DEBUG)/,$(OBJS_LINUX_DEBUG))
	@echo "Linking Linux debug executable..."
	$(CC_LINUX) $^ -o $@ $(LDFLAGS_LINUX)
	@echo "Linux debug build completed: $@"

# Generic compilation rules for Linux debug
$(BUILD_DIR_LINUX_DEBUG)/%.debug.o: %.cpp
	@echo "Compiling $< for Linux debug..."
	$(CC_LINUX) $(CFLAGS_LINUX_DEBUG) -c $< -o $@

#
# Linux debug targets (Editor)
#
.PHONY: edwilly-linux-debug
edwilly-linux-debug: $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_EDITOR_LINUX_DEBUG) copy-data-linux-debug

$(BUILD_DIR_LINUX_DEBUG)/$(TARGET_EDITOR_LINUX_DEBUG): $(addprefix $(BUILD_DIR_LINUX_DEBUG)/,$(OBJS_EDITOR_LINUX_DEBUG))
	@echo "Linking Linux editor debug executable..."
	$(CC_LINUX) $^ -o $@ $(LDFLAGS_EDITOR_LINUX)
	@echo "Linux editor debug build completed: $@"

# Generic compilation rules for Linux editor debug
$(BUILD_DIR_LINUX_DEBUG)/%.editor.debug.o: %.cpp
	@echo "Compiling $< for Linux editor debug..."
	$(CC_LINUX) $(CFLAGS_EDITOR_LINUX_DEBUG) -c $< -o $@

#
# Windows build targets (Game)
#
.PHONY: willy-windows
willy-windows: $(BUILD_DIR_WIN)/$(TARGET_WIN) willy-collect-dlls copy-data-windows

$(BUILD_DIR_WIN)/$(TARGET_WIN): $(addprefix $(BUILD_DIR_WIN)/,$(OBJS_WIN))
	@echo "Linking Windows executable..."
	$(CC_WIN) $^ -o $@ $(LDFLAGS_WIN)
	@echo "Windows build completed: $@"

# Generic compilation rules for Windows
$(BUILD_DIR_WIN)/%.win.o: %.cpp
	@echo "Compiling $< for Windows..."
	$(CC_WIN) $(CFLAGS_WIN) -c $< -o $@

#
# Windows Editor build targets
#
.PHONY: edwilly-windows
edwilly-windows: $(BUILD_DIR_WIN)/$(TARGET_EDITOR_WIN) edwilly-collect-dlls copy-data-windows

$(BUILD_DIR_WIN)/$(TARGET_EDITOR_WIN): $(addprefix $(BUILD_DIR_WIN)/,$(OBJS_EDITOR_WIN))
	@echo "Linking Windows editor executable..."
	$(CC_WIN) $^ -o $@ $(LDFLAGS_EDITOR_WIN)
	@echo "Windows editor build completed: $@"

# Generic compilation rules for Windows editor
$(BUILD_DIR_WIN)/%.editor.win.o: %.cpp
	@echo "Compiling $< for Windows editor..."
	$(CC_WIN) $(CFLAGS_EDITOR_WIN) -c $< -o $@

#
# Windows debug targets (Game)
#
.PHONY: willy-windows-debug
willy-windows-debug: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG) willy-collect-debug-dlls copy-data-windows-debug

$(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG): $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJS_WIN_DEBUG))
	@echo "Linking Windows debug executable..."
	$(CC_WIN) $^ -o $@ $(LDFLAGS_WIN)
	@echo "Windows debug build completed: $@"

# Generic compilation rules for Windows debug
$(BUILD_DIR_WIN_DEBUG)/%.win.debug.o: %.cpp
	@echo "Compiling $< for Windows debug..."
	$(CC_WIN) $(CFLAGS_WIN_DEBUG) -c $< -o $@

#
# Windows debug targets (Editor)
#
.PHONY: edwilly-windows-debug
edwilly-windows-debug: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_EDITOR_WIN_DEBUG) edwilly-collect-debug-dlls copy-data-windows-debug

$(BUILD_DIR_WIN_DEBUG)/$(TARGET_EDITOR_WIN_DEBUG): $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJS_EDITOR_WIN_DEBUG))
	@echo "Linking Windows editor debug executable..."
	$(CC_WIN) $^ -o $@ $(LDFLAGS_EDITOR_WIN)
	@echo "Windows editor debug build completed: $@"

# Generic compilation rules for Windows editor debug
$(BUILD_DIR_WIN_DEBUG)/%.editor.win.debug.o: %.cpp
	@echo "Compiling $< for Windows editor debug..."
	$(CC_WIN) $(CFLAGS_EDITOR_WIN_DEBUG) -c $< -o $@

#
# DLL collection for Windows builds
#
.PHONY: willy-collect-dlls
willy-collect-dlls: $(BUILD_DIR_WIN)/$(TARGET_WIN)
	@echo "Collecting DLLs for Windows Willy..."
	@if [ -f $(BUILD_DIR_WIN)/collect_dlls.sh ]; then \
		cd $(BUILD_DIR_WIN) && ./collect_dlls.sh $(TARGET_WIN) $(DLL_SOURCE_DIR) .; \
	elif [ -f collect_dlls.sh ]; then \
		./collect_dlls.sh $(BUILD_DIR_WIN)/$(TARGET_WIN) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN); \
	else \
		echo "Warning: collect_dlls.sh not found in current directory or $(BUILD_DIR_WIN)/"; \
		echo "You may need to manually copy required DLLs."; \
		echo "Required DLLs typically include: libgcc_s_seh-1.dll, libglib-2.0-0.dll, libgtk-3-0.dll, libgdk-3-0.dll, libcairo-2.dll, libpango-1.0-0.dll, SDL2.dll, SDL2_mixer.dll"; \
	fi

.PHONY: edwilly-collect-dlls
edwilly-collect-dlls: $(BUILD_DIR_WIN)/$(TARGET_EDITOR_WIN)
	@echo "Collecting DLLs for Windows Editor..."
	@if [ -f $(BUILD_DIR_WIN)/collect_dlls.sh ]; then \
		cd $(BUILD_DIR_WIN) && ./collect_dlls.sh $(TARGET_EDITOR_WIN) $(DLL_SOURCE_DIR) .; \
	elif [ -f collect_dlls.sh ]; then \
		./collect_dlls.sh $(BUILD_DIR_WIN)/$(TARGET_EDITOR_WIN) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN); \
	else \
		echo "Warning: collect_dlls.sh not found in current directory or $(BUILD_DIR_WIN)/"; \
		echo "You may need to manually copy required DLLs."; \
		echo "Required DLLs now include: libgcc_s_seh-1.dll, libglib-2.0-0.dll, libgtk-3-0.dll, libgdk-3-0.dll, libcairo-2.dll, libpango-1.0-0.dll, SDL2.dll, SDL2_mixer.dll"; \
	fi

.PHONY: willy-collect-debug-dlls
willy-collect-debug-dlls: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG)
	@echo "Collecting Debug DLLs for Windows Willy..."
	@if [ -f $(BUILD_DIR_WIN_DEBUG)/collect_dlls.sh ]; then \
		cd $(BUILD_DIR_WIN_DEBUG) && ./collect_dlls.sh $(TARGET_WIN_DEBUG) $(DLL_SOURCE_DIR) .; \
	elif [ -f collect_dlls.sh ]; then \
		./collect_dlls.sh $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN_DEBUG); \
	else \
		echo "Warning: collect_dlls.sh not found in current directory or $(BUILD_DIR_WIN_DEBUG)/"; \
		echo "You may need to manually copy required DLLs."; \
	fi

.PHONY: edwilly-collect-debug-dlls
edwilly-collect-debug-dlls: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_EDITOR_WIN_DEBUG)
	@echo "Collecting Debug DLLs for Windows Editor..."
	@if [ -f $(BUILD_DIR_WIN_DEBUG)/collect_dlls.sh ]; then \
		cd $(BUILD_DIR_WIN_DEBUG) && ./collect_dlls.sh $(TARGET_EDITOR_WIN_DEBUG) $(DLL_SOURCE_DIR) .; \
	elif [ -f collect_dlls.sh ]; then \
		./collect_dlls.sh $(BUILD_DIR_WIN_DEBUG)/$(TARGET_EDITOR_WIN_DEBUG) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN_DEBUG); \
	else \
		echo "Warning: collect_dlls.sh not found in current directory or $(BUILD_DIR_WIN_DEBUG)/"; \
		echo "You may need to manually copy required DLLs."; \
	fi

#
# Data file copying
#
.PHONY: copy-data-linux
copy-data-linux:
	@echo "Copying data files to Linux build directory..."
	@for file in $(DATA_FILES); do \
		if [ -f $$file ]; then \
			cp $$file $(BUILD_DIR_LINUX)/; \
			echo "Copied $$file to $(BUILD_DIR_LINUX)/"; \
		else \
			echo "Warning: $$file not found"; \
		fi; \
	done
	@if [ -d audio ]; then \
		mkdir -p $(BUILD_DIR_LINUX)/audio/; \
		cp audio/* $(BUILD_DIR_LINUX)/audio/ -R -f; \
	fi

.PHONY: copy-data-linux-debug
copy-data-linux-debug:
	@echo "Copying data files to Linux debug build directory..."
	@for file in $(DATA_FILES); do \
		if [ -f $$file ]; then \
			cp $$file $(BUILD_DIR_LINUX_DEBUG)/; \
			echo "Copied $$file to $(BUILD_DIR_LINUX_DEBUG)/"; \
		else \
			echo "Warning: $$file not found"; \
		fi; \
	done
	@if [ -d audio ]; then \
		mkdir -p $(BUILD_DIR_LINUX_DEBUG)/audio/; \
		cp audio/* $(BUILD_DIR_LINUX_DEBUG)/audio/ -R -f; \
	fi

.PHONY: copy-data-windows
copy-data-windows:
	@echo "Copying data files to Windows build directory..."
	@for file in $(DATA_FILES); do \
		if [ -f $$file ]; then \
			cp $$file $(BUILD_DIR_WIN)/; \
			echo "Copied $$file to $(BUILD_DIR_WIN)/"; \
		else \
			echo "Warning: $$file not found"; \
		fi; \
	done
	@if [ -d audio ]; then \
		mkdir -p $(BUILD_DIR_WIN)/audio/; \
		cp audio/* $(BUILD_DIR_WIN)/audio/ -R -f; \
	fi

.PHONY: copy-data-windows-debug
copy-data-windows-debug:
	@echo "Copying data files to Windows debug build directory..."
	@for file in $(DATA_FILES); do \
		if [ -f $$file ]; then \
			cp $$file $(BUILD_DIR_WIN_DEBUG)/; \
			echo "Copied $$file to $(BUILD_DIR_WIN_DEBUG)/"; \
		else \
			echo "Warning: $$file not found"; \
		fi; \
	done
	@if [ -d audio ]; then \
		mkdir -p $(BUILD_DIR_WIN_DEBUG)/audio/; \
		cp audio/* $(BUILD_DIR_WIN_DEBUG)/audio/ -R -f; \
	fi

# Install targets (Linux only)
.PHONY: install
install: willy-linux edwilly-linux
	@echo "Installing Willy the Worm and Editor..."
	sudo cp $(BUILD_DIR_LINUX)/$(TARGET_LINUX) /usr/local/bin/
	sudo cp $(BUILD_DIR_LINUX)/$(TARGET_EDITOR_LINUX) /usr/local/bin/
	sudo mkdir -p /usr/local/share/willy/
	sudo cp $(DATA_FILES) /usr/local/share/willy/ 2>/dev/null || true
	@if [ -d audio ]; then \
		sudo cp -r audio /usr/local/share/willy/; \
	fi
	@echo "Installation completed. Run with: willy (game) or edwilly (editor)"

.PHONY: uninstall
uninstall:
	@echo "Uninstalling Willy the Worm and Editor..."
	sudo rm -f /usr/local/bin/$(TARGET_LINUX)
	sudo rm -f /usr/local/bin/$(TARGET_EDITOR_LINUX)
	sudo rm -rf /usr/local/share/willy/
	@echo "Uninstallation completed."

# Clean targets
.PHONY: clean
clean:
	@echo "Cleaning build files..."
	find $(BUILD_DIR) -type f -name "*.o" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.dll" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.exe" -delete 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX)/$(TARGET_LINUX) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX)/$(TARGET_EDITOR_LINUX) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_EDITOR_LINUX_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN)/$(TARGET_WIN) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN)/$(TARGET_EDITOR_WIN) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_DEBUG)/$(TARGET_EDITOR_WIN_DEBUG) 2>/dev/null || true
	@echo "Clean completed."

.PHONY: clean-all
clean-all: clean
	@echo "Removing entire build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Full clean completed."

# Test targets
.PHONY: test-linux
test-linux: willy-linux edwilly-linux
	@echo "Testing Linux builds..."
	cd $(BUILD_DIR_LINUX) && ./$(TARGET_LINUX) -h
	cd $(BUILD_DIR_LINUX) && ./$(TARGET_EDITOR_LINUX) -h

.PHONY: test-windows
test-windows: willy-windows edwilly-windows
	@echo "Testing Windows builds..."
	@echo "Run $(BUILD_DIR_WIN)/$(TARGET_WIN) and $(BUILD_DIR_WIN)/$(TARGET_EDITOR_WIN) manually on Windows system"

# Package targets
.PHONY: package-linux
package-linux: willy-linux edwilly-linux
	@echo "Creating Linux package..."
	tar -czf willy-linux.tar.gz -C $(BUILD_DIR_LINUX) .
	@echo "Linux package created: willy-linux.tar.gz"

.PHONY: package-windows
package-windows: willy-windows edwilly-windows
	@echo "Creating Windows package..."
	cd $(BUILD_DIR_WIN) && zip -r ../willy-windows.zip .
	@echo "Windows package created: $(BUILD_DIR)/willy-windows.zip"

.PHONY: package-all
package-all: package-linux package-windows

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make               - Build Willy game and editor for Linux (default)"
	@echo "  make linux         - Build Willy game and editor for Linux"
	@echo "  make windows       - Build Willy game and editor for Windows (requires MinGW)"
	@echo ""
	@echo "  make willy-linux   - Build only the game for Linux"
	@echo "  make edwilly-linux - Build only the editor for Linux"
	@echo "  make willy-windows - Build only the game for Windows"
	@echo "  make edwilly-windows - Build only the editor for Windows"
	@echo ""
	@echo "  make debug         - Build debug versions for both platforms"
	@echo "  make linux-debug   - Build Willy game and editor for Linux with debug symbols"
	@echo "  make windows-debug - Build Willy game and editor for Windows with debug symbols"
	@echo ""
	@echo "  make install       - Install Willy game and editor system-wide (Linux only)"
	@echo "  make uninstall     - Uninstall Willy from system (Linux only)"
	@echo ""
	@echo "  make test-linux    - Test Linux builds"
	@echo "  make test-windows  - Test Windows builds"
	@echo ""
	@echo "  make package-linux   - Create Linux distribution package"
	@echo "  make package-windows - Create Windows distribution package"
	@echo "  make package-all     - Create packages for both platforms"
	@echo ""
	@echo "  make clean         - Remove build files"
	@echo "  make clean-all     - Remove entire build directory"
	@echo "  make help          - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  Linux build:   gcc, pkg-config, libgtk-3-dev, libsdl2-dev, libsdl2-mixer-dev"
	@echo "  Windows build: mingw-w64-gcc, mingw64-pkg-config, mingw64 GTK3 and SDL2 libraries"
	@echo ""
	@echo "Note: This is the GTK+ (C) version, not gtkmm (C++)"
	@echo ""
	@echo "Editor vs Game:"
	@echo "  Game (willy):   Full game with sound, requires SDL2"
	@echo "  Editor (edwilly): Level editor with integrated game testing, also requires SDL2"
