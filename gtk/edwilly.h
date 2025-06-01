#ifndef EDWILLY_H
#define EDWILLY_H

// Include the main willy.h header for all the core functionality
#include "willy.h"

// Editor-specific constants and structures
struct EditorGameOptions {
    std::string levels_file = "levels.json";
    int scale_factor = 3;
    bool show_help = false;
};

// Forward declarations
class WillyEditor;
class WillyEditorApplication;
class SpriteIterator;

// Function declarations
void print_editor_help(const char* program_name);
bool parse_editor_command_line(int argc, char* argv[]);

// Global editor options
extern EditorGameOptions editor_options;

// Background color globals (shared with main willy.cpp)
extern double redbg;
extern double greenbg;
extern double bluebg;

#endif // EDWILLY_H
