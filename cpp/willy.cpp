#include "willy.h"
#include "loadlevels.h"
#include "loadlevels.h"

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
        ctx->set_source_rgb(0.0, 0.0, 1.0); // Blue
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

// WillyGame implementation
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
    gen(rd()) {
    
    set_title("Willy the Worm - C++ GTK Edition");
    set_default_size(
        GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor,
        GAME_SCREEN_HEIGHT * GAME_CHAR_HEIGHT * scale_factor + 100
    );
    
    // Initialize sprite loader and level loader
    sprite_loader = std::make_unique<SpriteLoader>(scale_factor);
    level_loader = std::make_unique<LevelLoader>();
    
    // Load levels from file or create defaults
    level_loader->load_levels("levels.json");
    
    // Initialize intro text
    intro_text = {
        "Willy the Worm",
        "",
        "By Jason Hall",
        "(original version by Alan Farmer 1985)",
        "",
        "This code is Free Open Source Software (FOSS)",
        "Please feel free to do with it whatever you wish.",
        "",
        "If you do make changes though such as new levels,",
        "please share them with the world.",
        "",
        "",
        "Meet Willy the Worm. Willy is a fun-",
        "loving invertebrate who likes to climb",
        "ladders, bounce on springs",
        "and find his presents. But more",
        "than anything, Willy loves to ring",
        "bells!",
        "",
        "You can press the arrow keys ← ↑ → ↓",
        "to make Willy run and climb, or the",
        "space bar to make him jump. Anything",
        "else will make Willy stop and wait",
        "",
        "Good luck, and don't let Willy step on",
        "a tack or get ran over by a ball!",
        "",
        "Press Enter to Continue"
    };
    
    // Setup UI
    setup_ui();
    
    // Start in intro state
    current_state = GameState::INTRO;
    
    // Setup timer
    timer_connection = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &WillyGame::game_tick), 1000 / fps);
    
    show_all_children();
    
    // Give focus to the drawing area so it can receive keyboard events
    drawing_area.grab_focus();
}

WillyGame::~WillyGame() {
    timer_connection.disconnect();
}

void WillyGame::setup_ui() {
    add(vbox);
    
    // Create menu
    create_menubar();
    vbox.pack_start(menubar, Gtk::PACK_SHRINK);
    
    // Setup drawing area
    drawing_area.set_size_request(
        GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor,
        GAME_SCREEN_HEIGHT * GAME_CHAR_HEIGHT * scale_factor
    );
    drawing_area.signal_draw().connect(
        sigc::mem_fun(*this, &WillyGame::on_draw));
    
    // Make drawing area focusable and connect key events to it
    drawing_area.set_can_focus(true);
    drawing_area.add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    drawing_area.signal_key_press_event().connect(
        sigc::mem_fun(*this, &WillyGame::on_key_press));
    drawing_area.signal_key_release_event().connect(
        sigc::mem_fun(*this, &WillyGame::on_key_release));
    
    vbox.pack_start(drawing_area, Gtk::PACK_EXPAND_WIDGET);
    
    // Status bar
    update_status_bar();
    vbox.pack_start(status_bar, Gtk::PACK_SHRINK);
}

void WillyGame::create_menubar() {
    auto game_menu = Gtk::manage(new Gtk::Menu());
    
    auto new_game_item = Gtk::manage(new Gtk::MenuItem("New Game"));
    new_game_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyGame::new_game));
    game_menu->append(*new_game_item);
    
    auto quit_item = Gtk::manage(new Gtk::MenuItem("Quit"));
    quit_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyGame::quit_game));
    game_menu->append(*quit_item);
    
    auto game_item = Gtk::manage(new Gtk::MenuItem("Game"));
    game_item->set_submenu(*game_menu);
    menubar.append(*game_item);
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
    std::pair<int, int> ball_pit_pos = level_loader->get_ball_pit_position(level_name);
    for(int i = 0; i < 6; i++) {
        balls.emplace_back(ball_pit_pos.first, ball_pit_pos.second);
    }
    
    std::cout << "Loaded level: " << level_name << std::endl;
    std::cout << "Willy starts at: (" << willy_position.first << ", " << willy_position.second << ")" << std::endl;
    std::cout << "Ball pit at: (" << ball_pit_pos.first << ", " << ball_pit_pos.second << ")" << std::endl;
}

