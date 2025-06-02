#include "willy.h"
#include <cstring>
#include <getopt.h>
#include <unistd.h>

double redbg = 0.0;
double greenbg = 0.0;
double bluebg = 1.0;
GameOptions game_options;

// Ball implementation
Ball::Ball(int r, int c) : row(r), col(c), direction("") {}

WillyGame::WillyGame()
    : vbox(Gtk::ORIENTATION_VERTICAL), current_state(GameState::INTRO),
      scale_factor(game_options.scale_factor), // Use command line scale factor
      level(game_options.starting_level), // Use command line starting level
      score(0), lives(5), bonus(1000), willy_position({23, 7}),
      previous_willy_position({23, 7}), // Initialize previous position tracker
      willy_direction("RIGHT"), willy_velocity({0, 0}), jumping(false),
      fps(game_options.fps), // Use command line FPS
      frame_count(0), current_level("level1"), continuous_direction(""),
      moving_continuously(false), mouse_button_held(false), held_button(0),
      mouse_direction(""), mouse_up_held(false), mouse_down_held(false),
      up_pressed(false), down_pressed(false), life_adder(0), gen(rd()) {

  set_title("Willy the Worm - C++ GTK Edition");
  score_manager = std::make_unique<HighScoreManager>();
  sprite_loader = std::make_unique<SpriteLoader>(scale_factor);
  level_loader = std::make_unique<LevelLoader>();
  sound_manager = std::make_unique<SoundManager>();
  set_title("Willy the Worm - C++ GTK Edition");
  // After set_title(), add:
  set_decorated(true);
  set_skip_taskbar_hint(false);
  set_skip_pager_hint(false);
  property_window_position().set_value(Gtk::WIN_POS_CENTER);

  // Ensure the window manager shows normal window chrome
  if (get_window()) {
    get_window()->set_decorations(Gdk::DECOR_ALL);
    get_window()->set_functions(Gdk::FUNC_ALL);
  }
  // Explicitly enable window decorations
  set_decorated(true);
  set_deletable(true);
  set_resizable(true);

  // Set window type hint (helps on Windows)
  set_type_hint(Gdk::WINDOW_TYPE_HINT_NORMAL);
  // Initialize sound system
  if (!sound_manager->initialize()) {
    std::cout << "Warning: Sound system initialization failed" << std::endl;
  }

  // Apply command line sound setting
  sound_manager->set_sound_enabled(game_options.sound_enabled);

  // Load levels file from command line option
  if (!level_loader->load_levels(game_options.levels_file)) {
    std::cout << "Warning: Failed to load " << game_options.levels_file
              << ", trying default levels.json" << std::endl;
    level_loader->load_levels("levels.json");
  }

  setup_ui();
  current_state = GameState::INTRO;

  // Set up timer with command line FPS
  timer_connection = Glib::signal_timeout().connect(
      sigc::mem_fun(*this, &WillyGame::game_tick), 1000 / fps);

  show_all_children();

  // Store base game dimensions (before any scaling)
  base_game_width = GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor;
  base_game_height = (GAME_SCREEN_HEIGHT + 2) * GAME_CHAR_HEIGHT * scale_factor;

  // Calculate proper window size
  Gtk::Requisition menubar_min, menubar_nat;
  menubar.get_preferred_size(menubar_min, menubar_nat);

  Gtk::Requisition statusbar_min, statusbar_nat;
  status_bar.get_preferred_size(statusbar_min, statusbar_nat);

  int total_height =
      base_game_height + menubar_min.height + statusbar_min.height + 10;

  set_default_size(base_game_width, total_height);
  resize(base_game_width, total_height);

  // Connect resize signal
  signal_size_allocate().connect(
      sigc::hide(sigc::mem_fun(*this, &WillyGame::on_window_resize)));

  // Initial scaling calculation
  calculate_scaling_factors();

  drawing_area.grab_focus();

  // Print command line options being used
  std::cout << "Game initialized with options:" << std::endl;
  std::cout << "  Starting level: " << level << std::endl;
  std::cout << "  Levels file: " << game_options.levels_file << std::endl;
  std::cout << "  Number of balls: " << game_options.number_of_balls
            << std::endl;
  std::cout << "  FPS: " << fps << std::endl;
  std::cout << "  Scale factor: " << scale_factor << std::endl;
  std::cout << "  Sound enabled: "
            << (sound_manager->is_sound_enabled() ? "Yes" : "No") << std::endl;
  if (game_options.use_wasd)
    std::cout << "  WASD controls: Enabled" << std::endl;
  if (game_options.disable_flash)
    std::cout << "  Death flash: Disabled" << std::endl;
  if (game_options.mouse_support)
    std::cout << "  Mouse support: Enabled (not yet implemented)" << std::endl;
}
WillyGame::~WillyGame() { timer_connection.disconnect(); }

