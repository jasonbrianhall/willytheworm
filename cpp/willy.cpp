#include <gtkmm.h>
#include <cairomm/cairomm.h>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <memory>
#include <chrono>
#include <thread>
#include <set>

// Constants - avoid naming conflicts with system headers
const int GAME_CHAR_WIDTH = 8;
const int GAME_CHAR_HEIGHT = 8;
const int GAME_SCREEN_WIDTH = 40;
const int GAME_SCREEN_HEIGHT = 26;
const int GAME_MAX_WIDTH = 40;
const int GAME_MAX_HEIGHT = 25;
const int GAME_NEWLIFEPOINTS = 2000;

enum class GameState {
    INTRO,
    PLAYING,
    GAME_OVER,
    PAUSED
};

struct Ball {
    int row, col;
    std::string direction;
    
    Ball(int r = 0, int c = 0) : row(r), col(c), direction("") {}
};

class SpriteLoader {
private:
    int scale_factor;
    std::map<std::string, Cairo::RefPtr<Cairo::ImageSurface>> sprites;
    std::map<std::string, std::string> named_parts;
    
public:
    SpriteLoader(int scale = 3) : scale_factor(scale) {
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
    
    std::string find_chr_file() {
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
    
    void load_sprites() {
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
    
    void load_chr_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if(!file) {
            throw std::runtime_error("Cannot open file");
        }
        
        // Try to read as old format (8x8 bitmap)
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(file_size);
        file.read(reinterpret_cast<char*>(data.data()), file_size);
        
        load_old_format(data);
    }
    
    void load_old_format(const std::vector<uint8_t>& data) {
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
    
    Cairo::RefPtr<Cairo::ImageSurface> create_sprite_from_bitmap(
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
    
    void create_fallback_sprites() {
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
    
    Cairo::RefPtr<Cairo::ImageSurface> create_willy_sprite(bool facing_right) {
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
    
    Cairo::RefPtr<Cairo::ImageSurface> create_colored_rect(double r, double g, double b) {
        int size = GAME_CHAR_WIDTH * scale_factor;
        auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
        auto ctx = Cairo::Context::create(surface);
        
        ctx->set_source_rgb(r, g, b);
        ctx->rectangle(0, 0, size, size);
        ctx->fill();
        
        return surface;
    }
    
    Cairo::RefPtr<Cairo::ImageSurface> create_ladder_sprite() {
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
    
    Cairo::RefPtr<Cairo::ImageSurface> create_ball_sprite() {
        int size = GAME_CHAR_WIDTH * scale_factor;
        auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
        auto ctx = Cairo::Context::create(surface);
        
        ctx->set_source_rgb(1.0, 0.0, 0.0); // Red
        ctx->arc(size / 2.0, size / 2.0, size / 2.0 - 1, 0, 2 * M_PI);
        ctx->fill();
        
        return surface;
    }
    
    Cairo::RefPtr<Cairo::ImageSurface> create_bell_sprite() {
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
    
    Cairo::RefPtr<Cairo::ImageSurface> create_tack_sprite() {
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
    
    Cairo::RefPtr<Cairo::ImageSurface> create_spring_sprite(bool upward) {
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
    
    Cairo::RefPtr<Cairo::ImageSurface> create_empty_sprite() {
        int size = GAME_CHAR_WIDTH * scale_factor;
        return Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
    }
    
    Cairo::RefPtr<Cairo::ImageSurface> get_sprite(const std::string& name) {
        auto it = sprites.find(name);
        if(it != sprites.end()) {
            return it->second;
        }
        return sprites["EMPTY"];
    }
};

class WillyGame : public Gtk::Window {
private:
    Gtk::DrawingArea drawing_area;
    Gtk::Box vbox;
    Gtk::MenuBar menubar;
    Gtk::Label status_bar;
    
    std::unique_ptr<SpriteLoader> sprite_loader;
    
    // Game state
    GameState current_state = GameState::INTRO;
    int scale_factor = 3;
    int level = 1;
    int score = 0;
    int lives = 5;
    int bonus = 1000;
    std::pair<int, int> willy_position = {23, 7};
    std::string willy_direction = "RIGHT";
    std::pair<int, int> willy_velocity = {0, 0};
    bool jumping = false;
    int fps = 10;
    int frame_count = 0;
    
    // Fix the level data structure - use nested maps properly
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> level_data;
    std::string current_level = "level1";
    std::vector<Ball> balls;
    
    std::set<std::string> keys_pressed;
    std::random_device rd;
    std::mt19937 gen{rd()};
    
    sigc::connection timer_connection;
    
    // Intro screen text
    std::vector<std::string> intro_text = {
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
    
public:
    WillyGame() : vbox(Gtk::ORIENTATION_VERTICAL), gen(rd()) {
        set_title("Willy the Worm - C++ GTK Edition");
        set_default_size(
            GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor,
            GAME_SCREEN_HEIGHT * GAME_CHAR_HEIGHT * scale_factor + 100
        );
        
        // Initialize sprite loader
        sprite_loader = std::make_unique<SpriteLoader>(scale_factor);
        
        // Setup UI
        setup_ui();
        
        // Start in intro state
        current_state = GameState::INTRO;
        
        // Setup timer
        timer_connection = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &WillyGame::game_tick), 1000 / fps);
        
        show_all_children();
    }
    
    ~WillyGame() {
        timer_connection.disconnect();
    }
    
private:
    void setup_ui() {
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
        vbox.pack_start(drawing_area, Gtk::PACK_EXPAND_WIDGET);
        
        // Status bar
        update_status_bar();
        vbox.pack_start(status_bar, Gtk::PACK_SHRINK);
        
        // Key events
        set_can_focus(true);
        signal_key_press_event().connect(
            sigc::mem_fun(*this, &WillyGame::on_key_press));
        signal_key_release_event().connect(
            sigc::mem_fun(*this, &WillyGame::on_key_release));
    }
    
    void create_menubar() {
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
    
    void init_default_level() {
        // Initialize empty level with proper nested structure
        for(int row = 0; row < GAME_SCREEN_HEIGHT; row++) {
            for(int col = 0; col < GAME_SCREEN_WIDTH; col++) {
                level_data[current_level][std::to_string(row)][std::to_string(col)] = "EMPTY";
            }
        }
        
        // Add some basic level elements
        // Bottom platform
        for(int col = 5; col < 35; col++) {
            level_data[current_level]["24"][std::to_string(col)] = "PIPE1";
        }
        
        // Some ladders
        for(int row = 20; row < 25; row++) {
            level_data[current_level][std::to_string(row)]["10"] = "LADDER";
            level_data[current_level][std::to_string(row)]["30"] = "LADDER";
        }
        
        // Mid platforms
        for(int col = 8; col < 15; col++) {
            level_data[current_level]["20"][std::to_string(col)] = "PIPE1";
        }
        for(int col = 25; col < 32; col++) {
            level_data[current_level]["20"][std::to_string(col)] = "PIPE1";
        }
        
        // Top platforms
        for(int col = 15; col < 25; col++) {
            level_data[current_level]["16"][std::to_string(col)] = "PIPE1";
        }
        
        // Add more ladders
        for(int row = 16; row < 21; row++) {
            level_data[current_level][std::to_string(row)]["20"] = "LADDER";
        }
        
        // Add some game elements
        level_data[current_level]["23"]["12"] = "PRESENT";
        level_data[current_level]["23"]["28"] = "PRESENT";
        level_data[current_level]["19"]["9"] = "PRESENT";
        level_data[current_level]["19"]["31"] = "PRESENT";
        level_data[current_level]["15"]["20"] = "PRESENT";
        
        level_data[current_level]["19"]["15"] = "UPSPRING";
        level_data[current_level]["19"]["25"] = "SIDESPRING";
        level_data[current_level]["15"]["17"] = "UPSPRING";
        
        level_data[current_level]["12"]["20"] = "BELL";  // Goal
        
        level_data[current_level]["23"]["18"] = "TACK";  // Danger
        level_data[current_level]["23"]["22"] = "TACK";  // Danger
        
        // Ball pit
        level_data[current_level]["24"]["20"] = "BALLPIT";
        
        // Set Willy's starting position
        willy_position = {23, 7};
        
        // Initialize balls
        balls.clear();
        for(int i = 0; i < 6; i++) {
            balls.emplace_back(24, 20);
        }
    }
    
bool on_key_press(GdkEventKey* event) {
    std::string keyname = gdk_keyval_name(event->keyval);
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
        // Fix: Check for both "Return" and "Enter"
        if(keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
            start_game();
        }
    } else if(current_state == GameState::PLAYING) {
        if(keyname == "space") {
            jump();
        }
    } else if(current_state == GameState::GAME_OVER) {
        // Fix: Check for both "Return" and "Enter"
        if(keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
            current_state = GameState::INTRO;
        }
    }
    
    return true;
}
    
    bool on_key_release(GdkEventKey* event) {
        std::string keyname = gdk_keyval_name(event->keyval);
        keys_pressed.erase(keyname);
        return true;
    }
    
    void start_game() {
        current_state = GameState::PLAYING;
        level = 1;
        score = 0;
        lives = 5;
        bonus = 1000;
        frame_count = 0;
        init_default_level();
        update_status_bar();
    }
    
    void jump() {
        int y = willy_position.first;
        int x = willy_position.second;
        
        // Can only jump if on solid ground or platform
        if(y == GAME_MAX_HEIGHT - 1 || get_tile(y + 1, x).substr(0, 4) == "PIPE") {
            jumping = true;
            willy_velocity.second = -3; // Negative for upward movement
        }
    }
    
    std::string get_tile(int row, int col) {
        if(row >= 0 && row < GAME_SCREEN_HEIGHT && col >= 0 && col < GAME_SCREEN_WIDTH) {
            auto level_it = level_data.find(current_level);
            if(level_it != level_data.end()) {
                auto row_it = level_it->second.find(std::to_string(row));
                if(row_it != level_it->second.end()) {
                    auto col_it = row_it->second.find(std::to_string(col));
                    if(col_it != row_it->second.end()) {
                        return col_it->second;
                    }
                }
            }
        }
        return "EMPTY";
    }
    
    void set_tile(int row, int col, const std::string& tile) {
        if(row >= 0 && row < GAME_SCREEN_HEIGHT && col >= 0 && col < GAME_SCREEN_WIDTH) {
            level_data[current_level][std::to_string(row)][std::to_string(col)] = tile;
        }
    }
    
    bool can_move_to(int row, int col) {
        if(row < 0 || row >= GAME_SCREEN_HEIGHT || col < 0 || col >= GAME_SCREEN_WIDTH) {
            return false;
        }
        
        std::string tile = get_tile(row, col);
        return (tile == "EMPTY" || tile == "LADDER" || tile == "PRESENT" || 
                tile == "BELL" || tile == "UPSPRING" || tile == "SIDESPRING" || 
                tile == "TACK");
    }
    
    bool is_on_solid_ground() {
        int y = willy_position.first;
        int x = willy_position.second;
        
        if(y >= GAME_MAX_HEIGHT - 1) {
            return true;
        }
        
        std::string below_tile = get_tile(y + 1, x);
        return (below_tile.substr(0, 4) == "PIPE" || below_tile == "BALLPIT");
    }
    
    void update_willy_movement() {
        if(current_state != GameState::PLAYING) return;
        
        // Handle horizontal movement
        if(keys_pressed.count("Left") || keys_pressed.count("a")) {
            willy_direction = "LEFT";
            if(can_move_to(willy_position.first, willy_position.second - 1)) {
                willy_position.second--;
            }
        } else if(keys_pressed.count("Right") || keys_pressed.count("d")) {
            willy_direction = "RIGHT";
            if(can_move_to(willy_position.first, willy_position.second + 1)) {
                willy_position.second++;
            }
        }
        
        // Handle ladder movement
        std::string current_tile = get_tile(willy_position.first, willy_position.second);
        if(current_tile == "LADDER") {
            if(keys_pressed.count("Up") || keys_pressed.count("w")) {
                if(can_move_to(willy_position.first - 1, willy_position.second)) {
                    willy_position.first--;
                    willy_velocity.second = 0; // Stop falling when on ladder
                }
            } else if(keys_pressed.count("Down") || keys_pressed.count("s")) {
                if(can_move_to(willy_position.first + 1, willy_position.second)) {
                    willy_position.first++;
                    willy_velocity.second = 0; // Stop falling when on ladder
                }
            }
        }
        
        // Handle gravity and jumping
        if(current_tile != "LADDER") {
            // Apply gravity if not on solid ground
            if(!is_on_solid_ground()) {
                willy_velocity.second += 1; // Gravity
            } else {
                if(willy_velocity.second > 0) {
                    willy_velocity.second = 0; // Stop falling when hitting ground
                    jumping = false;
                }
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
    }
    
void update_balls() {
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
    
    void check_collisions() {
        if(current_state != GameState::PLAYING) return;
        
        int y = willy_position.first;
        int x = willy_position.second;
        std::string current_tile = get_tile(y, x);
        
        // Check ball collisions
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
            // Reverse horizontal direction
            willy_direction = (willy_direction == "RIGHT") ? "LEFT" : "RIGHT";
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
    
    void die() {
        lives--;
        if(lives <= 0) {
            game_over();
        } else {
            reset_level();
        }
    }
    
    void complete_level() {
        score += bonus;
        level++;
        // For now, just reset the same level with more difficulty
        reset_level();
        // Could load new levels here if we had level files
    }
    
    void reset_level() {
        willy_position = {23, 7};
        willy_velocity = {0, 0};
        jumping = false;
        bonus = 1000;
        frame_count = 0;
        
        // Reset balls
        for(auto& ball : balls) {
            ball.row = 24;
            ball.col = 20;
            ball.direction = "";
        }
        
        // Restore presents
        init_default_level();
    }
    
    void game_over() {
        current_state = GameState::GAME_OVER;
    }
    
    void new_game() {
        current_state = GameState::INTRO;
    }
    
    void quit_game() {
        hide();
    }
    
    void update_status_bar() {
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
    
    bool game_tick() {
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
    
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
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
    
    void draw_intro_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
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
    
    void draw_game_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
        // Draw level tiles
        for(const auto& row_pair : level_data[current_level]) {
            int row = std::stoi(row_pair.first);
            for(const auto& col_pair : row_pair.second) {
                int col = std::stoi(col_pair.first);
                const std::string& tile = col_pair.second;
                
                if(tile != "EMPTY") {
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
        
        // Draw balls
        for(const auto& ball : balls) {
            if(get_tile(ball.row, ball.col) != "BALLPIT") {
                int x = ball.col * GAME_CHAR_WIDTH * scale_factor;
                int y = ball.row * GAME_CHAR_HEIGHT * scale_factor;
                auto sprite = sprite_loader->get_sprite("BALL");
                if(sprite) {
                    cr->set_source(sprite, x, y);
                    cr->paint();
                }
            }
        }
        
        // Draw Willy
        int x = willy_position.second * GAME_CHAR_WIDTH * scale_factor;
        int y = willy_position.first * GAME_CHAR_HEIGHT * scale_factor;
        std::string sprite_name = (willy_direction == "LEFT") ? "WILLY_LEFT" : "WILLY_RIGHT";
        auto sprite = sprite_loader->get_sprite(sprite_name);
        if(sprite) {
            cr->set_source(sprite, x, y);
            cr->paint();
        }
    }
    
    void draw_game_over_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
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
};

class WillyApplication : public Gtk::Application {
protected:
    WillyApplication() : Gtk::Application("org.example.willytheworm") {}
    
public:
    static Glib::RefPtr<WillyApplication> create() {
        return Glib::RefPtr<WillyApplication>(new WillyApplication());
    }

protected:
    void on_activate() override {
        auto window = new WillyGame();
        add_window(*window);
        window->present();
    }
};

int main(int argc, char* argv[]) {
    auto app = WillyApplication::create();
    return app->run(argc, argv);
}
