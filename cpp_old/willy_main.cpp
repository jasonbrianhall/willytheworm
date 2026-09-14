#include "willy.h"
#include <getopt.h>

extern GameOptions game_options;

void print_help(const char *program_name) {
  std::cout << "Willy the Worm - C++ GTK Edition\n\n";
  std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
  std::cout << "Options:\n";
  std::cout << "  -l LEVEL          Start at specific level (default: 1)\n";
  std::cout
      << "  -L LEVELFILE      Use custom levels file (default: levels.json)\n";
  std::cout << "  -b BALLS          Set number of balls (default: 6)\n";
  std::cout << "  -w                Use WASD keyboard controls instead of "
               "arrow keys\n";
  std::cout << "  -f                Disable death flash effect\n";
  std::cout << "  -F FPS            Set frames per second (default: 10)\n";
  std::cout << "  -m                Enable mouse support\n";
  std::cout << "  -s                Start with sound disabled\n";
  std::cout << "  -S SCALE          Set scale factor (default: 3)\n";
  std::cout << "  -h, --help        Show this help message\n\n";
  std::cout << "Controls:\n";
  std::cout << "  Arrow Keys        Move Willy (or WASD with -w option)\n";
  std::cout << "  Space             Jump\n";
  std::cout << "  Mouse (with -m):  Hold mouse button relative to Willy:\n";
  std::cout << "    - Hold above    Keep moving up (climb ladder)\n";
  std::cout << "    - Hold below    Keep moving down (climb ladder)\n";
  std::cout << "    - Hold left     Keep moving left\n";
  std::cout << "    - Hold right    Keep moving right\n";
  std::cout << "  Right Click       Jump (with -m option)\n";
  std::cout << "  Middle Click      Stop Willy (with -m option)\n";
  std::cout << "  Ctrl+L            Skip level\n";
  std::cout << "  Ctrl+S            Toggle sound\n";
  std::cout << "  F5/F6/F7          Change background colors\n";
  std::cout << "  F11               Toggle fullscreen\n";
  std::cout << "  Escape            Quit game\n\n";
}

bool parse_command_line(int argc, char *argv[]) {
  static struct option long_options[] = {
      {"help", no_argument, nullptr, 'h'},
      {"level", required_argument, nullptr, 'l'},
      {"levels-file", required_argument, nullptr, 'L'},
      {"balls", required_argument, nullptr, 'b'},
      {"wasd", no_argument, nullptr, 'w'},
      {"no-flash", no_argument, nullptr, 'f'},
      {"fps", required_argument, nullptr, 'F'},
      {"mouse", no_argument, nullptr, 'm'},
      {"no-sound", no_argument, nullptr, 's'},
      {"scale", required_argument, nullptr, 'S'},
      {nullptr, 0, nullptr, 0}};

  int option_index = 0;
  int c;

  while ((c = getopt_long(argc, argv, "hl:L:b:wfF:msS:", long_options,
                          &option_index)) != -1) {
    printf("Option was %c\n", c);
    switch (c) {
    case 'h':
      game_options.show_help = true;
      return true;

    case 'l':
    case 'b':
    case 'F':
    case 'S': {
      try {
        int value = std::stoi(optarg);
        int min = (c == 'l') ? 1 : (c == 'b') ? 1 : (c == 'F') ? 1 : 1;
        int max = (c == 'l') ? 999 : (c == 'b') ? 20 : (c == 'F') ? 120 : 10;

        if (value < min || value > max) {
          std::cerr << "Error: "
                    << ((c == 'l')   ? "Level"
                        : (c == 'b') ? "Number of balls"
                        : (c == 'F') ? "FPS"
                                     : "Scale factor")
                    << " must be between " << min << " and " << max << "\n";
          return false;
        }

        if (c == 'l')
          game_options.starting_level = value;
        else if (c == 'b')
          game_options.number_of_balls = value;
        else if (c == 'F')
          game_options.fps = value;
        else if (c == 'S')
          game_options.scale_factor = value;
      } catch (const std::exception &) {
        std::cerr << "Error: Invalid value for "
                  << ((c == 'l')   ? "Level"
                      : (c == 'b') ? "Number of balls"
                      : (c == 'F') ? "FPS"
                                   : "Scale factor")
                  << ": " << optarg << "\n";
        return false;
      }
      break;
    }

    case 'L':
      game_options.levels_file = optarg;
      printf("Here\n");
      break;

    case 'w':
      game_options.use_wasd = true;
      break;

    case 'f':
      game_options.disable_flash = true;
      break;

    case 'm':
      game_options.mouse_support = true;
      break;

    case 's':
      game_options.sound_enabled = false;
      break;

    case '?':
      return false; // getopt_long already prints error messages

    default:
      std::cerr << "Error: Unknown option encountered.\n";
      return false;
    }
  }

  // Check for unexpected arguments
  if (optind < argc) {
    std::cerr << "Error: Unexpected argument: " << argv[optind] << "\n";
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  // Parse command line arguments BEFORE creating GTK app
  if (!parse_command_line(argc, argv)) {
    print_help(argv[0]);
    return 1;
  }

  if (game_options.show_help) {
    print_help(argv[0]);
    return 0;
  }

  // Run the game with the parsed options
  return run_willy_game(game_options);
}