bool WillyGame::check_movement_collision(int old_row, int old_col, int new_row,
                                         int new_col) {
  // Don't check collisions if moving to/from ballpit
  std::string old_tile = get_tile(old_row, old_col);
  std::string new_tile = get_tile(new_row, new_col);
  if (old_tile == "BALLPIT" || new_tile == "BALLPIT") {
    return false; // No collision in ballpit areas
  }

  for (const auto &ball : balls) {
    // Skip balls that are in ballpits
    if (get_tile(ball.row, ball.col) == "BALLPIT") {
      continue;
    }

    // Only check collision if ball is at Willy's new position (same row AND
    // column)
    if (ball.row == new_row && ball.col == new_col) {
      return true; // Collision detected - ball and Willy at same position
    }

    // Check for crossing paths (ball and Willy swapping positions)
    // This is only relevant if they're both moving horizontally at the same
    // level
    if (ball.row == old_row && ball.col == old_col && ball.row == new_row &&
        ball.col == new_col &&
        old_row == new_row) { // Only check swapping if on same horizontal level
      return true; // Collision detected - crossing paths horizontally
    }
  }
  return false;
}

void WillyGame::start_game() {
  current_state = GameState::PLAYING;
  level = game_options.starting_level; // Use the configured starting level
  score = 0;
  lives = game_options.starting_lives; // Use the configured starting lives
  bonus = 1000;
  frame_count = 0;
  continuous_direction = "";
  moving_continuously = false;
  up_pressed = false;
  down_pressed = false;
  life_adder = 0;

  // Check if the specified starting level exists, fall back to level 1 if not
  std::string level_name = "level" + std::to_string(level);
  if (!level_loader->level_exists(level_name)) {
    std::cout << "Warning: Level " << level
              << " does not exist, starting at level 1" << std::endl;
    level = 1;
    level_name = "level1";
  }

  load_level(level_name);
  update_status_bar();
}

void WillyGame::jump() {
  int y = willy_position.first;
  int x = willy_position.second;

  // Get the current and below tiles
  std::string current_tile = get_tile(y, x);
  std::string below_tile = get_tile(y + 1, x);

  // Can jump if standing on "UPSPRING" or if below tile is a "PIPE"
  if (current_tile == "UPSPRING" || below_tile.substr(0, 4) == "PIPE" ||
      y == GAME_MAX_HEIGHT - 1) {
    jumping = true;

    // Apply a stronger jump if standing on "UPSPRING"
    willy_velocity.second = (current_tile == "UPSPRING") ? -6 : -5;

    sound_manager->play_sound("jump.mp3");
  }
}

std::string WillyGame::get_tile(int row, int col) {
  return level_loader->get_tile(current_level, row, col);
}

void WillyGame::set_tile(int row, int col, const std::string &tile) {
  level_loader->set_tile(current_level, row, col, tile);
}

bool WillyGame::can_move_to(int row, int col) {
  if (row < 0 || row >= GAME_SCREEN_HEIGHT || col < 0 ||
      col >= GAME_SCREEN_WIDTH) {
    return false;
  }

  std::string tile = get_tile(row, col);
  return (tile == "EMPTY" || tile == "LADDER" || tile == "PRESENT" ||
          tile == "BELL" || tile == "UPSPRING" || tile == "SIDESPRING" ||
          tile == "TACK" || tile == "BALLPIT" || tile == "WILLY_RIGHT" ||
          tile == "WILLY_LEFT"); // Added BALLPIT
}

