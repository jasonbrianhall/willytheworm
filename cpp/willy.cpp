#include "willy.h"

double redbg=0.0;
double greenbg=0.0;
double bluebg=1.0;


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
    scale_factor(3),
    level(1),
    score(0),
    lives(5),
    bonus(1000),
    willy_position({23, 7}),
    willy_direction("RIGHT"),
    willy_velocity({0, 0}),
    jumping(false),
    fps(10),
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
    
    level_loader->load_levels("levels.json");
    
    setup_ui();
    current_state = GameState::INTRO;
    
    timer_connection = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &WillyGame::game_tick), 1000 / fps);
    
    show_all_children();
    
    // Calculate proper window size AFTER UI is created so we can measure the menubar
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    
    Gtk::Requisition statusbar_min, statusbar_nat;
    status_bar.get_preferred_size(statusbar_min, statusbar_nat);
    
    int game_width = GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor;
    int game_height = GAME_SCREEN_HEIGHT * GAME_CHAR_HEIGHT * scale_factor;
    
    // Add actual measured heights of menubar and status bar, plus some padding
    int total_height = game_height + menubar_min.height + statusbar_min.height + 10;
    
    set_default_size(game_width, total_height);
    resize(game_width, total_height);
    
    drawing_area.grab_focus();
}

WillyGame::~WillyGame() {
    timer_connection.disconnect();
}

void WillyGame::setup_ui() {
    add(vbox);
    
    //create_menubar();  // Removed since it doesn't actually work and doesn't add anything
    vbox.pack_start(menubar, Gtk::PACK_SHRINK);
    
    // Set the drawing area size to match exactly what we need for the game
    int game_width = GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor;
    int game_height = (GAME_SCREEN_HEIGHT + 1) * GAME_CHAR_HEIGHT * scale_factor; // Adding plus 1 for the status bar
    
    drawing_area.set_size_request(game_width, game_height);
    
    // DON'T let the drawing area expand - keep it at exact size
    drawing_area.set_hexpand(false);
    drawing_area.set_vexpand(false);
    
    drawing_area.signal_draw().connect(
        sigc::mem_fun(*this, &WillyGame::on_draw));
    
    drawing_area.set_can_focus(true);
    drawing_area.add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    drawing_area.signal_key_press_event().connect(
        sigc::mem_fun(*this, &WillyGame::on_key_press));
    drawing_area.signal_key_release_event().connect(
        sigc::mem_fun(*this, &WillyGame::on_key_release));
    
    // Pack without expanding so it keeps its exact size
    vbox.pack_start(drawing_area, Gtk::PACK_SHRINK);
    
    update_status_bar();
    vbox.pack_start(status_bar, Gtk::PACK_SHRINK);
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
    for(int i = 0; i < 6; i++) {
       balls.emplace_back(ball_pit_pos.first, ball_pit_pos.second);
       //balls.emplace_back(1, 12);  
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
        
        // Check if ball is at Willy's new position
        if(ball.row == new_row && ball.col == new_col) {
            return true; // Collision detected
        }
        
        // Check for crossing paths (ball and Willy swapping positions)
        if(ball.row == old_row && ball.col == old_col && 
           ball.row == new_row && ball.col == new_col) {
            // This means ball was at Willy's old position and Willy is moving to ball's position
            // Need to check if ball is also moving to Willy's old position
            // This is a simplified check - you might need more sophisticated logic
            return true; // Collision detected
        }
    }
    return false;
}

bool WillyGame::on_key_press(GdkEventKey* event) {
    std::string keyname = gdk_keyval_name(event->keyval);
    //std::cout << "Key pressed: " << keyname << std::endl;
    keys_pressed.insert(keyname);
    
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
        } else if(keyname == "Left") {
            continuous_direction = "LEFT";
            moving_continuously = true;
            willy_direction = "LEFT";
        } else if(keyname == "Right") {
            continuous_direction = "RIGHT";
            moving_continuously = true;
            willy_direction = "RIGHT";
        } else if(keyname == "Up") {
            up_pressed = true;
        } else if(keyname == "Down") {
            down_pressed = true;
        } else if(keyname == "L" || keyname == "l") {
            complete_level_nobonus();
        } else if(keyname == "F5") {
            redbg+=0.25;
            if (redbg>1.0)
            {
                redbg=0.0;
            }
        } else if(keyname == "F6") {
            greenbg+=0.25;
            if (greenbg>1.0)
            {
                greenbg=0.0;
            }
        } else if(keyname == "F7") {
            bluebg+=0.25;
            if (bluebg>1.0)
            {
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
    }  else if(current_state == GameState::HIGH_SCORE_ENTRY) {
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
    if (balls.size() < 6 && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ball_spawn).count() > delay_distribution(rng)) {
        balls.emplace_back(primary_ball_pit_pos.first, primary_ball_pit_pos.second);
        last_ball_spawn = now; // Reset spawn timer
    }
}

