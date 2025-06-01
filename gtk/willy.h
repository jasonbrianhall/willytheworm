#ifndef WILLY_H
#define WILLY_H

#include <gtk/gtk.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <glib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <pthread.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Forward declarations
typedef struct _WillyGame WillyGame;
typedef struct _WillyEditor WillyEditor;

// Global variables to store command line options
typedef struct {
    int starting_level;
    char levels_file[256];
    int number_of_balls;
    bool use_wasd;
    bool disable_flash;
    int fps;
    bool mouse_support;
    bool sound_enabled;
    int scale_factor;
    bool show_help;
    int starting_lives;
} GameOptions;

// Sound Manager
typedef struct {
    GHashTable *sound_cache;  // filename -> Mix_Chunk*
    pthread_mutex_t sound_mutex;
    bool sound_enabled;
    bool initialized;
} SoundManager;

SoundManager* sound_manager_new(void);
void sound_manager_free(SoundManager *manager);
bool sound_manager_initialize(SoundManager *manager);
void sound_manager_cleanup(SoundManager *manager);
void sound_manager_play_sound(SoundManager *manager, const char *filename);
void sound_manager_set_sound_enabled(SoundManager *manager, bool enabled);
bool sound_manager_is_sound_enabled(SoundManager *manager);
char* sound_manager_find_sound_file(const char *filename);

// High Score Management
typedef struct {
    char name[32];
    int score;
} HighScore;

typedef struct {
    HighScore permanent_scores[10];  // All-time "Nightcrawlers"
    HighScore daily_scores[10];      // Daily "Pinworms"
} HighScoreManager;

HighScoreManager* high_score_manager_new(void);
void high_score_manager_free(HighScoreManager *manager);
void high_score_manager_load_scores(HighScoreManager *manager);
void high_score_manager_save_scores(HighScoreManager *manager);
bool high_score_manager_is_high_score(HighScoreManager *manager, int score);
bool high_score_manager_is_permanent_high_score(HighScoreManager *manager, int score);
void high_score_manager_add_score(HighScoreManager *manager, const char *name, int score);
char* high_score_manager_get_score_description(HighScoreManager *manager, int score);
char* high_score_manager_get_achievement_message(HighScoreManager *manager, int score);
char* high_score_manager_get_score_file_path(void);
bool high_score_manager_is_new_day(const char *file_path);
void high_score_manager_parse_scores_json(HighScoreManager *manager, const char *json_content);
HighScore high_score_manager_parse_score_line(const char *line);

// Game constants
#define GAME_CHAR_WIDTH 8
#define GAME_CHAR_HEIGHT 8
#define GAME_SCREEN_WIDTH 40
#define GAME_SCREEN_HEIGHT 25
#define GAME_MAX_WIDTH 40
#define GAME_MAX_HEIGHT 26
#define GAME_NEWLIFEPOINTS 2000

typedef enum {
    GAME_STATE_INTRO,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER,
    GAME_STATE_PAUSED,
    GAME_STATE_HIGH_SCORE_ENTRY,
    GAME_STATE_HIGH_SCORE_DISPLAY
} GameState;

typedef struct {
    int row;
    int col;
    char direction[16];
} Ball;

Ball* ball_new(int r, int c);
void ball_free(Ball *ball);

// Level Loader
typedef struct {
    // Level data structure: level_name -> row -> col -> tile_type
    GHashTable *level_data;        // char* -> GHashTable* (row_str -> GHashTable* (col_str -> char*))
    GHashTable *original_level_data; // Backup for resetting
    GHashTable *ball_pit_data;     // pit_name -> GHashTable* (pit_type -> coordinates)
} LevelLoader;

LevelLoader* level_loader_new(void);
void level_loader_free(LevelLoader *loader);
bool level_loader_load_levels(LevelLoader *loader, const char *filename);
void level_loader_create_default_levels(LevelLoader *loader);
char* level_loader_find_levels_file(const char *filename);
void level_loader_reset_levels(LevelLoader *loader);
int level_loader_get_max_levels(LevelLoader *loader);
bool level_loader_level_exists(LevelLoader *loader, const char *level_name);
char* level_loader_get_tile(LevelLoader *loader, const char *level_name, int row, int col);
void level_loader_set_tile(LevelLoader *loader, const char *level_name, int row, int col, const char *tile);
void level_loader_get_willy_start_position(LevelLoader *loader, const char *level_name, int *row, int *col);
void level_loader_get_ball_pit_position(LevelLoader *loader, const char *level_name, int *row, int *col);
bool level_loader_save_levels(LevelLoader *loader, const char *filename);
char* level_loader_trim(const char *str);
char* level_loader_extract_string_value(const char *line);
int* level_loader_extract_array_value(const char *line, int *count);
bool level_loader_parse_json_file(LevelLoader *loader, const char *content);