bool WillyGame::is_on_solid_ground() {
  int y = willy_position.first;
  int x = willy_position.second;

  if (y >= GAME_MAX_HEIGHT - 1) {
    return true;
  }

  std::string current_tile = get_tile(y, x);
  std::string below_tile = get_tile(y + 1, x);

  if (current_tile == "LADDER") {
    return true;
  }

  return (below_tile.substr(0, 4) == "PIPE");
}

void WillyGame::update_willy_movement() {
  if (current_state != GameState::PLAYING)
    return;
  previous_willy_position = willy_position;
  // Store Willy's current position for collision checking
  int old_row = willy_position.first;
  int old_col = willy_position.second;

  std::string current_tile =
      get_tile(willy_position.first, willy_position.second);
  bool on_ladder = (current_tile == "LADDER");
  bool moved_on_ladder = false;

  if (up_pressed) {
    int target_row = willy_position.first - 1;
    if (target_row >= 0) {
      std::string above_tile = get_tile(target_row, willy_position.second);

      if (on_ladder && above_tile == "LADDER" &&
          can_move_to(target_row, willy_position.second)) {
        // Check for collision before moving
        if (!check_movement_collision(old_row, old_col, target_row,
                                      willy_position.second)) {
          willy_position.first--;
          willy_velocity.second = 0;
          moved_on_ladder = true;
          moving_continuously = false;
          continuous_direction = "";
          sound_manager->play_sound("ladder.mp3"); // Add ladder sound
        } else {
          die();
          return;
        }
      } else if (!on_ladder && above_tile == "LADDER" &&
                 can_move_to(target_row, willy_position.second)) {
        // Check for collision before moving
        if (!check_movement_collision(old_row, old_col, target_row,
                                      willy_position.second)) {
          willy_position.first--;
          willy_velocity.second = 0;
          moved_on_ladder = true;
          moving_continuously = false;
          continuous_direction = "";
          sound_manager->play_sound("ladder.mp3"); // Add ladder sound
        } else {
          die();
          return;
        }
      }
    }
  }

  if (down_pressed && !moved_on_ladder) {
    int target_row = willy_position.first + 1;
    if (target_row < GAME_SCREEN_HEIGHT) {
      std::string below_tile = get_tile(target_row, willy_position.second);

      if (on_ladder && can_move_to(target_row, willy_position.second)) {
        // Check for collision before moving
        if (!check_movement_collision(old_row, old_col, target_row,
                                      willy_position.second)) {
          willy_position.first++;
          willy_velocity.second = 0;
          moved_on_ladder = true;
          moving_continuously = false;
          continuous_direction = "";
          sound_manager->play_sound("ladder.mp3"); // Add ladder sound
        } else {
          die();
          return;
        }
      } else if (!on_ladder && below_tile == "LADDER" &&
                 can_move_to(target_row, willy_position.second)) {
        // Check for collision before moving
        if (!check_movement_collision(old_row, old_col, target_row,
                                      willy_position.second)) {
          willy_position.first++;
          willy_velocity.second = 0;
          moved_on_ladder = true;
          moving_continuously = false;
          continuous_direction = "";
          sound_manager->play_sound("ladder.mp3"); // Add ladder sound
        } else {
          die();
          return;
        }
      }
    }
  }

  if (!moved_on_ladder) {
    if (moving_continuously && !continuous_direction.empty()) {
      bool hit_obstacle = false;

      if (continuous_direction == "LEFT") {
        if (can_move_to(willy_position.first, willy_position.second - 1)) {
          // Check for collision before moving
          if (!check_movement_collision(old_row, old_col, willy_position.first,
                                        willy_position.second - 1)) {
            willy_position.second--;
          } else {
            die();
            return;
          }
        } else {
          hit_obstacle = true;
        }
      } else if (continuous_direction == "RIGHT") {
        if (can_move_to(willy_position.first, willy_position.second + 1)) {
          // Check for collision before moving
          if (!check_movement_collision(old_row, old_col, willy_position.first,
                                        willy_position.second + 1)) {
            willy_position.second++;
          } else {
            die();
            return;
          }
        } else {
          hit_obstacle = true;
        }
      }

      if (hit_obstacle) {
        moving_continuously = false;
        continuous_direction = "";
      }
    } else if (!moving_continuously) {
      if (keys_pressed.count("Left")) {
        willy_direction = "LEFT";
        if (can_move_to(willy_position.first, willy_position.second - 1)) {
          // Check for collision before moving
          if (!check_movement_collision(old_row, old_col, willy_position.first,
                                        willy_position.second - 1)) {
            willy_position.second--;
          } else {
            die();
            return;
          }
        }
      } else if (keys_pressed.count("Right")) {
        willy_direction = "RIGHT";
        if (can_move_to(willy_position.first, willy_position.second + 1)) {
          // Check for collision before moving
          if (!check_movement_collision(old_row, old_col, willy_position.first,
                                        willy_position.second + 1)) {
            willy_position.second++;
          } else {
            die();
            return;
          }
        }
      }
    }
  }

  current_tile = get_tile(willy_position.first, willy_position.second);
  on_ladder = (current_tile == "LADDER");

  if (!on_ladder) {
    if (!is_on_solid_ground()) {
      willy_velocity.second += 1;
    } else {
      if (willy_velocity.second > 0) {
        willy_velocity.second = 0;
        jumping = false;
      }
    }

    if (willy_velocity.second != 0) {
      int new_y = willy_position.first + (willy_velocity.second > 0 ? 1 : -1);
      if (can_move_to(new_y, willy_position.second)) {
        // Check for collision before moving due to gravity/jumping
        if (!check_movement_collision(willy_position.first,
                                      willy_position.second, new_y,
                                      willy_position.second)) {
          willy_position.first = new_y;
        } else {
          die();
          return;
        }
      }

      if (willy_velocity.second < 0) {
        willy_velocity.second++;
      }
    }
  } else {
    willy_velocity.second = 0;
    jumping = false;
  }
}

