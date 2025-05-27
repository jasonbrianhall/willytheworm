#ifndef WILLY_H
#define WILLY_H

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
#include <utility>
#include <sstream>
#include <algorithm>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <map>
#include <string>
#include <thread>
#include <mutex>

// Global variables to store command line options (add these near the top of willy.cpp)
struct GameOptions {
    int starting_level = 1;
    std::string levels_file = "levels.json";
    int number_of_balls = 6;
    bool use_wasd = false;
    bool disable_flash = false;
    int fps = 10;
    bool mouse_support = false;
    bool sound_enabled = true;
    int scale_factor = 3;
    bool show_help = false;
};

class SoundManager {
private:
    std::map<std::string, Mix_Chunk*> sound_cache;
    std::mutex sound_mutex;
    bool sound_enabled;
    bool initialized;
    
    std::string find_sound_file(const std::string& filename);
    
public:
    SoundManager();
    ~SoundManager();
    
    bool initialize();
    void cleanup();
    void play_sound(const std::string& filename);
    void set_sound_enabled(bool enabled) { sound_enabled = enabled; }
    bool is_sound_enabled() const { return sound_enabled; }
};

// High Score Management
struct HighScore {
    std::string name;
    int score;
};

class HighScoreManager {
private:
    std::vector<HighScore> permanent_scores;  // All-time "Nightcrawlers"
    std::vector<HighScore> daily_scores;      // Daily "Pinworms"
    
    std::string get_score_file_path();
    bool is_new_day(const std::string& file_path);
    void parse_scores_json(const std::string& json_content);
    HighScore parse_score_line(const std::string& line);
    
public:
    HighScoreManager();
    ~HighScoreManager();
    
    void load_scores();
    void save_scores();
    bool is_high_score(int score);
    bool is_permanent_high_score(int score);
    void add_score(const std::string& name, int score);
    std::string get_score_description(int score);
    std::string get_achievement_message(int score);
    std::vector<HighScore> get_permanent_scores() const;
    std::vector<HighScore> get_daily_scores() const;
};

// Game constants
const int GAME_CHAR_WIDTH = 8;
const int GAME_CHAR_HEIGHT = 8;
const int GAME_SCREEN_WIDTH = 40;
const int GAME_SCREEN_HEIGHT = 25;
const int GAME_MAX_WIDTH = 40;
const int GAME_MAX_HEIGHT = 26;
const int GAME_NEWLIFEPOINTS = 2000;

enum class GameState {
    INTRO,
    PLAYING,
    GAME_OVER,
    PAUSED,
    HIGH_SCORE_ENTRY,
    HIGH_SCORE_DISPLAY
};

struct Ball {
    int row, col;
    std::string direction;
    
    Ball(int r = 0, int c = 0);
};

class LevelLoader {
private:
    // Level data structure: level_name -> row -> col -> tile_type
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> level_data;
    
    // Original level data for resetting
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> original_level_data;
    
    // Ball pit data: pit_name -> pit_type -> coordinates
    std::map<std::string, std::map<std::string, std::pair<int, int>>> ball_pit_data;
    
    // Simple JSON parsing helpers
    std::string trim(const std::string& str);
    std::string extract_string_value(const std::string& line);
    std::vector<int> extract_array_value(const std::string& line);
    bool parse_json_file(const std::string& content);
    
public:
    LevelLoader();
    ~LevelLoader();
    
    // Load levels from JSON file
    bool load_levels(const std::string& filename = "levels.json");
    
    // Create default levels if file not found
    void create_default_levels();
    
    // Find levels file in various locations
    std::string find_levels_file(const std::string& filename);
    
    // Get level data
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> get_level_data() const;
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> get_original_level_data() const;
    std::map<std::string, std::map<std::string, std::pair<int, int>>> get_ball_pit_data() const;
    
    // Reset levels to original state
    void reset_levels();
    
    // Utility functions
    int get_max_levels() const;
    bool level_exists(const std::string& level_name) const;
    
    // Tile operations
    std::string get_tile(const std::string& level_name, int row, int col) const;
    void set_tile(const std::string& level_name, int row, int col, const std::string& tile);
    
    // Get special positions
    std::pair<int, int> get_willy_start_position(const std::string& level_name) const;
    std::pair<int, int> get_ball_pit_position(const std::string& level_name) const;
    
    // Save levels to JSON file
    bool save_levels(const std::string& filename) const;
};