void WillyGame::check_collisions() {
    if(current_state != GameState::PLAYING) return;
    
    int y = willy_position.first;
    int x = willy_position.second;
    std::string current_tile = get_tile(y, x);
    
    // Check ball collisions (don't die in ballpits)
    for(const auto& ball : balls) {
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
    flash_death_screen();
    
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
    
    // Draw ALL sprite positions with blue background, even empty ones
    for(int row = 0; row < GAME_MAX_HEIGHT; row++) {
        for(int col = 0; col < GAME_MAX_WIDTH; col++) {
            int x = col * GAME_CHAR_WIDTH * scale_factor;
            int y = row * GAME_CHAR_HEIGHT * scale_factor + menubar_height; // Offset by menubar height
            
            // Paint blue background for EVERY sprite position
            cr->set_source_rgb(redbg, greenbg, bluebg);
            cr->rectangle(x, y, GAME_CHAR_WIDTH * scale_factor, GAME_CHAR_HEIGHT * scale_factor);
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
                
                int x = ball.col * GAME_CHAR_WIDTH * scale_factor;
                int y = ball.row * GAME_CHAR_HEIGHT * scale_factor + menubar_height; // Offset by menubar height
                
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
        
        int x = willy_position.second * GAME_CHAR_WIDTH * scale_factor;
        int y = willy_position.first * GAME_CHAR_HEIGHT * scale_factor + menubar_height; // Offset by menubar height
        
        std::string sprite_name = (willy_direction == "LEFT") ? "WILLY_LEFT" : "WILLY_RIGHT";
        auto sprite = sprite_loader->get_sprite(sprite_name);
        if(sprite) {
            cr->set_source(sprite, x, y);
            cr->paint();
        }
    }
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
    
    // Text data exactly like Python version
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
    
    // Get the drawing area dimensions (not the full window)
    Gtk::Allocation allocation = drawing_area.get_allocation();
    int final_width = allocation.get_width();
    int final_height = allocation.get_height() - menubar_height; // Subtract menubar height
    
    // Create a larger surface to render text at high resolution
    int large_width = final_width * 2;
    int large_height = final_height * 2;
    auto large_surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, large_width, large_height);
    auto large_ctx = Cairo::Context::create(large_surface);
    
    // Clear large surface to blue
    large_ctx->set_source_rgb(redbg, greenbg, bluebg);
    large_ctx->paint();
    
    // Use larger font size for the large surface
    int large_font_size = std::max(16, (large_height / (int)textdata.size()) - 10);
    int line_spacing = large_font_size + 8; // Add extra spacing between lines
    
    Pango::FontDescription font_desc;
    font_desc.set_family("Courier");
    font_desc.set_size(large_font_size * PANGO_SCALE);
    
    auto layout = Pango::Layout::create(large_ctx);
    layout->set_font_description(font_desc);
    
    int counter = 0;
    
    for(const auto& message : textdata) {
        if(message.empty() || (message.size() == 1 && message[0].empty())) {
            counter++;
            continue;
        }
        
        // Calculate total width of this line first
        int max_width = 0;
        for(const auto& message2 : message) {
            auto sprite = sprite_loader->get_sprite(message2);
            if(sprite && (message2 == "WILLY_RIGHT" || message2 == "WILLY_LEFT" || 
                         message2 == "LADDER" || message2 == "UPSPRING" || 
                         message2 == "SIDESPRING" || message2 == "PRESENT" || 
                         message2 == "BELL" || message2 == "TACK" || message2 == "BALL")) {
                max_width += (GAME_CHAR_WIDTH * scale_factor) * 2; // Scale up for large surface
            } else {
                layout->set_text(message2);
                int text_width, text_height;
                layout->get_pixel_size(text_width, text_height);
                max_width += text_width;
            }
        }
        
        // Now draw the line centered on large surface
        int currentpos = (large_width - max_width) / 2;
        
        for(const auto& message2 : message) {
            if(message2.empty()) continue;
            
            auto sprite = sprite_loader->get_sprite(message2);
            if(sprite && (message2 == "WILLY_RIGHT" || message2 == "WILLY_LEFT" || 
                         message2 == "LADDER" || message2 == "UPSPRING" || 
                         message2 == "SIDESPRING" || message2 == "PRESENT" || 
                         message2 == "BELL" || message2 == "TACK" || message2 == "BALL")) {
                // Scale up sprite for large surface
                large_ctx->save();
                large_ctx->translate(currentpos, line_spacing * counter);
                large_ctx->scale(2.0, 2.0); // Scale up sprites
                large_ctx->set_source(sprite, 0, 0);
                large_ctx->paint();
                large_ctx->restore();
                currentpos += (GAME_CHAR_WIDTH * scale_factor) * 2;
            } else {
                // Draw text on large surface
                large_ctx->set_source_rgb(1.0, 1.0, 1.0); // White text
                layout->set_text(message2);
                
                int text_width, text_height;
                layout->get_pixel_size(text_width, text_height);
                
                large_ctx->move_to(currentpos, line_spacing * counter);
                layout->show_in_cairo_context(large_ctx);
                currentpos += text_width;
            }
        }
        
        counter++;
    }
    
    // Now scale down the large surface to the final size for smooth anti-aliasing
    // Offset by menubar height
    cr->save();
    cr->translate(0, menubar_height);
    cr->scale(0.5, 0.5);
    cr->set_source(large_surface, 0, 0);
    cr->paint();
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

int main(int argc, char* argv[]) {
    auto app = WillyApplication::create();
    return app->run(argc, argv);
}