void WillyGame::update_balls() {
  if (current_state != GameState::PLAYING)
    return;

  static std::chrono::steady_clock::time_point last_ball_spawn =
      std::chrono::steady_clock::now();
  static std::uniform_int_distribution<int> delay_distribution(
      500, 2000); // Random delay between 500ms - 2000ms
  static std::mt19937 rng(std::random_device{}());

  // Get the primary ball pit position
  std::pair<int, int> primary_ball_pit_pos = find_ballpit_position();

  // Move any balls in non-primary ball pit positions to the primary ball pit
  for (auto &ball : balls) {
    if (get_tile(ball.row, ball.col) == "BALLPIT" &&
        (ball.row != primary_ball_pit_pos.first ||
         ball.col != primary_ball_pit_pos.second)) {
      ball.row = primary_ball_pit_pos.first;
      ball.col = primary_ball_pit_pos.second;
      ball.direction = ""; // Reset movement after relocation
    }
  }

  // Apply movement logic to all balls
  for (auto &ball : balls) {
    // Apply gravity to balls
    if (ball.row < GAME_MAX_HEIGHT - 1 &&
        get_tile(ball.row + 1, ball.col).substr(0, 4) != "PIPE") {
      ball.row++;
      ball.direction = "";
    } else {
      // Ball is on a platform, move horizontally
      if (ball.direction.empty()) {
        std::uniform_real_distribution<> dis(0.0, 1.0);
        ball.direction = (dis(rng) > 0.5) ? "RIGHT" : "LEFT";
      }

      if (ball.direction == "RIGHT") {
        if (ball.col + 1 < GAME_MAX_WIDTH &&
            get_tile(ball.row, ball.col + 1).substr(0, 4) != "PIPE") {
          ball.col++;
        } else {
          ball.direction = "LEFT";
        }
      } else { // LEFT
        if (ball.col - 1 >= 0 &&
            get_tile(ball.row, ball.col - 1).substr(0, 4) != "PIPE") {
          ball.col--;
        } else {
          ball.direction = "RIGHT";
        }
      }
    }
  }

  // Ensure the ball count stays within the limit and apply random spawn delay
  auto now = std::chrono::steady_clock::now();
  if (balls.size() < game_options.number_of_balls &&
      std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                            last_ball_spawn)
              .count() > delay_distribution(rng)) {
    balls.emplace_back(primary_ball_pit_pos.first, primary_ball_pit_pos.second);
    last_ball_spawn = now; // Reset spawn timer
  }
}