// Sprite Loader
typedef struct {
    int scale_factor;
    GHashTable *sprites;        // char* -> cairo_surface_t*
    GHashTable *named_parts;    // char* -> char*
} SpriteLoader;

SpriteLoader* sprite_loader_new(int scale);
void sprite_loader_free(SpriteLoader *loader);
char* sprite_loader_find_chr_file(void);
void sprite_loader_load_sprites(SpriteLoader *loader);
void sprite_loader_load_chr_file(SpriteLoader *loader, const char *path);
void sprite_loader_load_old_format(SpriteLoader *loader, const uint8_t *data, size_t size);
cairo_surface_t* sprite_loader_create_sprite_from_bitmap(SpriteLoader *loader, const uint8_t *data, int char_index);
void sprite_loader_create_fallback_sprites(SpriteLoader *loader);
cairo_surface_t* sprite_loader_create_willy_sprite(SpriteLoader *loader, bool facing_right);
cairo_surface_t* sprite_loader_create_colored_rect(SpriteLoader *loader, double r, double g, double b);
cairo_surface_t* sprite_loader_create_ladder_sprite(SpriteLoader *loader);
cairo_surface_t* sprite_loader_create_ball_sprite(SpriteLoader *loader);
cairo_surface_t* sprite_loader_create_bell_sprite(SpriteLoader *loader);
cairo_surface_t* sprite_loader_create_tack_sprite(SpriteLoader *loader);
cairo_surface_t* sprite_loader_create_spring_sprite(SpriteLoader *loader, bool upward);
cairo_surface_t* sprite_loader_create_empty_sprite(SpriteLoader *loader);
cairo_surface_t* sprite_loader_get_sprite(SpriteLoader *loader, const char *name);

// Willy Game Structure
struct _WillyGame {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *vbox;
    GtkWidget *menubar;
    GtkWidget *status_bar;
    
    SpriteLoader *sprite_loader;
    LevelLoader *level_loader;
    HighScoreManager *score_manager;
    SoundManager *sound_manager;
    
    // Game state
    GameState current_state;
    int scale_factor;
    int level;
    int score;
    int lives;
    int bonus;
    int willy_row;
    int willy_col;
    int previous_willy_row;
    int previous_willy_col;
    char willy_direction[16];
    int willy_velocity_x;
    int willy_velocity_y;
    bool jumping;
    int fps;
    int frame_count;

    double current_scale_x;
    double current_scale_y;
    int base_game_width;
    int base_game_height;
    bool maintain_aspect_ratio;
    
    // Level data
    char current_level[32];
    GList *balls;  // List of Ball*
    
    GHashTable *keys_pressed;  // char* -> gboolean
    
    guint timer_id;
    
    char continuous_direction[16];
    bool moving_continuously;
    bool up_pressed;
    bool down_pressed;
    
    // High score entry state
    char name_input[32];
    bool mouse_button_held;
    int held_button;
    char mouse_direction[16];
    bool mouse_up_held;
    bool mouse_down_held;
    int life_adder;
    
    // Mouse dragging
    bool mouse_dragging;
    double drag_start_x, drag_start_y;
    double drag_end_x, drag_end_y;
};