class SpriteLoader {
private:
    int scale_factor;
    std::map<std::string, Cairo::RefPtr<Cairo::ImageSurface>> sprites;
    std::map<std::string, std::string> named_parts;
    
public:
    explicit SpriteLoader(int scale = 3);
    
    std::string find_chr_file();
    void load_sprites();
    void load_chr_file(const std::string& path);
    void load_old_format(const std::vector<uint8_t>& data);
    Cairo::RefPtr<Cairo::ImageSurface> create_sprite_from_bitmap(
        const std::vector<uint8_t>& data, int char_index);
    void create_fallback_sprites();
    Cairo::RefPtr<Cairo::ImageSurface> create_willy_sprite(bool facing_right);
    Cairo::RefPtr<Cairo::ImageSurface> create_colored_rect(double r, double g, double b);
    Cairo::RefPtr<Cairo::ImageSurface> create_ladder_sprite();
    Cairo::RefPtr<Cairo::ImageSurface> create_ball_sprite();
    Cairo::RefPtr<Cairo::ImageSurface> create_bell_sprite();
    Cairo::RefPtr<Cairo::ImageSurface> create_tack_sprite();
    Cairo::RefPtr<Cairo::ImageSurface> create_spring_sprite(bool upward);
    Cairo::RefPtr<Cairo::ImageSurface> create_empty_sprite();
    Cairo::RefPtr<Cairo::ImageSurface> get_sprite(const std::string& name);
};

class WillyGame : public Gtk::Window {
private:
    Gtk::DrawingArea drawing_area;
    Gtk::Box vbox;
    Gtk::MenuBar menubar;
    Gtk::Label status_bar;
    
    std::unique_ptr<SpriteLoader> sprite_loader;
    std::unique_ptr<LevelLoader> level_loader;
    std::unique_ptr<HighScoreManager> score_manager;
    std::pair<int, int> previous_willy_position;  // Track where Willy was last frame
    // Game state
    GameState current_state;
    int scale_factor;
    int level;
    int score;
    int lives;
    int bonus;
    std::pair<int, int> willy_position;
    std::string willy_direction;
    std::pair<int, int> willy_velocity;
    bool jumping;
    int fps;
    int frame_count;

    double current_scale_x = 1.0;
    double current_scale_y = 1.0;
    int base_game_width;
    int base_game_height;
    bool maintain_aspect_ratio = true;
    // Level data - now using LevelLoader's data structure
    std::string current_level;
    std::vector<Ball> balls;
    
    std::set<std::string> keys_pressed;
    std::random_device rd;
    std::mt19937 gen;
    
    sigc::connection timer_connection;
    
    std::string continuous_direction;  // For continuous movement
    bool moving_continuously;
    bool up_pressed = false;
    bool down_pressed = false;
    
    // High score entry state
    std::string name_input;
    
public:
    WillyGame();
    ~WillyGame();
    void on_window_resize();
    void calculate_scaling_factors();

    
private:


    void setup_ui();
    void create_menubar();
    void load_level(const std::string& level_name);
    bool on_key_press(GdkEventKey* event);
    bool on_key_release(GdkEventKey* event);
    void start_game();
    void jump();
    std::string get_tile(int row, int col);
    void set_tile(int row, int col, const std::string& tile);
    bool can_move_to(int row, int col);
    bool is_on_solid_ground();
    void update_willy_movement();
    void update_balls();
    void check_collisions();
    void die();
    void complete_level();
    void complete_level_nobonus();
    void reset_level();
    void game_over();
    void new_game();
    void quit_game();
    void update_status_bar();
    bool game_tick();
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
    void draw_intro_screen(const Cairo::RefPtr<Cairo::Context>& cr);
    void draw_game_screen(const Cairo::RefPtr<Cairo::Context>& cr);
    void draw_game_over_screen(const Cairo::RefPtr<Cairo::Context>& cr);
    void draw_high_score_entry_screen(const Cairo::RefPtr<Cairo::Context>& cr);
    void draw_high_score_display_screen(const Cairo::RefPtr<Cairo::Context>& cr);
    std::pair<int, int> find_ballpit_position();
    bool check_movement_collision(int old_row, int old_col, int new_row, int new_col);
    std::unique_ptr<SoundManager> sound_manager;
    void flash_death_screen();
    void flash_death_screen_seizure();
};

class WillyApplication : public Gtk::Application {
protected:
    WillyApplication();
    
public:
    static Glib::RefPtr<WillyApplication> create();

protected:
    void on_activate() override;
};

#endif // WILLY_H