void WillyGame::check_collisions() {
  if (current_state != GameState::PLAYING)
    return;

  int y = willy_position.first;
  int x = willy_position.second;
  std::string current_tile = get_tile(y, x);

  // Check if Willy left a destroyable pipe (PIPE18) - destroy it after he
  // leaves
  int prev_y = previous_willy_position.first;
  int prev_x = previous_willy_position.second;

  // Only check if Willy actually moved
  if (prev_y != y || prev_x != x) {
    // Check if there's a destroyable pipe below where Willy was previously
    // standing
    if (prev_y + 1 < GAME_SCREEN_HEIGHT) {
      std::string below_previous_tile = get_tile(prev_y + 1, prev_x);
      if (below_previous_tile == "PIPE18") {
        // Destroy the pipe after Willy leaves it
        set_tile(prev_y + 1, prev_x, "EMPTY");

        // Optional: Play a destruction sound
        sound_manager->play_sound("present.mp3"); // Using existing sound

        // Optional: Add points for destroying the pipe
        score += 50;
      }
    }
  }

  // Check ball collisions (only die if at same horizontal level and same
  // column)
  for (const auto &ball : balls) {
    // Only check collision if Willy and ball are at the SAME row AND column
    // AND not in a ballpit
    if (ball.row == y && ball.col == x && current_tile != "BALLPIT") {
      sound_manager->play_sound("tack.mp3"); // Death sound
      die();
      return;
    }
  }

  // Check tile interactions
  if (current_tile == "TACK") {
    // sound_manager->play_sound("tack.mp3");
    die();
  } else if (current_tile == "BELL") {
    sound_manager->play_sound("bell.mp3");
    complete_level();
  } else if (current_tile == "PRESENT") {
    score += 100;
    sound_manager->play_sound("present.mp3");
    set_tile(y, x, "EMPTY");
  } else if (current_tile == "UPSPRING") {
    sound_manager->play_sound("jump.mp3");
    jump();
  } else if (current_tile == "SIDESPRING") {
    sound_manager->play_sound("jump.mp3");
    // Reverse continuous direction if moving continuously
    if (moving_continuously) {
      if (continuous_direction == "RIGHT") {
        continuous_direction = "LEFT";
        willy_direction = "LEFT";
      } else if (continuous_direction == "LEFT") {
        continuous_direction = "RIGHT";
        willy_direction = "RIGHT";
      }
    } else {
      // Just reverse direction without continuous movement
      willy_direction = (willy_direction == "RIGHT") ? "LEFT" : "RIGHT";
    }
  }

  // Check for jumping over balls (bonus points)
  if (jumping || willy_velocity.second != 0) {
    for (int i = 1; i < 5; i++) {
      int check_y = y + i;
      if (check_y < GAME_SCREEN_HEIGHT) {
        for (const auto &ball : balls) {
          if (ball.row == check_y && ball.col == x) {
            score += 20;
            sound_manager->play_sound("boop.mp3");
            break;
          }
        }
      }
    }
  }
}

void WillyGame::die() {
  // Play death sound
  sound_manager->play_sound("tack.mp3");

  // Flash the screen
  if (!game_options.disable_flash) {
    flash_death_screen();
  }

  lives--;
  if (lives <= 0) {
    game_over();
  } else {
    reset_level();
  }
}

void WillyGame::complete_level_nobonus() {
  level++;
  continuous_direction = "";
  moving_continuously = false;
  willy_direction = "LEFT";

  // Try to load next level
  std::string next_level = "level" + std::to_string(level);
  if (level_loader->level_exists(next_level)) {
    load_level(next_level);
  } else {
    // No more levels, restart from level 1 with increased difficulty
    level = 1;
    load_level("level1");
  }

  // Reset game state for new level
  bonus = 1000;
  frame_count = 0;
}

