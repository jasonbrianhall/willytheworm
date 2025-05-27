#include "willy.h"
#include <getopt.h>
#include <unistd.h>
#include <cstring>

double redbg=0.0;
double greenbg=0.0;
double bluebg=1.0;
GameOptions game_options;

// Ball implementation
Ball::Ball(int r, int c) : row(r), col(c), direction("") {}

// SpriteLoader implementation
SpriteLoader::SpriteLoader(int scale) : scale_factor(scale) {
    // Initialize sprite name mapping
    named_parts["0"] = "WILLY_RIGHT";
    named_parts["1"] = "WILLY_LEFT";
    named_parts["2"] = "PRESENT";
    named_parts["3"] = "LADDER";
    named_parts["4"] = "TACK";
    named_parts["5"] = "UPSPRING";
    named_parts["6"] = "SIDESPRING";
    named_parts["7"] = "BALL";
    named_parts["8"] = "BELL";
    
    // Add pipe parts 51-90
    for(int i = 51; i <= 90; i++) {
        named_parts[std::to_string(i)] = "PIPE" + std::to_string(i - 50);
    }
    named_parts["126"] = "BALLPIT";
    named_parts["127"] = "EMPTY";
    
    load_sprites();
}

std::string SpriteLoader::find_chr_file() {
    std::vector<std::string> possible_paths = {
        "willy.chr",
        "data/willy.chr",
        "/usr/games/willytheworm/data/willy.chr"
    };
    
    for(const auto& path : possible_paths) {
        std::ifstream file(path, std::ios::binary);
        if(file.good()) {
            return path;
        }
    }
    return "";
}

void SpriteLoader::load_sprites() {
    std::string chr_path = find_chr_file();
    
    if(!chr_path.empty()) {
        try {
            load_chr_file(chr_path);
            std::cout << "Loaded sprites from " << chr_path << std::endl;
            return;
        } catch(const std::exception& e) {
            std::cout << "Error loading .chr file: " << e.what() << std::endl;
        }
    }
    
    std::cout << "Creating fallback sprites..." << std::endl;
    create_fallback_sprites();
}

void SpriteLoader::load_chr_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if(!file) {
        throw std::runtime_error("Cannot open file");
    }
    
    // Read as 8x8 bitmap format
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(file_size);
    file.read(reinterpret_cast<char*>(data.data()), file_size);
    
    load_old_format(data);
}

