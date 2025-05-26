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

// Forward declaration to avoid circular dependency
class LevelLoader;

// Constants - avoid naming conflicts with system headers
const int GAME_CHAR_WIDTH = 8;
const int GAME_CHAR_HEIGHT = 8;
//const int GAME_SCREEN_WIDTH = 40;
//const int GAME_SCREEN_HEIGHT = 26;
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
    
    Ball(int r = 0, int c = 0);
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
    std::unique_ptr<LevelLoader> level_loader;  // Add level loader
    
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
    
    // Level data - now using LevelLoader's data structure
    std::string current_level;
    std::vector<Ball> balls;
    
    std::set<std::string> keys_pressed;
    std::random_device rd;
    std::mt19937 gen;
    
    sigc::connection timer_connection;
    
    // Intro screen text
    std::vector<std::string> intro_text;
    
public:
    WillyGame();
    ~WillyGame();
    
private:
    std::string continuous_direction;  // For continuous movement
    bool moving_continuously;
    bool up_pressed = false;
    bool down_pressed = false;
    void setup_ui();
    void create_menubar();
    void load_level(const std::string& level_name);  // Replace init_default_level
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