void WillyGame::complete_level() {
  score += bonus;
  level++;
  continuous_direction = "";
  moving_continuously = false;
  willy_direction = "LEFT";

  // Try to load next level
  std::string next_level = "level" + std::to_string(level);
  if (level_loader->level_exists(next_level)) {
    load_level(next_level);
  } else {
    // No more levels, restart from level 1 with increased difficulty
    level = 1;
    load_level("level1");
  }

  // Reset game state for new level
  bonus = 1000;
  frame_count = 0;
}

void WillyGame::reset_level() {
  level_loader->reset_levels();
  load_level(current_level);

  willy_velocity = {0, 0};
  jumping = false;
  bonus = 1000;
  frame_count = 0;
  continuous_direction = "";
  moving_continuously = false;
  up_pressed = false;
  down_pressed = false;
}

void WillyGame::game_over() {
  if (score_manager->is_high_score(score)) {
    current_state = GameState::HIGH_SCORE_ENTRY;
    name_input = "";
  } else {
    current_state = GameState::HIGH_SCORE_DISPLAY;
  }
}

void WillyGame::new_game() { current_state = GameState::INTRO; }

void WillyGame::quit_game() { hide(); }

void WillyGame::update_status_bar() {
  if (current_state == GameState::PLAYING) {
    std::string status_text =
        "SCORE: " + std::to_string(score) +
        "    BONUS: " + std::to_string(bonus) +
        "    Level: " + std::to_string(level) +
        "    Willy the Worms Left: " + std::to_string(lives);
    status_bar.set_text(status_text);
  } else {
    status_bar.set_text("Willy the Worm - C++ GTK Edition");
  }
}

bool WillyGame::game_tick() {
  if (current_state == GameState::PLAYING) {
    update_willy_movement();
    update_balls();
    check_collisions();

    // Update bonus/timer
    frame_count++;
    if (frame_count >= fps) {
      frame_count = 0;
      int old_bonus = bonus;
      bonus = std::max(0, bonus - 10);

      if ((score / GAME_NEWLIFEPOINTS) > life_adder) {
        lives++;
        life_adder++;
        std::cout << "Extra life awarded! Score: " << score
                  << ", Lives: " << lives << std::endl;

        // Play a sound for extra life
        sound_manager->play_sound(
            "bell.mp3"); // Or create a special extra life sound
      }

      // Check for time warnings
      if ((bonus <= 100 && old_bonus > 100) ||
          (bonus <= 50 && old_bonus > 50)) {
        std::cout << "Warning: Time running low!" << std::endl;
        sound_manager->play_sound("bell.mp3"); // Warning sound
      }

      // Check if timer ran out - Willy dies!
      if (bonus <= 0) {
        std::cout << "Time's up! Bonus reached zero - Willy dies!" << std::endl;
        sound_manager->play_sound("tack.mp3"); // Death sound
        die();                                 // Kill Willy when timer expires
        return true; // Exit early since we're now in death/reset state
      }
    }
    update_status_bar();
  }
  drawing_area.queue_draw();

  return true; // Continue the timer
}

void WillyApplication::on_activate() {
  auto window = new WillyGame();
  add_window(*window);
  window->present();
}

// Function to run the game with specific options (called from editor)
int run_willy_game(const GameOptions &options) {
  // Set the global game_options to the provided options
  game_options = options;

  // Print startup information
  std::cout << "Willy the Worm - C++ GTK Edition\n";
  std::cout << "Starting level: " << game_options.starting_level << "\n";
  std::cout << "Levels file: " << game_options.levels_file << "\n";
  std::cout << "Number of balls: " << game_options.number_of_balls << "\n";
  std::cout << "FPS: " << game_options.fps << "\n";
  std::cout << "Scale factor: " << game_options.scale_factor << "\n";
  if (game_options.use_wasd)
    std::cout << "Using WASD controls\n";
  if (game_options.disable_flash)
    std::cout << "Death flash disabled\n";
  if (game_options.mouse_support)
    std::cout << "Mouse support enabled\n";
  if (!game_options.sound_enabled)
    std::cout << "Sound disabled\n";
  std::cout << "\n";

  // Create a new argc/argv with only the program name for GTK
  int gtk_argc = 1;
  char program_name[] = "willy";
  char *gtk_argv[] = {program_name, nullptr};

  auto app = WillyApplication::create();
  return app->run(gtk_argc, gtk_argv);
}