void SpriteLoader::load_old_format(const std::vector<uint8_t>& data) {
    int num_chars = data.size() / 8;
    
    for(int i = 0; i < num_chars; i++) {
        auto it = named_parts.find(std::to_string(i));
        if(it != named_parts.end()) {
            try {
                auto surface = create_sprite_from_bitmap(data, i);
                sprites[it->second] = surface;
            } catch(const std::exception& e) {
                std::cout << "Error creating sprite " << i << ": " << e.what() << std::endl;
            }
        }
    }
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_sprite_from_bitmap(
    const std::vector<uint8_t>& data, int char_index) {
    
    int size = GAME_CHAR_WIDTH * scale_factor;
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto ctx = Cairo::Context::create(surface);
    
    // Clear to transparent
    ctx->set_operator(Cairo::OPERATOR_CLEAR);
    ctx->paint();
    ctx->set_operator(Cairo::OPERATOR_OVER);
    
    // Extract bits and draw pixels
    for(int row = 0; row < 8; row++) {
        uint8_t byte = data[char_index * 8 + row];
        for(int col = 0; col < 8; col++) {
            int bit = (byte >> (7 - col)) & 1;
            if(bit) {
                ctx->set_source_rgba(1.0, 1.0, 1.0, 1.0); // White pixel
                ctx->rectangle(col * scale_factor, row * scale_factor, 
                             scale_factor, scale_factor);
                ctx->fill();
            }
        }
    }
    
    return surface;
}

void SpriteLoader::create_fallback_sprites() {
    sprites["WILLY_RIGHT"] = create_willy_sprite(true);
    sprites["WILLY_LEFT"] = create_willy_sprite(false);
    sprites["PIPE1"] = create_colored_rect(0.5, 0.5, 0.5);
    sprites["LADDER"] = create_ladder_sprite();
    sprites["PRESENT"] = create_colored_rect(1.0, 0.0, 1.0);
    sprites["BALL"] = create_ball_sprite();
    sprites["BELL"] = create_bell_sprite();
    sprites["TACK"] = create_tack_sprite();
    sprites["UPSPRING"] = create_spring_sprite(true);
    sprites["SIDESPRING"] = create_spring_sprite(false);
    sprites["BALLPIT"] = create_colored_rect(0.3, 0.3, 0.3);
    sprites["EMPTY"] = create_empty_sprite();
    
    // Add all pipe variations
    for(int i = 1; i <= 40; i++) {
        sprites["PIPE" + std::to_string(i)] = create_colored_rect(0.5, 0.5, 0.5);
    }
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_willy_sprite(bool facing_right) {
    int size = GAME_CHAR_WIDTH * scale_factor;
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto ctx = Cairo::Context::create(surface);
    
    // Yellow body
    ctx->set_source_rgb(1.0, 1.0, 0.0);
    ctx->rectangle(0, 0, size, size);
    ctx->fill();
    
    // Eyes
    ctx->set_source_rgb(0.0, 0.0, 0.0);
    int eye_size = std::max(1, size / 8);
    if(facing_right) {
        ctx->rectangle(size * 0.25, size * 0.25, eye_size, eye_size);
        ctx->fill();
        ctx->rectangle(size * 0.625, size * 0.25, eye_size, eye_size);
        ctx->fill();
    } else {
        ctx->rectangle(size * 0.125, size * 0.25, eye_size, eye_size);
        ctx->fill();
        ctx->rectangle(size * 0.5, size * 0.25, eye_size, eye_size);
        ctx->fill();
    }
    
    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_colored_rect(double r, double g, double b) {
    int size = GAME_CHAR_WIDTH * scale_factor;
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto ctx = Cairo::Context::create(surface);
    
    ctx->set_source_rgb(r, g, b);
    ctx->rectangle(0, 0, size, size);
    ctx->fill();
    
    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_ladder_sprite() {
    int size = GAME_CHAR_WIDTH * scale_factor;
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto ctx = Cairo::Context::create(surface);
    
    // Brown color
    ctx->set_source_rgb(0.6, 0.3, 0.0);
    
    // Side rails
    int rail_width = std::max(1, size / 4);
    ctx->rectangle(0, 0, rail_width, size);
    ctx->fill();
    ctx->rectangle(size - rail_width, 0, rail_width, size);
    ctx->fill();
    
    // Rungs
    int rung_height = std::max(1, size / 8);
    for(int i = 0; i < size; i += size / 4) {
        ctx->rectangle(0, i, size, rung_height);
        ctx->fill();
    }
    
    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_ball_sprite() {
    int size = GAME_CHAR_WIDTH * scale_factor;
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto ctx = Cairo::Context::create(surface);
    
    ctx->set_source_rgb(1.0, 0.0, 0.0); // Red
    ctx->arc(size / 2.0, size / 2.0, size / 2.0 - 1, 0, 2 * M_PI);
    ctx->fill();
    
    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_bell_sprite() {
    int size = GAME_CHAR_WIDTH * scale_factor;
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto ctx = Cairo::Context::create(surface);
    
    ctx->set_source_rgb(1.0, 1.0, 0.0); // Gold
    ctx->arc(size / 2.0, size / 2.0, size / 2.0 - 1, 0, 2 * M_PI);
    ctx->fill();
    
    // Bell details
    ctx->set_source_rgb(0.8, 0.8, 0.0);
    ctx->arc(size / 2.0, size / 3.0, size / 4.0, 0, 2 * M_PI);
    ctx->fill();
    
    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_tack_sprite() {
    int size = GAME_CHAR_WIDTH * scale_factor;
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto ctx = Cairo::Context::create(surface);
    
    ctx->set_source_rgb(0.5, 0.5, 0.5); // Gray
    // Triangle pointing up
    ctx->move_to(size / 2.0, 0);
    ctx->line_to(0, size);
    ctx->line_to(size, size);
    ctx->close_path();
    ctx->fill();
    
    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_spring_sprite(bool upward) {
    int size = GAME_CHAR_WIDTH * scale_factor;
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    auto ctx = Cairo::Context::create(surface);
    
    if(upward) {
        ctx->set_source_rgb(0.0, 1.0, 0.0); // Green
        // Horizontal lines (coil effect)
        int line_height = std::max(1, size / 8);
        for(int i = 0; i < 4; i++) {
            int y = size - (i + 1) * (size / 4);
            ctx->rectangle(size * 0.25, y, size * 0.5, line_height);
            ctx->fill();
        }
    } else {
        ctx->set_source_rgb(redbg, greenbg, bluebg); // Blue
        // Vertical lines (coil effect)
        int line_width = std::max(1, size / 8);
        for(int i = 0; i < 4; i++) {
            int x = i * (size / 4);
            ctx->rectangle(x, size * 0.25, line_width, size * 0.5);
            ctx->fill();
        }
    }
    
    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_empty_sprite() {
    int size = GAME_CHAR_WIDTH * scale_factor;
    return Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::get_sprite(const std::string& name) {
    auto it = sprites.find(name);
    if(it != sprites.end()) {
        return it->second;
    }
    return sprites["EMPTY"];
}

WillyGame::WillyGame() : 
    vbox(Gtk::ORIENTATION_VERTICAL), 
    current_state(GameState::INTRO),
    scale_factor(game_options.scale_factor),  // Use command line scale factor
    level(game_options.starting_level),       // Use command line starting level
    score(0),
    lives(5),
    bonus(1000),
    willy_position({23, 7}),
    previous_willy_position({23, 7}),         // Initialize previous position tracker
    willy_direction("RIGHT"),
    willy_velocity({0, 0}),
    jumping(false),
    fps(game_options.fps),                    // Use command line FPS
    frame_count(0),
    current_level("level1"),
    continuous_direction(""),
    moving_continuously(false),
    up_pressed(false),
    down_pressed(false),
    gen(rd()) {
    
    set_title("Willy the Worm - C++ GTK Edition");
    score_manager = std::make_unique<HighScoreManager>();
    sprite_loader = std::make_unique<SpriteLoader>(scale_factor);
    level_loader = std::make_unique<LevelLoader>();
    sound_manager = std::make_unique<SoundManager>();
    
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
    
    int total_height = base_game_height + menubar_min.height + statusbar_min.height + 10;
    
    set_default_size(base_game_width, total_height);
    resize(base_game_width, total_height);
    
    // Connect resize signal
    signal_size_allocate().connect(sigc::hide(sigc::mem_fun(*this, &WillyGame::on_window_resize)));
    
    // Initial scaling calculation
    calculate_scaling_factors();
    
    drawing_area.grab_focus();
    
    // Print command line options being used
    std::cout << "Game initialized with options:" << std::endl;
    std::cout << "  Starting level: " << level << std::endl;
    std::cout << "  Levels file: " << game_options.levels_file << std::endl;
    std::cout << "  Number of balls: " << game_options.number_of_balls << std::endl;
    std::cout << "  FPS: " << fps << std::endl;
    std::cout << "  Scale factor: " << scale_factor << std::endl;
    std::cout << "  Sound enabled: " << (sound_manager->is_sound_enabled() ? "Yes" : "No") << std::endl;
    if (game_options.use_wasd) std::cout << "  WASD controls: Enabled" << std::endl;
    if (game_options.disable_flash) std::cout << "  Death flash: Disabled" << std::endl;
    if (game_options.mouse_support) std::cout << "  Mouse support: Enabled (not yet implemented)" << std::endl;
}
WillyGame::~WillyGame() {
    timer_connection.disconnect();
}

bool WillyGame::on_button_press(GdkEventButton* event) {
    if (!game_options.mouse_support || current_state != GameState::PLAYING) {
        return false;
    }
    
    // Debug: Print which button was pressed
    std::cout << "Mouse button " << event->button << " pressed" << std::endl;
    
    // Get menubar height to account for offset
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    int menubar_height = menubar_min.height;
    
    // Convert mouse coordinates to game coordinates
    double mouse_x = event->x;
    double mouse_y = event->y - menubar_height;
    
    // Account for scaling
    mouse_x /= current_scale_x;
    mouse_y /= current_scale_y;
    
    // Convert to grid coordinates
    int scaled_char_width = GAME_CHAR_WIDTH * scale_factor;
    int scaled_char_height = GAME_CHAR_HEIGHT * scale_factor;
    
    int click_col = (int)(mouse_x / scaled_char_width);
    int click_row = (int)(mouse_y / scaled_char_height);
    
    // Get Willy's current position
    int willy_row = willy_position.first;
    int willy_col = willy_position.second;
    
    std::cout << "Mouse click at grid (" << click_row << ", " << click_col << "), Willy at (" << willy_row << ", " << willy_col << ")" << std::endl;
    
    if (event->button == 1) {  // Left mouse button
        mouse_button_held = true;
        held_button = 1;
        
        // Determine direction based on where user clicked relative to Willy
        int row_diff = click_row - willy_row;
        int col_diff = click_col - willy_col;
        
        // Use the larger difference to determine primary direction
        if (abs(col_diff) > abs(row_diff)) {
            // Horizontal movement
            if (col_diff > 0) {
                // Clicked to the right of Willy
                mouse_direction = "RIGHT";
                continuous_direction = "RIGHT";
                moving_continuously = true;
                willy_direction = "RIGHT";
                std::cout << "Holding RIGHT" << std::endl;
            } else if (col_diff < 0) {
                // Clicked to the left of Willy
                mouse_direction = "LEFT";
                continuous_direction = "LEFT";
                moving_continuously = true;
                willy_direction = "LEFT";
                std::cout << "Holding LEFT" << std::endl;
            }
        } else {
            // Vertical movement
            if (row_diff < 0) {
                // Clicked above Willy
                mouse_direction = "UP";
                mouse_up_held = true;
                up_pressed = true;
                std::cout << "Holding UP" << std::endl;
                
            } else if (row_diff > 0) {
                // Clicked below Willy
                mouse_direction = "DOWN";
                mouse_down_held = true;
                down_pressed = true;
                std::cout << "Holding DOWN" << std::endl;
            }
        }
        
    } else if (event->button == 2) {  // Middle mouse button - stop
        // Stop all movement
        continuous_direction = "";
        moving_continuously = false;
        up_pressed = false;
        down_pressed = false;
        mouse_up_held = false;
        mouse_down_held = false;
        mouse_button_held = false;
        std::cout << "Middle click - stopping Willy" << std::endl;
        
    } else if (event->button == 3) {  // Right mouse button - jump
        jump();
        std::cout << "Right click - jumping" << std::endl;
        
    } else {
        // Debug: Show any other button numbers
        std::cout << "Unknown mouse button: " << event->button << std::endl;
    }
    
    return true;
}


bool WillyGame::on_button_release(GdkEventButton* event) {
    if (!game_options.mouse_support || current_state != GameState::PLAYING) {
        return false;
    }
    
    std::cout << "Mouse button " << event->button << " released" << std::endl;
    
    if (event->button == 1 && mouse_button_held && held_button == 1) {
        // Stop the movement that was being held
        mouse_button_held = false;
        held_button = 0;
        
        if (mouse_direction == "LEFT" || mouse_direction == "RIGHT") {
            continuous_direction = "";
            moving_continuously = false;
            std::cout << "Released horizontal movement" << std::endl;
        } else if (mouse_direction == "UP") {
            mouse_up_held = false;
            up_pressed = false;
            std::cout << "Released UP movement" << std::endl;
        } else if (mouse_direction == "DOWN") {
            mouse_down_held = false;
            down_pressed = false;
            std::cout << "Released DOWN movement" << std::endl;
        }
        
        mouse_direction = "";
    }
    
    return true;
}

bool WillyGame::on_motion_notify(GdkEventMotion* event) {
    if (!game_options.mouse_support || current_state != GameState::PLAYING) {
        return false;
    }
    
    // Optional: Could implement mouse hover effects here
    // For now, just consume the event
    return true;
}


void WillyGame::setup_ui() {
    add(vbox);
    
    vbox.pack_start(menubar, Gtk::PACK_SHRINK);
    
    // Allow the drawing area to expand and fill available space
    drawing_area.set_hexpand(true);
    drawing_area.set_vexpand(true);
    
    drawing_area.signal_draw().connect(
        sigc::mem_fun(*this, &WillyGame::on_draw));
    
    drawing_area.set_can_focus(true);
    
    // Existing keyboard events
    drawing_area.add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    drawing_area.signal_key_press_event().connect(
        sigc::mem_fun(*this, &WillyGame::on_key_press));
    drawing_area.signal_key_release_event().connect(
        sigc::mem_fun(*this, &WillyGame::on_key_release));

   if (game_options.mouse_support) {
        // Enable all mouse button events including press and release
        drawing_area.add_events(Gdk::BUTTON_PRESS_MASK | 
                               Gdk::BUTTON_RELEASE_MASK |
                               Gdk::BUTTON1_MOTION_MASK | 
                               Gdk::BUTTON2_MOTION_MASK | 
                               Gdk::BUTTON3_MOTION_MASK);
        drawing_area.signal_button_press_event().connect(
            sigc::mem_fun(*this, &WillyGame::on_button_press));
        drawing_area.signal_button_release_event().connect(
            sigc::mem_fun(*this, &WillyGame::on_button_release));
        std::cout << "Mouse support enabled - hold mouse button to keep moving, middle-click to stop" << std::endl;
    }
    
   
    // Pack with expansion so it fills available space
    vbox.pack_start(drawing_area, Gtk::PACK_EXPAND_WIDGET);
    
    update_status_bar();
    vbox.pack_start(status_bar, Gtk::PACK_SHRINK);
}


void WillyGame::calculate_scaling_factors() {
    // Get current window size
    int window_width, window_height;
    get_size(window_width, window_height);
    
    // Get menubar and status bar heights
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    
    Gtk::Requisition statusbar_min, statusbar_nat;
    status_bar.get_preferred_size(statusbar_min, statusbar_nat);
    
    // Calculate available space for the game area
    int available_width = window_width;
    int available_height = window_height - menubar_min.height - statusbar_min.height;
    
    // Calculate scale based on height, unless width is smaller than height
    double scale;
    if (available_width < available_height) {
        // Width is the limiting factor
        scale = (double)available_width / base_game_width;
    } else {
        // Height is the limiting factor (normal case)
        scale = (double)available_height / base_game_height;
    }
    
    // Round to nearest 0.1
    double rounded_scale = std::round(scale*10.0)/10.0;
    
    // Apply the same scale to both dimensions to maintain aspect ratio
    current_scale_x = rounded_scale;
    current_scale_y = rounded_scale;
    
    // Don't scale below 0.1 or above 10.0 for sanity
    current_scale_x = std::max(0.1, std::min(10.0, current_scale_x));
    current_scale_y = std::max(0.1, std::min(10.0, current_scale_y));
}


void WillyGame::on_window_resize() {
    calculate_scaling_factors();
    
    // Force a redraw
    drawing_area.queue_draw();
}

void WillyGame::load_level(const std::string& level_name) {
    current_level = level_name;
    
    // Check if level exists
    if(!level_loader->level_exists(level_name)) {
        std::cout << "Level " << level_name << " does not exist!" << std::endl;
        return;
    }
    
    // Get Willy's starting position from the level
    willy_position = level_loader->get_willy_start_position(level_name);
    
    // Initialize balls at the ball pit position
    balls.clear();
    //std::pair<int, int> ball_pit_pos = level_loader->get_ball_pit_position(level_name);
    std::pair<int, int> ball_pit_pos = find_ballpit_position();
    printf("Ball %i %i\n", ball_pit_pos.first, ball_pit_pos.second); 
    for(int i = 0; i < game_options.number_of_balls; i++) {
       balls.emplace_back(ball_pit_pos.first, ball_pit_pos.second);
    }
    
    std::cout << "Loaded level: " << level_name << std::endl;
    std::cout << "Willy starts at: (" << willy_position.first << ", " << willy_position.second << ")" << std::endl;
    std::cout << "Ball pit at: (" << ball_pit_pos.first << ", " << ball_pit_pos.second << ")" << std::endl;
}

bool WillyGame::check_movement_collision(int old_row, int old_col, int new_row, int new_col) {
    // Don't check collisions if moving to/from ballpit
    std::string old_tile = get_tile(old_row, old_col);
    std::string new_tile = get_tile(new_row, new_col);
    if(old_tile == "BALLPIT" || new_tile == "BALLPIT") {
        return false; // No collision in ballpit areas
    }
    
    for(const auto& ball : balls) {
        // Skip balls that are in ballpits
        if(get_tile(ball.row, ball.col) == "BALLPIT") {
            continue;
        }
        
        // Only check collision if ball is at Willy's new position (same row AND column)
        if(ball.row == new_row && ball.col == new_col) {
            return true; // Collision detected - ball and Willy at same position
        }
        
        // Check for crossing paths (ball and Willy swapping positions)
        // This is only relevant if they're both moving horizontally at the same level
        if(ball.row == old_row && ball.col == old_col && 
           ball.row == new_row && ball.col == new_col && 
           old_row == new_row) { // Only check swapping if on same horizontal level
            return true; // Collision detected - crossing paths horizontally
        }
    }
    return false;
}

bool WillyGame::on_key_press(GdkEventKey* event) {
    std::string keyname = gdk_keyval_name(event->keyval);
    //std::cout << "Key pressed: " << keyname << std::endl;
    keys_pressed.insert(keyname);
    
    // Check for modifier keys
    bool ctrl_pressed = (event->state & GDK_CONTROL_MASK);
    bool shift_pressed = (event->state & GDK_SHIFT_MASK);
    bool alt_pressed = (event->state & GDK_MOD1_MASK);
        
    if(keyname == "Escape") {
        quit_game();
    } else if(keyname == "F11") {
        if(get_window()->get_state() & GDK_WINDOW_STATE_FULLSCREEN) {
            unfullscreen();
        } else {
            fullscreen();
        }
    } else if(current_state == GameState::INTRO) {
        if(keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
            std::cout << "Starting game..." << std::endl;
            start_game();
        }
    } else if(current_state == GameState::PLAYING) {
        if(keyname == "space") {
            jump();
     } else if(keyname == "Left" || (game_options.use_wasd && keyname == "a")) {
        continuous_direction = "LEFT";
        moving_continuously = true;
        willy_direction = "LEFT";
    } else if(keyname == "Right" || (game_options.use_wasd && keyname == "d")) {
        continuous_direction = "RIGHT";
        moving_continuously = true;
        willy_direction = "RIGHT";
    } else if(keyname == "Up" || (game_options.use_wasd && keyname == "w")) {
        up_pressed = true;
    } else if(keyname == "Down" || (game_options.use_wasd && keyname == "s")) {
        down_pressed = true;
        } else if((keyname == "L" || keyname == "l") && ctrl_pressed) {
            // Level skip with Ctrl+L (matching Python version)
            complete_level_nobonus();
        } else if((keyname == "S" || keyname == "s") && ctrl_pressed) {
            // Sound toggle with Ctrl+S
            bool current_sound_state = sound_manager->is_sound_enabled();
            sound_manager->set_sound_enabled(!current_sound_state);
            
            // Visual feedback for sound toggle
            std::string sound_status = current_sound_state ? "OFF" : "ON";
            std::cout << "Sound toggled " << sound_status << std::endl;
            
            // Optional: Play a test sound when enabling
            if (!current_sound_state) {
                sound_manager->play_sound("bell.mp3");
            }
        } else if(keyname == "F5") {
            redbg+=0.25;
            if (redbg>1.0) {
                redbg=0.0;
            }
        } else if(keyname == "F6") {
            greenbg+=0.25;
            if (greenbg>1.0) {
                greenbg=0.0;
            }
        } else if(keyname == "F7") {
            bluebg+=0.25;
            if (bluebg>1.0) {
                bluebg=0.0;
            }
        } else {
            moving_continuously = false;
            continuous_direction = "";
        }
    } else if(current_state == GameState::GAME_OVER) {
        if(keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
            current_state = GameState::INTRO;
        }
    } else if(current_state == GameState::HIGH_SCORE_ENTRY) {
        if(keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
            if(!name_input.empty()) {
                score_manager->add_score(name_input, score);
            }
            current_state = GameState::HIGH_SCORE_DISPLAY;
        } else if(keyname == "BackSpace") {
            if(!name_input.empty()) {
                name_input.pop_back();
            }
        } else if(keyname.length() == 1) {
            // Single character key
            if(name_input.length() < 20) { // Limit name length
                name_input += keyname;
            }
        }
    } else if(current_state == GameState::HIGH_SCORE_DISPLAY) {
        if(keyname == "Escape") {
            quit_game();
        } else {
            current_state = GameState::INTRO;
        }
    }
    
    return true;
}

bool WillyGame::on_key_release(GdkEventKey* event) {
    std::string keyname = gdk_keyval_name(event->keyval);
    keys_pressed.erase(keyname);
    
    if(keyname == "Up") {
        up_pressed = false;
    } else if(keyname == "Down") {
        down_pressed = false;
    }
    
    return true;
}

void WillyGame::start_game() {
    current_state = GameState::PLAYING;
    level = 1;
    score = 0;
    lives = 5;
    bonus = 1000;
    frame_count = 0;
    continuous_direction = "";
    moving_continuously = false;
    up_pressed = false;
    down_pressed = false;
    
    load_level("level1");
    update_status_bar();
}

void WillyGame::jump() {
    int y = willy_position.first;
    int x = willy_position.second;

    // Get the current and below tiles
    std::string current_tile = get_tile(y, x);
    std::string below_tile = get_tile(y + 1, x);

    // Can jump if standing on "UPSPRING" or if below tile is a "PIPE"
    if (current_tile == "UPSPRING" || below_tile.substr(0, 4) == "PIPE" || y == GAME_MAX_HEIGHT - 1) {
        jumping = true;

        // Apply a stronger jump if standing on "UPSPRING"
        willy_velocity.second = (current_tile == "UPSPRING") ? -6 : -5;

        sound_manager->play_sound("jump.mp3");
    }
}


std::string WillyGame::get_tile(int row, int col) {
    return level_loader->get_tile(current_level, row, col);
}

void WillyGame::set_tile(int row, int col, const std::string& tile) {
    level_loader->set_tile(current_level, row, col, tile);
}

bool WillyGame::can_move_to(int row, int col) {
    if(row < 0 || row >= GAME_SCREEN_HEIGHT || col < 0 || col >= GAME_SCREEN_WIDTH) {
        return false;
    }
    
    std::string tile = get_tile(row, col);
    return (tile == "EMPTY" || tile == "LADDER" || tile == "PRESENT" || 
            tile == "BELL" || tile == "UPSPRING" || tile == "SIDESPRING" || 
            tile == "TACK" || tile == "BALLPIT" || tile == "WILLY_RIGHT" || tile == "WILLY_LEFT");  // Added BALLPIT
}

bool WillyGame::is_on_solid_ground() {
    int y = willy_position.first;
    int x = willy_position.second;
    
    if(y >= GAME_MAX_HEIGHT - 1) {
        return true;
    }
    
    std::string current_tile = get_tile(y, x);
    std::string below_tile = get_tile(y + 1, x);
    
    if(current_tile == "LADDER") {
        return true;
    }
    
    return (below_tile.substr(0, 4) == "PIPE");
}

void WillyGame::update_willy_movement() {
    if(current_state != GameState::PLAYING) return;
    previous_willy_position = willy_position;
    // Store Willy's current position for collision checking
    int old_row = willy_position.first;
    int old_col = willy_position.second;
    
    std::string current_tile = get_tile(willy_position.first, willy_position.second);
    bool on_ladder = (current_tile == "LADDER");
    bool moved_on_ladder = false;
    
    if(up_pressed) {
        int target_row = willy_position.first - 1;
        if(target_row >= 0) {
            std::string above_tile = get_tile(target_row, willy_position.second);
            
            if(on_ladder && above_tile == "LADDER" && can_move_to(target_row, willy_position.second)) {
                // Check for collision before moving
                if(!check_movement_collision(old_row, old_col, target_row, willy_position.second)) {
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
            else if(!on_ladder && above_tile == "LADDER" && can_move_to(target_row, willy_position.second)) {
                // Check for collision before moving
                if(!check_movement_collision(old_row, old_col, target_row, willy_position.second)) {
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
    
    if(down_pressed && !moved_on_ladder) {
        int target_row = willy_position.first + 1;
        if(target_row < GAME_SCREEN_HEIGHT) {
            std::string below_tile = get_tile(target_row, willy_position.second);
            
            if(on_ladder && can_move_to(target_row, willy_position.second)) {
                // Check for collision before moving
                if(!check_movement_collision(old_row, old_col, target_row, willy_position.second)) {
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
            else if(!on_ladder && below_tile == "LADDER" && can_move_to(target_row, willy_position.second)) {
                // Check for collision before moving
                if(!check_movement_collision(old_row, old_col, target_row, willy_position.second)) {
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
    
    if(!moved_on_ladder) {
        if(moving_continuously && !continuous_direction.empty()) {
            bool hit_obstacle = false;
            
            if(continuous_direction == "LEFT") {
                if(can_move_to(willy_position.first, willy_position.second - 1)) {
                    // Check for collision before moving
                    if(!check_movement_collision(old_row, old_col, willy_position.first, willy_position.second - 1)) {
                        willy_position.second--;
                    } else {
                        die();
                        return;
                    }
                } else {
                    hit_obstacle = true;
                }
            } else if(continuous_direction == "RIGHT") {
                if(can_move_to(willy_position.first, willy_position.second + 1)) {
                    // Check for collision before moving
                    if(!check_movement_collision(old_row, old_col, willy_position.first, willy_position.second + 1)) {
                        willy_position.second++;
                    } else {
                        die();
                        return;
                    }
                } else {
                    hit_obstacle = true;
                }
            }
            
            if(hit_obstacle) {
                moving_continuously = false;
                continuous_direction = "";
            }
        }
        else if(!moving_continuously) {
            if(keys_pressed.count("Left")) {
                willy_direction = "LEFT";
                if(can_move_to(willy_position.first, willy_position.second - 1)) {
                    // Check for collision before moving
                    if(!check_movement_collision(old_row, old_col, willy_position.first, willy_position.second - 1)) {
                        willy_position.second--;
                    } else {
                        die();
                        return;
                    }
                }
            } else if(keys_pressed.count("Right")) {
                willy_direction = "RIGHT";
                if(can_move_to(willy_position.first, willy_position.second + 1)) {
                    // Check for collision before moving
                    if(!check_movement_collision(old_row, old_col, willy_position.first, willy_position.second + 1)) {
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
    
    if(!on_ladder) {
        if(!is_on_solid_ground()) {
            willy_velocity.second += 1;
        } else {
            if(willy_velocity.second > 0) {
                willy_velocity.second = 0;
                jumping = false;
            }
        }
        
        if(willy_velocity.second != 0) {
            int new_y = willy_position.first + (willy_velocity.second > 0 ? 1 : -1);
            if(can_move_to(new_y, willy_position.second)) {
                // Check for collision before moving due to gravity/jumping
                if(!check_movement_collision(willy_position.first, willy_position.second, new_y, willy_position.second)) {
                    willy_position.first = new_y;
                } else {
                    die();
                    return;
                }
            }
            
            if(willy_velocity.second < 0) {
                willy_velocity.second++;
            }
        }
    } else {
        willy_velocity.second = 0;
        jumping = false;
    }
}

void WillyGame::update_balls() {
    if (current_state != GameState::PLAYING) return;
    
    static std::chrono::steady_clock::time_point last_ball_spawn = std::chrono::steady_clock::now();
    static std::uniform_int_distribution<int> delay_distribution(500, 2000); // Random delay between 500ms - 2000ms
    static std::mt19937 rng(std::random_device{}());

    // Get the primary ball pit position
    std::pair<int, int> primary_ball_pit_pos = find_ballpit_position();

    // Move any balls in non-primary ball pit positions to the primary ball pit
    for (auto& ball : balls) {
        if (get_tile(ball.row, ball.col) == "BALLPIT" &&
            (ball.row != primary_ball_pit_pos.first || ball.col != primary_ball_pit_pos.second)) {
            ball.row = primary_ball_pit_pos.first;
            ball.col = primary_ball_pit_pos.second;
            ball.direction = ""; // Reset movement after relocation
        }
    }

    // Apply movement logic to all balls
    for (auto& ball : balls) {
        // Apply gravity to balls
        if (ball.row < GAME_MAX_HEIGHT - 1 && get_tile(ball.row + 1, ball.col).substr(0, 4) != "PIPE") {
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
    if (balls.size() < game_options.number_of_balls && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ball_spawn).count() > delay_distribution(rng)) {
        balls.emplace_back(primary_ball_pit_pos.first, primary_ball_pit_pos.second);
        last_ball_spawn = now; // Reset spawn timer
    }
}

void WillyGame::check_collisions() {
    if(current_state != GameState::PLAYING) return;
    
    int y = willy_position.first;
    int x = willy_position.second;
    std::string current_tile = get_tile(y, x);
    
    // Check if Willy left a destroyable pipe (PIPE18) - destroy it after he leaves
    int prev_y = previous_willy_position.first;
    int prev_x = previous_willy_position.second;
    
    // Only check if Willy actually moved
    if(prev_y != y || prev_x != x) {
        // Check if there's a destroyable pipe below where Willy was previously standing
        if(prev_y + 1 < GAME_SCREEN_HEIGHT) {
            std::string below_previous_tile = get_tile(prev_y + 1, prev_x);
            if(below_previous_tile == "PIPE18") {
                // Destroy the pipe after Willy leaves it
                set_tile(prev_y + 1, prev_x, "EMPTY");
                
                // Optional: Play a destruction sound
                sound_manager->play_sound("present.mp3");  // Using existing sound
                
                // Optional: Add points for destroying the pipe
                score += 50;
            }
        }
    }
    
    // Check ball collisions (only die if at same horizontal level and same column)
    for(const auto& ball : balls) {
        // Only check collision if Willy and ball are at the SAME row AND column
        // AND not in a ballpit
        if(ball.row == y && ball.col == x && current_tile != "BALLPIT") {
            sound_manager->play_sound("tack.mp3"); // Death sound
            die();
            return;
        }
    }
    
    // Check tile interactions
    if(current_tile == "TACK") {
        //sound_manager->play_sound("tack.mp3");
        die();
    } else if(current_tile == "BELL") {
        sound_manager->play_sound("bell.mp3");
        complete_level();
    } else if(current_tile == "PRESENT") {
        score += 100;
        sound_manager->play_sound("present.mp3");
        set_tile(y, x, "EMPTY");
    } else if(current_tile == "UPSPRING") {
       sound_manager->play_sound("jump.mp3");
       jump();
    } else if(current_tile == "SIDESPRING") {
        sound_manager->play_sound("jump.mp3");
        // Reverse continuous direction if moving continuously
        if(moving_continuously) {
            if(continuous_direction == "RIGHT") {
                continuous_direction = "LEFT";
                willy_direction = "LEFT";
            } else if(continuous_direction == "LEFT") {
                continuous_direction = "RIGHT";
                willy_direction = "RIGHT";
            }
        } else {
            // Just reverse direction without continuous movement
            willy_direction = (willy_direction == "RIGHT") ? "LEFT" : "RIGHT";
        }
    }
    
    // Check for jumping over balls (bonus points)
    if(jumping || willy_velocity.second != 0) {
        for(int i = 1; i < 5; i++) {
            int check_y = y + i;
            if(check_y < GAME_SCREEN_HEIGHT) {
                for(const auto& ball : balls) {
                    if(ball.row == check_y && ball.col == x) {
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
    if(lives <= 0) {
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
    if(level_loader->level_exists(next_level)) {
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
    if(level_loader->level_exists(next_level)) {
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
    if(score_manager->is_high_score(score)) {
        current_state = GameState::HIGH_SCORE_ENTRY;
        name_input = "";
    } else {
        current_state = GameState::HIGH_SCORE_DISPLAY;
    }
}

void WillyGame::new_game() {
    current_state = GameState::INTRO;
}

void WillyGame::quit_game() {
    hide();
}

void WillyGame::update_status_bar() {
    if(current_state == GameState::PLAYING) {
        std::string status_text = "SCORE: " + std::to_string(score) + 
                                "    BONUS: " + std::to_string(bonus) + 
                                "    Level: " + std::to_string(level) + 
                                "    Willy the Worms Left: " + std::to_string(lives);
        status_bar.set_text(status_text);
    } else {
        status_bar.set_text("Willy the Worm - C++ GTK Edition");
    }
}

bool WillyGame::game_tick() {
    if(current_state == GameState::PLAYING) {
        update_willy_movement();
        update_balls();
        check_collisions();
        
        // Update bonus
        frame_count++;
        if(frame_count >= fps) {
            frame_count = 0;
            bonus = std::max(0, bonus - 10);
        }
        update_status_bar();
    }
    drawing_area.queue_draw();
    
    return true; // Continue the timer
}

bool WillyGame::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Only paint blue background for intro screen
    if(current_state == GameState::INTRO) {
        cr->set_source_rgb(redbg, greenbg, bluebg); // Blue background for intro only
        cr->paint();
        draw_intro_screen(cr);
    } else if(current_state == GameState::PLAYING) {
        draw_game_screen(cr);
    } else if(current_state == GameState::GAME_OVER) {
        draw_game_over_screen(cr);
    } else if(current_state == GameState::HIGH_SCORE_ENTRY) {
    draw_high_score_entry_screen(cr);
} else if(current_state == GameState::HIGH_SCORE_DISPLAY) {
    draw_high_score_display_screen(cr);
}
    
    return true;
}

std::pair<int, int> WillyGame::find_ballpit_position() {
    for(int row = 0; row < GAME_MAX_HEIGHT; row++) {
        for(int col = 0; col < GAME_MAX_WIDTH; col++) {
            std::string tile = get_tile(row, col);
            if(tile == "BALLPIT") {
                return {row, col}; // Return the first position found
            }
        }
    }
    return {-1, -1}; // Indicate that no BALLPIT was found
}

void WillyGame::draw_game_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Get menubar height to use as offset
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    int menubar_height = menubar_min.height;
    
    // Apply scaling transformation
    cr->save();
    cr->translate(0, menubar_height);
    cr->scale(current_scale_x, current_scale_y);
    
    // Calculate the scaled character dimensions
    int scaled_char_width = GAME_CHAR_WIDTH * scale_factor;
    int scaled_char_height = GAME_CHAR_HEIGHT * scale_factor;
    
    // Draw ALL sprite positions with blue background, even empty ones
    for(int row = 0; row < GAME_MAX_HEIGHT; row++) {
        for(int col = 0; col < GAME_MAX_WIDTH; col++) {
            int x = col * scaled_char_width;
            int y = row * scaled_char_height;
            
            // Paint blue background for EVERY sprite position
            cr->set_source_rgb(redbg, greenbg, bluebg);
            cr->rectangle(x, y, scaled_char_width, scaled_char_height);
            cr->fill();
            
            // Get the tile at this position
            std::string tile = get_tile(row, col);
            
            // Draw sprite if not empty or Willy start position, but not at Willy's current position
            if(tile != "EMPTY" && tile.find("WILLY") == std::string::npos && 
               !(row == willy_position.first && col == willy_position.second)) {
                auto sprite = sprite_loader->get_sprite(tile);
                if(sprite) {
                    cr->set_source(sprite, x, y);
                    cr->paint();
                }
            }
        }
    }
    
    // Draw balls (but not the ones in ball pits or at Willy's position)
    for(const auto& ball : balls) {
        if(get_tile(ball.row, ball.col) != "BALLPIT" && 
           !(ball.row == willy_position.first && ball.col == willy_position.second)) {
            
            // Make sure ball is in visible area
            if(ball.row >= 0 && ball.row < GAME_MAX_HEIGHT && 
               ball.col >= 0 && ball.col < GAME_MAX_WIDTH) {
                
                int x = ball.col * scaled_char_width;
                int y = ball.row * scaled_char_height;
                
                auto sprite = sprite_loader->get_sprite("BALL");
                if(sprite) {
                    cr->set_source(sprite, x, y);
                    cr->paint();
                }
            }
        }
    }
    
    // Draw Willy - make sure he's in visible area
    if(willy_position.first >= 0 && willy_position.first < GAME_MAX_HEIGHT &&
       willy_position.second >= 0 && willy_position.second < GAME_MAX_WIDTH) {
        
        int x = willy_position.second * scaled_char_width;
        int y = willy_position.first * scaled_char_height;
        
        std::string sprite_name = (willy_direction == "LEFT") ? "WILLY_LEFT" : "WILLY_RIGHT";
        auto sprite = sprite_loader->get_sprite(sprite_name);
        if(sprite) {
            cr->set_source(sprite, x, y);
            cr->paint();
        }
    }
    
    // Draw status information below the game area
    if(current_state == GameState::PLAYING) {
        // Calculate status bar area (below the main game area)
        int status_y = (GAME_SCREEN_HEIGHT + 1) * scaled_char_height; // +1 to go BELOW the last line
        int status_height = 2 * scaled_char_height; // Give it some height
        
        // Draw blue background for status area (same as game background)
        cr->set_source_rgb(redbg, greenbg, bluebg);
        cr->rectangle(0, status_y, GAME_SCREEN_WIDTH * scaled_char_width, status_height);
        cr->fill();
        
        // Create font for status text
        Pango::FontDescription font_desc;
        font_desc.set_family("Courier");
        // Scale font size based on the current scale - make it bigger and scale properly
        int font_size = std::max(12, (int)(16 * std::min(current_scale_x, current_scale_y)));
        font_desc.set_size(font_size * PANGO_SCALE);
        
        auto layout = Pango::Layout::create(cr);
        layout->set_font_description(font_desc);
        
        // Create status text with fixed-width formatting
        char status_buffer[200];
        snprintf(status_buffer, sizeof(status_buffer), 
                "SCORE: %5d    BONUS: %4d    LEVEL: %2d    WILLY THE WORMS LEFT: %3d",
                score, bonus, level, lives);
        std::string status_text = status_buffer;
        
        layout->set_text(status_text);
        
        // Draw white text
        cr->set_source_rgb(1.0, 1.0, 1.0);
        
        // Position text in the status area
        int text_width, text_height;
        layout->get_pixel_size(text_width, text_height);
        
        // Center the text in the status area
        int text_x = (GAME_SCREEN_WIDTH * scaled_char_width - text_width) / 2;
        int text_y = status_y + (status_height - text_height) / 2;
        
        cr->move_to(text_x, text_y);
        layout->show_in_cairo_context(cr);
    }
    
    cr->restore();
}
void WillyGame::create_menubar() {
    // Set menubar background to proper gray theme
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data(R"(
        menubar {
            background-color: #f0f0f0;
            color: #000000;
            border-bottom: 1px solid #d0d0d0;
        }
        menubar > menuitem {
            background-color: #f0f0f0;
            color: #000000;
            padding: 4px 8px;
        }
        menubar > menuitem:hover {
            background-color: #e0e0e0;
            color: #000000;
        }
        menubar menu {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #d0d0d0;
        }
        menubar menu menuitem {
            background-color: #ffffff;
            color: #000000;
            padding: 4px 12px;
        }
        menubar menu menuitem:hover {
            background-color: #0078d4;
            color: #ffffff;
        }
        menubar menu separator {
            background-color: #d0d0d0;
            min-height: 1px;
        }
    )");
    
    auto style_context = menubar.get_style_context();
    style_context->add_provider(css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // File menu
    auto file_menu = Gtk::manage(new Gtk::Menu());
    
    auto new_game_item = Gtk::manage(new Gtk::MenuItem("_New Game", true));
    new_game_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyGame::new_game));
    file_menu->append(*new_game_item);
    
    file_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
    
    auto quit_item = Gtk::manage(new Gtk::MenuItem("_Quit", true));
    quit_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyGame::quit_game));
    file_menu->append(*quit_item);
    
    auto file_item = Gtk::manage(new Gtk::MenuItem("_File", true));
    file_item->set_submenu(*file_menu);
    menubar.append(*file_item);
    
    // Game menu
    auto game_menu = Gtk::manage(new Gtk::Menu());
    
    auto pause_item = Gtk::manage(new Gtk::MenuItem("_Pause", true));
    pause_item->signal_activate().connect([this]() {
        if(current_state == GameState::PLAYING) {
            current_state = GameState::PAUSED;
        } else if(current_state == GameState::PAUSED) {
            current_state = GameState::PLAYING;
        }
    });
    game_menu->append(*pause_item);
    
    auto reset_item = Gtk::manage(new Gtk::MenuItem("_Reset Level", true));
    reset_item->signal_activate().connect([this]() {
        if(current_state == GameState::PLAYING) {
            reset_level();
        }
    });
    game_menu->append(*reset_item);
    
    auto game_item = Gtk::manage(new Gtk::MenuItem("_Game", true));
    game_item->set_submenu(*game_menu);
    menubar.append(*game_item);
    
    // Help menu
    auto help_menu = Gtk::manage(new Gtk::Menu());
    
    auto about_item = Gtk::manage(new Gtk::MenuItem("_About", true));
    about_item->signal_activate().connect([this]() {
        current_state = GameState::INTRO;
    });
    help_menu->append(*about_item);
    
    auto help_item = Gtk::manage(new Gtk::MenuItem("_Help", true));
    help_item->set_submenu(*help_menu);
    menubar.append(*help_item);
}

void WillyGame::draw_intro_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Get menubar height to use as offset
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    int menubar_height = menubar_min.height;
    
    // Get current drawing area size
    Gtk::Allocation allocation = drawing_area.get_allocation();
    int area_width = allocation.get_width();
    int area_height = allocation.get_height() - menubar_height;
    
    // Apply offset for menubar and clear background
    cr->save();
    cr->translate(0, menubar_height);
    
    // Clear the intro area with blue background
    cr->set_source_rgb(redbg, greenbg, bluebg);
    cr->rectangle(0, 0, area_width, area_height);
    cr->fill();
    
    // Text data
    std::vector<std::vector<std::string>> textdata = {
        {""},
        {""},
        {"Willy the Worm"},
        {""},
        {"By Jason Hall"},
        {"(original version by Alan Farmer 1985)"},
        {""},
        {"This code is Free Open Source Software (FOSS)"},
        {"Please feel free to do with it whatever you wish."},
        {""},
        {"If you do make changes though such as new levels,"},
        {"please share them with the world."},
        {""},
        {""},
        {"Meet Willy the Worm ", "WILLY_RIGHT", ". Willy is a fun-"},
        {"loving invertebrate who likes to climb"},
        {"ladders ", "LADDER", " bounce on springs ", "UPSPRING", " ", "SIDESPRING"},
        {"and find his presents ", "PRESENT", ".  But more"},
        {"than anything, Willy loves to ring,"},
        {"bells! ", "BELL"},
        {""},
        {"You can press the arrow keys ← ↑ → ↓"},
        {"to make Willy run and climb, or the"},
        {"space bar to make him jump. Anything"},
        {"else will make Willy stop and wait"},
        {""},
        {"Good luck, and don't let Willy step on"},
        {"a tack ", "TACK", " or get ran over by a ball! ", "BALL"},
        {""},
        {"Press Enter to Continue"}
    };
    
    // Calculate appropriate sizing
    int num_lines = textdata.size();
    int available_height = area_height - 40; // Leave some margin
    int line_height = available_height / num_lines;
    int base_font_size = std::max(10, std::min(20, line_height - 2));
    
    // Set up font
    Pango::FontDescription font_desc;
    font_desc.set_family("Courier");
    font_desc.set_size(base_font_size * PANGO_SCALE);
    
    auto layout = Pango::Layout::create(cr);
    layout->set_font_description(font_desc);
    
    // Sprite size should match text height
    int sprite_size = base_font_size;
    
    for(size_t line_idx = 0; line_idx < textdata.size(); line_idx++) {
        const auto& line = textdata[line_idx];
        
        if(line.empty() || (line.size() == 1 && line[0].empty())) {
            continue;
        }
        
        int y_pos = 20 + line_idx * line_height; // 20px top margin
        
        // Calculate line width for centering
        int line_width = 0;
        for(const auto& element : line) {
            if(element.empty()) continue;
            
            if(element == "WILLY_RIGHT" || element == "WILLY_LEFT" || 
               element == "LADDER" || element == "UPSPRING" || 
               element == "SIDESPRING" || element == "PRESENT" || 
               element == "BELL" || element == "TACK" || element == "BALL") {
                line_width += sprite_size;
            } else {
                layout->set_text(element);
                int text_w, text_h;
                layout->get_pixel_size(text_w, text_h);
                line_width += text_w;
            }
        }
        
        // Center the line
        int start_x = (area_width - line_width) / 2;
        int current_x = start_x;
        
        // Draw elements
        for(const auto& element : line) {
            if(element.empty()) continue;
            
            if(element == "WILLY_RIGHT" || element == "WILLY_LEFT" || 
               element == "LADDER" || element == "UPSPRING" || 
               element == "SIDESPRING" || element == "PRESENT" || 
               element == "BELL" || element == "TACK" || element == "BALL") {
                
                auto sprite = sprite_loader->get_sprite(element);
                if(sprite) {
                    cr->save();
                    cr->translate(current_x, y_pos);
                    
                    // Scale sprite to match text size
                    double sprite_scale = (double)sprite_size / (GAME_CHAR_WIDTH * scale_factor);
                    cr->scale(sprite_scale, sprite_scale);
                    
                    cr->set_source(sprite, 0, 0);
                    cr->paint();
                    cr->restore();
                }
                current_x += sprite_size;
            } else {
                // Draw text
                layout->set_text(element);
                int text_w, text_h;
                layout->get_pixel_size(text_w, text_h);
                
                cr->set_source_rgb(1.0, 1.0, 1.0);
                cr->move_to(current_x, y_pos);
                layout->show_in_cairo_context(cr);
                
                current_x += text_w;
            }
        }
    }
    
    cr->restore();
}


void WillyGame::draw_game_over_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Create font
    Pango::FontDescription font_desc;
    font_desc.set_family("Monospace");
    font_desc.set_size(24 * PANGO_SCALE);
    
    auto layout = create_pango_layout("");
    layout->set_font_description(font_desc);
    
    cr->set_source_rgb(1.0, 1.0, 1.0); // White text
    
    // Game Over text
    layout->set_text("GAME OVER");
    int text_width, text_height;
    layout->get_pixel_size(text_width, text_height);
    
    int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    int y_offset = (GAME_SCREEN_HEIGHT * GAME_CHAR_HEIGHT * scale_factor - text_height) / 2 - 50;
    
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
    
    // Final Score
    std::string score_text = "Final Score: " + std::to_string(score);
    layout->set_text(score_text);
    layout->get_pixel_size(text_width, text_height);
    
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    y_offset += 60;
    
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
    
    // Continue text
    layout->set_text("Press Enter to return to intro");
    layout->get_pixel_size(text_width, text_height);
    
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    y_offset += 80;
    
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
}

// WillyApplication implementation
WillyApplication::WillyApplication() : Gtk::Application("org.example.willytheworm") {}

Glib::RefPtr<WillyApplication> WillyApplication::create() {
    return Glib::RefPtr<WillyApplication>(new WillyApplication());
}

void WillyGame::flash_death_screen() {
    // Get the window and context for drawing
    auto window = drawing_area.get_window();
    if (!window) return;
    
    auto cr = window->create_cairo_context();
    
    // Flash white for 0.25 seconds
    cr->set_source_rgb(1.0, 1.0, 1.0); // White
    cr->paint();
    
    // Force immediate display update
    window->invalidate(false);
    
    // Process pending GTK events to ensure the white screen shows
    while (Gtk::Main::events_pending()) {
        Gtk::Main::iteration();
    }
    
    // Hold the white screen for 0.25 seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    
    // Return to normal - this will be handled by the next game tick/draw cycle
}

void WillyGame::flash_death_screen_seizure() {
    // Get the current drawing area allocation
    Gtk::Allocation allocation = drawing_area.get_allocation();
    
    // Calculate the duration of the flashing in seconds (0.25 seconds like Python)
    double duration = 0.25;
    
    // Calculate the number of times to switch between colors (assuming ~60 FPS)
    int num_flashes = static_cast<int>(duration * 60);
    
    // Colors to alternate between
    Cairo::RefPtr<Cairo::ImageSurface> surface1 = Cairo::ImageSurface::create(
        Cairo::FORMAT_RGB24, allocation.get_width(), allocation.get_height());
    Cairo::RefPtr<Cairo::ImageSurface> surface2 = Cairo::ImageSurface::create(
        Cairo::FORMAT_RGB24, allocation.get_width(), allocation.get_height());
    
    // Fill surfaces with different colors
    auto ctx1 = Cairo::Context::create(surface1);
    ctx1->set_source_rgb(1.0, 1.0, 1.0); // White
    ctx1->paint();
    
    auto ctx2 = Cairo::Context::create(surface2);
    ctx2->set_source_rgb(redbg, greenbg, bluebg); // Blue (or current background color)
    ctx2->paint();
    
    // Get the window and context for drawing
    auto window = drawing_area.get_window();
    if (!window) return;
    
    auto cr = window->create_cairo_context();
    
    // Start the flashing
    for (int i = 0; i < num_flashes; i++) {
        // Alternate between the two colors
        if (i % 2 == 0) {
            cr->set_source(surface1, 0, 0);
        } else {
            cr->set_source(surface2, 0, 0);
        }
        
        cr->paint();
        
        // Force immediate display update
        window->invalidate(false);
        
        // Process pending GTK events
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration();
        }
        
        // Wait for a short amount of time before the next frame
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(1000000.0 / 60.0)));
    }
    
    // Clear back to normal background
    cr->set_source_rgb(redbg, greenbg, bluebg); // Blue
    cr->paint();
    window->invalidate(false);
}

void WillyApplication::on_activate() {
    auto window = new WillyGame();
    add_window(*window);
    window->present();
}

void print_help(const char* program_name) {
    std::cout << "Willy the Worm - C++ GTK Edition\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -l LEVEL          Start at specific level (default: 1)\n";
    std::cout << "  -L LEVELFILE      Use custom levels file (default: levels.json)\n";
    std::cout << "  -b BALLS          Set number of balls (default: 6)\n";
    std::cout << "  -w                Use WASD keyboard controls instead of arrow keys\n";
    std::cout << "  -f                Disable death flash effect\n";
    std::cout << "  -F FPS            Set frames per second (default: 10)\n";
    std::cout << "  -m                Enable mouse support\n";
    std::cout << "  -s                Start with sound disabled\n";
    std::cout << "  -S SCALE          Set scale factor (default: 3)\n";
    std::cout << "  -h, --help        Show this help message\n\n";
    std::cout << "Controls:\n";
    std::cout << "  Arrow Keys        Move Willy (or WASD with -w option)\n";
    std::cout << "  Space             Jump\n";
    std::cout << "  Ctrl+L            Skip level\n";
    std::cout << "  Ctrl+S            Toggle sound\n";
    std::cout << "  F5/F6/F7          Change background colors\n";
    std::cout << "  F11               Toggle fullscreen\n";
    std::cout << "  Escape            Quit game\n\n";
}

bool parse_command_line(int argc, char* argv[]) {
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
        {nullptr, 0, nullptr, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "hl:L:b:wfF:msS:", long_options, &option_index)) != -1) {
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
                        std::cerr << "Error: " << ((c == 'l') ? "Level" :
                                                  (c == 'b') ? "Number of balls" :
                                                  (c == 'F') ? "FPS" : "Scale factor")
                                  << " must be between " << min << " and " << max << "\n";
                        return false;
                    }

                    if (c == 'l') game_options.starting_level = value;
                    else if (c == 'b') game_options.number_of_balls = value;
                    else if (c == 'F') game_options.fps = value;
                    else if (c == 'S') game_options.scale_factor = value;
                } catch (const std::exception&) {
                    std::cerr << "Error: Invalid value for " << ((c == 'l') ? "Level" :
                                                                 (c == 'b') ? "Number of balls" :
                                                                 (c == 'F') ? "FPS" : "Scale factor")
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
                return false;  // getopt_long already prints error messages

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


int main(int argc, char* argv[]) {
    // Parse command line arguments BEFORE creating GTK app
    if (!parse_command_line(argc, argv)) {
        print_help(argv[0]);
        return 1;
    }
    
    if (game_options.show_help) {
        print_help(argv[0]);
        return 0;
    }
    
    // Print startup information
    std::cout << "Willy the Worm - C++ GTK Edition\n";
    std::cout << "Starting level: " << game_options.starting_level << "\n";
    std::cout << "Levels file: " << game_options.levels_file << "\n";
    std::cout << "Number of balls: " << game_options.number_of_balls << "\n";
    std::cout << "FPS: " << game_options.fps << "\n";
    std::cout << "Scale factor: " << game_options.scale_factor << "\n";
    if (game_options.use_wasd) std::cout << "Using WASD controls\n";
    if (game_options.disable_flash) std::cout << "Death flash disabled\n";
    if (game_options.mouse_support) std::cout << "Mouse support enabled\n";
    if (!game_options.sound_enabled) std::cout << "Sound disabled\n";
    std::cout << "\n";

    // Create a new argc/argv with only the program name for GTK
    // GTK doesn't need to see our custom arguments
    int gtk_argc = 1;
    char* gtk_argv[] = {argv[0], nullptr};

    auto app = WillyApplication::create();
    return app->run(gtk_argc, gtk_argv);  // Pass cleaned arguments to GTK
}