bool WillyGame::on_key_press(GdkEventKey* event) {
    std::string keyname = gdk_keyval_name(event->keyval);
    std::cout << "Key pressed: " << keyname << std::endl;
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
            // Don't stop horizontal movement - let update_willy_movement handle it
        } else if(keyname == "Down") {
            down_pressed = true;
            // Don't stop horizontal movement - let update_willy_movement handle it
        } else {
            // Any other key stops movement
            moving_continuously = false;
            continuous_direction = "";
        }
    } else if(current_state == GameState::GAME_OVER) {
        if(keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
            current_state = GameState::INTRO;
        }
    }
    
    return true;
}

bool WillyGame::on_key_release(GdkEventKey* event) {
    std::string keyname = gdk_keyval_name(event->keyval);
    keys_pressed.erase(keyname);
    
    // Track up/down key releases
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
    
    // Load the first level using the level loader
    load_level("level1");
    
    update_status_bar();
}

void WillyGame::jump() {
    int y = willy_position.first;
    int x = willy_position.second;
    
    // Can only jump if on solid ground or platform
    if(y == GAME_MAX_HEIGHT - 1 || get_tile(y + 1, x).substr(0, 4) == "PIPE") {
        jumping = true;
        willy_velocity.second = -5; // Increased from -3 to -4 for higher jump
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
            tile == "TACK" || tile == "BALLPIT");  // Added BALLPIT
}

bool WillyGame::is_on_solid_ground() {
    int y = willy_position.first;
    int x = willy_position.second;
    
    if(y >= GAME_MAX_HEIGHT - 1) {
        return true;
    }
    
    std::string current_tile = get_tile(y, x);
    std::string below_tile = get_tile(y + 1, x);
    
    // Standing on top of a ladder is solid ground
    if(current_tile == "LADDER" && below_tile != "LADDER") {
        return true;
    }
    
    return (below_tile.substr(0, 4) == "PIPE" || below_tile == "BALLPIT");
}

void WillyGame::update_willy_movement() {
    if(current_state != GameState::PLAYING) return;
    
    std::string current_tile = get_tile(willy_position.first, willy_position.second);
    bool on_ladder = (current_tile == "LADDER");
    
    // Handle ladder movement when on a ladder AND up/down was pressed
    bool moved_on_ladder = false;
    if(on_ladder) {
        if(up_pressed) {
            int target_row = willy_position.first - 1;
            if(target_row >= 0) {
                std::string above_tile = get_tile(target_row, willy_position.second);
                // Can move up if there's a ladder above OR if it's a valid tile to move to
                // But DON'T move above the top of a ladder sequence
                if(above_tile == "LADDER") {
                    // There's a ladder above, safe to move up
                    if(can_move_to(target_row, willy_position.second)) {
                        willy_position.first--;
                        willy_velocity.second = 0;
                        moved_on_ladder = true;
                        // Stop horizontal movement when actively climbing
                        moving_continuously = false;
                        continuous_direction = "";
                    }
                } else if(can_move_to(target_row, willy_position.second) && 
                         (above_tile == "EMPTY" || above_tile == "PRESENT" || 
                          above_tile == "BELL" || above_tile == "UPSPRING" || 
                          above_tile == "SIDESPRING" || above_tile == "TACK" || 
                          above_tile == "BALLPIT")) {
                    // Can move to this tile, but this ends ladder climbing
                    willy_position.first--;
                    willy_velocity.second = 0;
                    moved_on_ladder = true;
                    // Stop horizontal movement when leaving ladder
                    moving_continuously = false;
                    continuous_direction = "";
                    // Clear up_pressed since we're leaving the ladder
                    up_pressed = false;
                }
                // If above_tile is a wall/pipe, don't move up at all
            }
        } else if(down_pressed) {
            int target_row = willy_position.first + 1;
            if(target_row < GAME_SCREEN_HEIGHT && can_move_to(target_row, willy_position.second)) {
                willy_position.first++;
                willy_velocity.second = 0;
                moved_on_ladder = true;
                // Stop horizontal movement when actively climbing
                moving_continuously = false;
                continuous_direction = "";
            }
        }
    }
    
    // Handle horizontal movement (only if we didn't just move on ladder)
    if(!moved_on_ladder) {
        if(moving_continuously && !continuous_direction.empty()) {
            bool hit_obstacle = false;
            
            if(continuous_direction == "LEFT") {
                if(can_move_to(willy_position.first, willy_position.second - 1)) {
                    willy_position.second--;
                } else {
                    hit_obstacle = true;
                }
            } else if(continuous_direction == "RIGHT") {
                if(can_move_to(willy_position.first, willy_position.second + 1)) {
                    willy_position.second++;
                } else {
                    hit_obstacle = true;
                }
            }
            
            // If we hit an obstacle, stop continuous movement
            if(hit_obstacle) {
                moving_continuously = false;
                continuous_direction = "";
            }
            
            // Check if we just moved onto a ladder and up/down is pressed
            std::string new_tile = get_tile(willy_position.first, willy_position.second);
            if(new_tile == "LADDER" && (up_pressed || down_pressed)) {
                // We just hit a ladder and want to climb - this will be handled next frame
                // The movement logic above will handle stopping horizontal movement
            }
        }
        // Handle discrete movement (when not moving continuously)
        else if(!moving_continuously) {
            if(keys_pressed.count("Left")) {
                willy_direction = "LEFT";
                if(can_move_to(willy_position.first, willy_position.second - 1)) {
                    willy_position.second--;
                }
            } else if(keys_pressed.count("Right")) {
                willy_direction = "RIGHT";
                if(can_move_to(willy_position.first, willy_position.second + 1)) {
                    willy_position.second++;
                }
            }
        }
    }
    
    // Handle gravity and jumping (but NOT when on a ladder)
    if(!on_ladder) {
        // Apply gravity if not on solid ground
        if(!is_on_solid_ground()) {
            willy_velocity.second += 1; // Gravity
        } else {
            if(willy_velocity.second > 0) {
                willy_velocity.second = 0; // Stop falling when hitting ground
                jumping = false;
            }
        }
        
        // Apply vertical velocity
        if(willy_velocity.second != 0) {
            int new_y = willy_position.first + (willy_velocity.second > 0 ? 1 : -1);
            if(can_move_to(new_y, willy_position.second)) {
                willy_position.first = new_y;
            }
            
            // Reduce upward velocity
            if(willy_velocity.second < 0) {
                willy_velocity.second++;
            }
        }
    } else {
        // On ladder - stop all vertical velocity and jumping
        willy_velocity.second = 0;
        jumping = false;
    }
}