// Willy Game Functions
WillyGame* willy_game_new(void);
void willy_game_free(WillyGame *game);
void willy_game_setup_ui(WillyGame *game);
void willy_game_create_menubar(WillyGame *game);
void willy_game_load_level(WillyGame *game, const char *level_name);
gboolean willy_game_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean willy_game_on_key_release(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean willy_game_on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gboolean willy_game_on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gboolean willy_game_on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data);
void willy_game_start_game(WillyGame *game);
void willy_game_jump(WillyGame *game);
char* willy_game_get_tile(WillyGame *game, int row, int col);
void willy_game_set_tile(WillyGame *game, int row, int col, const char *tile);
bool willy_game_can_move_to(WillyGame *game, int row, int col);
bool willy_game_is_on_solid_ground(WillyGame *game);
void willy_game_update_willy_movement(WillyGame *game);
void willy_game_update_balls(WillyGame *game);
void willy_game_check_collisions(WillyGame *game);
void willy_game_die(WillyGame *game);
void willy_game_complete_level(WillyGame *game);
void willy_game_complete_level_nobonus(WillyGame *game);
void willy_game_reset_level(WillyGame *game);
void willy_game_game_over(WillyGame *game);
void willy_game_new_game(WillyGame *game);
void willy_game_quit_game(WillyGame *game);
void willy_game_update_status_bar(WillyGame *game);
gboolean willy_game_game_tick(gpointer user_data);
gboolean willy_game_on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data);
void willy_game_draw_intro_screen(WillyGame *game, cairo_t *cr);
void willy_game_draw_game_screen(WillyGame *game, cairo_t *cr);
void willy_game_draw_game_over_screen(WillyGame *game, cairo_t *cr);
void willy_game_draw_high_score_entry_screen(WillyGame *game, cairo_t *cr);
void willy_game_draw_high_score_display_screen(WillyGame *game, cairo_t *cr);
void willy_game_find_ballpit_position(WillyGame *game, int *row, int *col);
bool willy_game_check_movement_collision(WillyGame *game, int old_row, int old_col, int new_row, int new_col);
void willy_game_flash_death_screen(WillyGame *game);
void willy_game_calculate_scaling_factors(WillyGame *game);
void willy_game_on_window_resize(WillyGame *game);

// Willy Editor Structure  
struct _WillyEditor {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *vbox;
    GtkWidget *menubar;
    GtkWidget *status_bar;
    
    SpriteLoader *sprite_loader;
    LevelLoader *level_loader;
    
    // Editor state
    int scale_factor;
    int current_level_num;
    char current_level[32];
    int max_levels;
    
    int sprite_iterator_index;
    char **sprite_names;
    int sprite_names_count;
    
    // Scaling factors for window resizing
    double current_scale_x;
    double current_scale_y;
    int base_game_width;
    int base_game_height;
    
    // Editor states
    enum {
        EDITOR_STATE_INTRO,
        EDITOR_STATE_EDITING
    } editor_state;
};

// Willy Editor Functions
WillyEditor* willy_editor_new(void);
void willy_editor_free(WillyEditor *editor);
void willy_editor_setup_ui(WillyEditor *editor);
void willy_editor_create_menubar(WillyEditor *editor);
gboolean willy_editor_on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data);
void willy_editor_draw_intro_screen(WillyEditor *editor, cairo_t *cr);
void willy_editor_draw_editor_screen(WillyEditor *editor, cairo_t *cr);
gboolean willy_editor_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
gboolean willy_editor_on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gboolean willy_editor_on_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data);
void willy_editor_load_level(WillyEditor *editor, int level_num);
void willy_editor_save_level(WillyEditor *editor);
void willy_editor_new_level(WillyEditor *editor);
void willy_editor_next_level(WillyEditor *editor);
void willy_editor_change_background_color(WillyEditor *editor, int color_component);
void willy_editor_update_status_bar(WillyEditor *editor);
void willy_editor_calculate_scaling_factors(WillyEditor *editor);
void willy_editor_on_window_resize(WillyEditor *editor);
void willy_editor_screen_to_grid(WillyEditor *editor, double screen_x, double screen_y, int *row, int *col);
void willy_editor_place_sprite(WillyEditor *editor, int row, int col, const char *sprite_name);
void willy_editor_remove_sprite(WillyEditor *editor, int row, int col);
void willy_editor_ensure_level_exists(WillyEditor *editor);
void willy_editor_quit_editor(WillyEditor *editor);
void willy_editor_test_level(WillyEditor *editor);

// Sprite Iterator Functions
void sprite_iterator_init(WillyEditor *editor);
void sprite_iterator_free(WillyEditor *editor);
const char* sprite_iterator_current(WillyEditor *editor);
void sprite_iterator_next(WillyEditor *editor);
void sprite_iterator_previous(WillyEditor *editor);
int sprite_iterator_size(WillyEditor *editor);

// Application Functions
int run_willy_game(const GameOptions *options);
int run_edwilly_game(const GameOptions *options);

// Global variables
extern double redbg;
extern double greenbg;
extern double bluebg;
extern GameOptions game_options;

#endif // WILLY_H