void WillyGame::update_balls() {
    if(current_state != GameState::PLAYING) return;
    
    for(auto& ball : balls) {
        // Check if ball is in ball pit
        if(get_tile(ball.row, ball.col) == "BALLPIT") {
            ball.direction = "";
            continue;
        }
        
        // Apply gravity to balls
        if(ball.row < GAME_MAX_HEIGHT - 1 && get_tile(ball.row + 1, ball.col).substr(0, 4) != "PIPE") {
            ball.row++;
            ball.direction = "";
        } else {
            // Ball is on a platform, move horizontally
            if(ball.direction.empty()) {
                std::uniform_real_distribution<> dis(0.0, 1.0);
                ball.direction = (dis(gen) > 0.5) ? "RIGHT" : "LEFT";
            }
            
            if(ball.direction == "RIGHT") {
                if(ball.col + 1 < GAME_MAX_WIDTH && 
                   get_tile(ball.row, ball.col + 1).substr(0, 4) != "PIPE") {
                    ball.col++;
                } else {
                    ball.direction = "LEFT";
                }
            } else { // LEFT
                if(ball.col - 1 >= 0 && 
                   get_tile(ball.row, ball.col - 1).substr(0, 4) != "PIPE") {
                    ball.col--;
                } else {
                    ball.direction = "RIGHT";
                }
            }
        }
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
            die();
            return;
        }
    }
    
    // Check tile interactions
    if(current_tile == "TACK") {
        die();
    } else if(current_tile == "BELL") {
        complete_level();
    } else if(current_tile == "PRESENT") {
        score += 100;
        set_tile(y, x, "EMPTY");
    } else if(current_tile == "UPSPRING") {
        if(!jumping) {
            jump();
        }
    } else if(current_tile == "SIDESPRING") {
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
                        break;
                    }
                }
            }
        }
    }
}

void WillyGame::die() {
    lives--;
    if(lives <= 0) {
        game_over();
    } else {
        reset_level();
    }
}

void WillyGame::complete_level() {
    score += bonus;
    level++;
    
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
    // Reset to original level state
    level_loader->reset_levels();
    
    // Reload current level
    load_level(current_level);
    
    // Reset game state
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
    current_state = GameState::GAME_OVER;
}

void WillyGame::new_game() {
    current_state = GameState::INTRO;
}

void WillyGame::quit_game() {
    hide();
}

void WillyGame::update_status_bar() {
    if(current_state == GameState::PLAYING) {
        std::string status_text = "Score: " + std::to_string(score) + 
                                " | Bonus: " + std::to_string(bonus) + 
                                " | Level: " + std::to_string(level) + 
                                " | Lives: " + std::to_string(lives);
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
    }
    
    update_status_bar();
    drawing_area.queue_draw();
    return true; // Continue the timer
}

bool WillyGame::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Clear background
    cr->set_source_rgb(0.0, 0.0, 1.0); // Blue background
    cr->paint();
    
    if(current_state == GameState::INTRO) {
        draw_intro_screen(cr);
    } else if(current_state == GameState::PLAYING) {
        draw_game_screen(cr);
    } else if(current_state == GameState::GAME_OVER) {
        draw_game_over_screen(cr);
    }
    
    return true;
}

void WillyGame::draw_intro_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Calculate font size based on screen height
    int line_height = (GAME_SCREEN_HEIGHT * GAME_CHAR_HEIGHT * scale_factor) / intro_text.size();
    int font_size = std::max(12, line_height - 4);
    
    // Create font
    Pango::FontDescription font_desc;
    font_desc.set_family("Monospace");
    font_desc.set_size(font_size * PANGO_SCALE);
    
    auto layout = create_pango_layout("");
    layout->set_font_description(font_desc);
    
    cr->set_source_rgb(1.0, 1.0, 1.0); // White text
    
    int y_offset = 20;
    for(const auto& line : intro_text) {
        layout->set_text(line);
        
        int text_width, text_height;
        layout->get_pixel_size(text_width, text_height);
        
        int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
        
        cr->move_to(x_offset, y_offset);
        layout->show_in_cairo_context(cr);
        
        y_offset += line_height;
    }
}

void WillyGame::draw_game_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Get level data from level loader
    auto level_data = level_loader->get_level_data();
    auto current_level_data = level_data.find(current_level);
    
    if(current_level_data != level_data.end()) {
        // Draw level tiles EXCEPT where Willy is positioned
        for(const auto& row_pair : current_level_data->second) {
            int row = std::stoi(row_pair.first);
            for(const auto& col_pair : row_pair.second) {
                int col = std::stoi(col_pair.first);
                const std::string& tile = col_pair.second;
                
                // Don't render anything where Willy is positioned
                if(row == willy_position.first && col == willy_position.second) {
                    continue;
                }
                
                if(tile != "EMPTY" && tile.find("WILLY") == std::string::npos) {
                    int x = col * GAME_CHAR_WIDTH * scale_factor;
                    int y = row * GAME_CHAR_HEIGHT * scale_factor;
                    auto sprite = sprite_loader->get_sprite(tile);
                    if(sprite) {
                        cr->set_source(sprite, x, y);
                        cr->paint();
                    }
                }
            }
        }
    }
    
    // Draw balls (but not where Willy is)
    for(const auto& ball : balls) {
        if(get_tile(ball.row, ball.col) != "BALLPIT" && 
           !(ball.row == willy_position.first && ball.col == willy_position.second)) {
            int x = ball.col * GAME_CHAR_WIDTH * scale_factor;
            int y = ball.row * GAME_CHAR_HEIGHT * scale_factor;
            auto sprite = sprite_loader->get_sprite("BALL");
            if(sprite) {
                cr->set_source(sprite, x, y);
                cr->paint();
            }
        }
    }
    
    // Draw Willy LAST so he appears on top of everything
    int x = willy_position.second * GAME_CHAR_WIDTH * scale_factor;
    int y = willy_position.first * GAME_CHAR_HEIGHT * scale_factor;
    std::string sprite_name = (willy_direction == "LEFT") ? "WILLY_LEFT" : "WILLY_RIGHT";
    auto sprite = sprite_loader->get_sprite(sprite_name);
    if(sprite) {
        cr->set_source(sprite, x, y);
        cr->paint();
    }
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

void WillyApplication::on_activate() {
    auto window = new WillyGame();
    add_window(*window);
    window->present();
}

int main(int argc, char* argv[]) {
    auto app = WillyApplication::create();
    return app->run(argc, argv);
}
