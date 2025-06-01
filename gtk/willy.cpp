#include "willy.h"
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

double redbg = 0.0;
double greenbg = 0.0;
double bluebg = 1.0;
GameOptions game_options = {
    .starting_level = 1,
    .levels_file = "levels.json",
    .number_of_balls = 6,
    .use_wasd = false,
    .disable_flash = false,
    .fps = 10,
    .mouse_support = false,
    .sound_enabled = true,
    .scale_factor = 3,
    .show_help = false,
    .starting_lives = 5
};

// Ball implementation
Ball* ball_new(int r, int c) {
    Ball *ball = g_malloc(sizeof(Ball));
    ball->row = r;
    ball->col = c;
    strcpy(ball->direction, "");
    return ball;
}

void ball_free(Ball *ball) {
    if (ball) {
        g_free(ball);
    }
}

// WillyGame implementation
WillyGame* willy_game_new(void) {
    WillyGame *game = g_malloc0(sizeof(WillyGame));
    
    // Initialize basic state
    game->current_state = GAME_STATE_INTRO;
    game->scale_factor = game_options.scale_factor;
    game->level = game_options.starting_level;
    game->score = 0;
    game->lives = 5;
    game->bonus = 1000;
    game->willy_row = 23;
    game->willy_col = 7;
    game->previous_willy_row = 23;
    game->previous_willy_col = 7;
    strcpy(game->willy_direction, "RIGHT");
    game->willy_velocity_x = 0;
    game->willy_velocity_y = 0;
    game->jumping = false;
    game->fps = game_options.fps;
    game->frame_count = 0;
    strcpy(game->current_level, "level1");
    strcpy(game->continuous_direction, "");
    game->moving_continuously = false;
    game->mouse_button_held = false;
    game->held_button = 0;
    strcpy(game->mouse_direction, "");
    game->mouse_up_held = false;
    game->mouse_down_held = false;
    game->up_pressed = false;
    game->down_pressed = false;
    game->life_adder = 0;
    
    // Initialize scaling
    game->current_scale_x = 1.0;
    game->current_scale_y = 1.0;
    game->maintain_aspect_ratio = true;
    
    // Create window
    game->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(game->window), "Willy the Worm - C GTK Edition");
    gtk_window_set_decorated(GTK_WINDOW(game->window), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(game->window), FALSE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(game->window), FALSE);
    gtk_window_set_position(GTK_WINDOW(game->window), GTK_WIN_POS_CENTER);
    gtk_window_set_deletable(GTK_WINDOW(game->window), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(game->window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(game->window), GDK_WINDOW_TYPE_HINT_NORMAL);
    
    // Initialize components
    game->score_manager = high_score_manager_new();
    game->sprite_loader = sprite_loader_new(game->scale_factor);
    game->level_loader = level_loader_new();
    game->sound_manager = sound_manager_new();
    
    // Initialize sound system
    if (!sound_manager_initialize(game->sound_manager)) {
        printf("Warning: Sound system initialization failed\n");
    }
    sound_manager_set_sound_enabled(game->sound_manager, game_options.sound_enabled);
    
    // Load levels file
    if (!level_loader_load_levels(game->level_loader, game_options.levels_file)) {
        printf("Warning: Failed to load %s, trying default levels.json\n", game_options.levels_file);
        level_loader_load_levels(game->level_loader, "levels.json");
    }
    
    // Initialize hash table for pressed keys
    game->keys_pressed = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    // Initialize balls list
    game->balls = NULL;
    
    willy_game_setup_ui(game);
    
    // Store base game dimensions
    game->base_game_width = GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor;
    game->base_game_height = (GAME_SCREEN_HEIGHT + 2) * GAME_CHAR_HEIGHT * game->scale_factor;
    
    // Calculate proper window size and set default size
    GtkRequisition menubar_req;
    gtk_widget_get_preferred_size(game->menubar, &menubar_req, NULL);
    
    GtkRequisition statusbar_req;
    gtk_widget_get_preferred_size(game->status_bar, &statusbar_req, NULL);
    
    int total_height = game->base_game_height + menubar_req.height + statusbar_req.height + 10;
    
    gtk_window_set_default_size(GTK_WINDOW(game->window), game->base_game_width, total_height);
    gtk_window_resize(GTK_WINDOW(game->window), game->base_game_width, total_height);
    
    // Connect signals
    g_signal_connect(game->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(game->window, "size-allocate", G_CALLBACK(willy_game_on_window_resize), game);
    
    // Initial scaling calculation
    willy_game_calculate_scaling_factors(game);
    
    gtk_widget_grab_focus(game->drawing_area);
    
    // Set up timer
    game->timer_id = g_timeout_add(1000 / game->fps, willy_game_game_tick, game);
    
    gtk_widget_show_all(game->window);
    
    // Print command line options being used
    printf("Game initialized with options:\n");
    printf("  Starting level: %d\n", game->level);
    printf("  Levels file: %s\n", game_options.levels_file);
    printf("  Number of balls: %d\n", game_options.number_of_balls);
    printf("  FPS: %d\n", game->fps);
    printf("  Scale factor: %d\n", game->scale_factor);
    printf("  Sound enabled: %s\n", sound_manager_is_sound_enabled(game->sound_manager) ? "Yes" : "No");
    if (game_options.use_wasd) printf("  WASD controls: Enabled\n");
    if (game_options.disable_flash) printf("  Death flash: Disabled\n");
    if (game_options.mouse_support) printf("  Mouse support: Enabled\n");
    
    return game;
}

void willy_game_free(WillyGame *game) {
    if (!game) return;
    
    if (game->timer_id > 0) {
        g_source_remove(game->timer_id);
    }
    
    // Free balls list
    g_list_free_full(game->balls, (GDestroyNotify)ball_free);
    
    // Free hash table
    if (game->keys_pressed) {
        g_hash_table_destroy(game->keys_pressed);
    }
    
    // Free components
    if (game->score_manager) high_score_manager_free(game->score_manager);
    if (game->sprite_loader) sprite_loader_free(game->sprite_loader);
    if (game->level_loader) level_loader_free(game->level_loader);
    if (game->sound_manager) sound_manager_free(game->sound_manager);
    
    g_free(game);
}

gboolean willy_game_on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    
    if (!game_options.mouse_support || game->current_state != GAME_STATE_PLAYING) {
        return FALSE;
    }
    
    printf("Mouse button %d pressed\n", (int)event->button);
    
    // Get menubar height to account for offset
    GtkRequisition menubar_req;
    gtk_widget_get_preferred_size(game->menubar, &menubar_req, NULL);
    int menubar_height = menubar_req.height;
    
    // Convert mouse coordinates to game coordinates
    double mouse_x = event->x;
    double mouse_y = event->y - menubar_height;
    
    // Account for scaling
    mouse_x /= game->current_scale_x;
    mouse_y /= game->current_scale_y;
    
    // Convert to grid coordinates
    int scaled_char_width = GAME_CHAR_WIDTH * game->scale_factor;
    int scaled_char_height = GAME_CHAR_HEIGHT * game->scale_factor;
    
    int click_col = (int)(mouse_x / scaled_char_width);
    int click_row = (int)(mouse_y / scaled_char_height);
    
    printf("Mouse click at grid (%d, %d), Willy at (%d, %d)\n", 
           click_row, click_col, game->willy_row, game->willy_col);
    
    if (event->button == 1) {  // Left mouse button
        game->mouse_button_held = true;
        game->held_button = 1;
        
        // Determine direction based on where user clicked relative to Willy
        int row_diff = click_row - game->willy_row;
        int col_diff = click_col - game->willy_col;
        
        // Use the larger difference to determine primary direction
        if (abs(col_diff) > abs(row_diff)) {
            // Horizontal movement
            if (col_diff > 0) {
                strcpy(game->mouse_direction, "RIGHT");
                strcpy(game->continuous_direction, "RIGHT");
                game->moving_continuously = true;
                strcpy(game->willy_direction, "RIGHT");
                printf("Holding RIGHT\n");
            } else if (col_diff < 0) {
                strcpy(game->mouse_direction, "LEFT");
                strcpy(game->continuous_direction, "LEFT");
                game->moving_continuously = true;
                strcpy(game->willy_direction, "LEFT");
                printf("Holding LEFT\n");
            }
        } else {
            // Vertical movement
            if (row_diff < 0) {
                strcpy(game->mouse_direction, "UP");
                game->mouse_up_held = true;
                game->up_pressed = true;
                printf("Holding UP\n");
            } else if (row_diff > 0) {
                strcpy(game->mouse_direction, "DOWN");
                game->mouse_down_held = true;
                game->down_pressed = true;
                printf("Holding DOWN\n");
            }
        }
    } else if (event->button == 2) {  // Middle mouse button - stop
        strcpy(game->continuous_direction, "");
        game->moving_continuously = false;
        game->up_pressed = false;
        game->down_pressed = false;
        game->mouse_up_held = false;
        game->mouse_down_held = false;
        game->mouse_button_held = false;
        printf("Middle click - stopping Willy\n");
    } else if (event->button == 3) {  // Right mouse button - jump
        willy_game_jump(game);
        printf("Right click - jumping\n");
    }
    
    return TRUE;
}

gboolean willy_game_on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    
    if (!game_options.mouse_support || game->current_state != GAME_STATE_PLAYING) {
        return FALSE;
    }
    
    printf("Mouse button %d released\n", (int)event->button);
    
    if (event->button == 1 && game->mouse_button_held && game->held_button == 1) {
        game->mouse_button_held = false;
        game->held_button = 0;
        
        if (strcmp(game->mouse_direction, "LEFT") == 0 || strcmp(game->mouse_direction, "RIGHT") == 0) {
            strcpy(game->continuous_direction, "");
            game->moving_continuously = false;
            printf("Released horizontal movement\n");
        } else if (strcmp(game->mouse_direction, "UP") == 0) {
            game->mouse_up_held = false;
            game->up_pressed = false;
            printf("Released UP movement\n");
        } else if (strcmp(game->mouse_direction, "DOWN") == 0) {
            game->mouse_down_held = false;
            game->down_pressed = false;
            printf("Released DOWN movement\n");
        }
        
        strcpy(game->mouse_direction, "");
    }
    
    return TRUE;
}

gboolean willy_game_on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    
    if (!game_options.mouse_support || game->current_state != GAME_STATE_PLAYING) {
        return FALSE;
    }
    
    // Optional: Could implement mouse hover effects here
    return TRUE;
}

void willy_game_setup_ui(WillyGame *game) {
    // Create main vbox
    game->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(game->window), game->vbox);
    
    // Create menubar (placeholder for now)
    game->menubar = gtk_menu_bar_new();
    willy_game_create_menubar(game);
    gtk_box_pack_start(GTK_BOX(game->vbox), game->menubar, FALSE, FALSE, 0);
    
    // Create drawing area
    game->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(game->drawing_area, TRUE);
    gtk_widget_set_vexpand(game->drawing_area, TRUE);
    gtk_widget_set_can_focus(game->drawing_area, TRUE);
    
    // Connect drawing area signals
    g_signal_connect(game->drawing_area, "draw", G_CALLBACK(willy_game_on_draw), game);
    g_signal_connect(game->drawing_area, "key-press-event", G_CALLBACK(willy_game_on_key_press), game);
    g_signal_connect(game->drawing_area, "key-release-event", G_CALLBACK(willy_game_on_key_release), game);
    
    // Enable keyboard events
    gtk_widget_add_events(game->drawing_area, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
    
    // Enable mouse events if mouse support is enabled
    if (game_options.mouse_support) {
        gtk_widget_add_events(game->drawing_area, 
                             GDK_BUTTON_PRESS_MASK | 
                             GDK_BUTTON_RELEASE_MASK |
                             GDK_BUTTON1_MOTION_MASK | 
                             GDK_BUTTON2_MOTION_MASK | 
                             GDK_BUTTON3_MOTION_MASK);
        g_signal_connect(game->drawing_area, "button-press-event", G_CALLBACK(willy_game_on_button_press), game);
        g_signal_connect(game->drawing_area, "button-release-event", G_CALLBACK(willy_game_on_button_release), game);
        printf("Mouse support enabled - hold mouse button to keep moving, middle-click to stop\n");
    }
    
    gtk_box_pack_start(GTK_BOX(game->vbox), game->drawing_area, TRUE, TRUE, 0);
    
    // Create status bar
    game->status_bar = gtk_label_new("");
    willy_game_update_status_bar(game);
    gtk_box_pack_start(GTK_BOX(game->vbox), game->status_bar, FALSE, FALSE, 0);
}

void willy_game_create_menubar(WillyGame *game) {
    // File menu
    GtkWidget *file_menu = gtk_menu_new();
    
    GtkWidget *new_game_item = gtk_menu_item_new_with_mnemonic("_New Game");
    g_signal_connect_swapped(new_game_item, "activate", G_CALLBACK(willy_game_new_game), game);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_game_item);
    
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator);
    
    GtkWidget *quit_item = gtk_menu_item_new_with_mnemonic("_Quit");
    g_signal_connect_swapped(quit_item, "activate", G_CALLBACK(willy_game_quit_game), game);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);
    
    GtkWidget *file_item = gtk_menu_item_new_with_mnemonic("_File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(game->menubar), file_item);
    
    // Game menu
    GtkWidget *game_menu = gtk_menu_new();
    
    GtkWidget *pause_item = gtk_menu_item_new_with_mnemonic("_Pause");
    g_signal_connect(pause_item, "activate", G_CALLBACK(willy_game_pause_toggle), game);
    gtk_menu_shell_append(GTK_MENU_SHELL(game_menu), pause_item);
    
    GtkWidget *reset_item = gtk_menu_item_new_with_mnemonic("_Reset Level");
    g_signal_connect_swapped(reset_item, "activate", G_CALLBACK(willy_game_reset_level), game);
    gtk_menu_shell_append(GTK_MENU_SHELL(game_menu), reset_item);
    
    GtkWidget *game_item = gtk_menu_item_new_with_mnemonic("_Game");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(game_item), game_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(game->menubar), game_item);
    
    // Help menu
    GtkWidget *help_menu = gtk_menu_new();
    
    GtkWidget *about_item = gtk_menu_item_new_with_mnemonic("_About");
    g_signal_connect(about_item, "activate", G_CALLBACK(willy_game_show_about), game);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);
    
    GtkWidget *help_item = gtk_menu_item_new_with_mnemonic("_Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(game->menubar), help_item);
}

void willy_game_pause_toggle(GtkWidget *widget, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    if (game->current_state == GAME_STATE_PLAYING) {
        game->current_state = GAME_STATE_PAUSED;
    } else if (game->current_state == GAME_STATE_PAUSED) {
        game->current_state = GAME_STATE_PLAYING;
    }
}

void willy_game_show_about(GtkWidget *widget, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    game->current_state = GAME_STATE_INTRO;
}

void willy_game_calculate_scaling_factors(WillyGame *game) {
    // Get current window size
    GtkAllocation allocation;
    gtk_widget_get_allocation(game->window, &allocation);
    int window_width = allocation.width;
    int window_height = allocation.height;
    
    // Get menubar and status bar heights
    GtkRequisition menubar_req, statusbar_req;
    gtk_widget_get_preferred_size(game->menubar, &menubar_req, NULL);
    gtk_widget_get_preferred_size(game->status_bar, &statusbar_req, NULL);
    
    // Calculate available space for the game area
    int available_width = window_width;
    int available_height = window_height - menubar_req.height - statusbar_req.height;
    
    // Calculate scale based on height, unless width is smaller than height
    double scale;
    if (available_width < available_height) {
        scale = (double)available_width / game->base_game_width;
    } else {
        scale = (double)available_height / game->base_game_height;
    }
    
    // Round to nearest 0.1
    double rounded_scale = round(scale * 10.0) / 10.0;
    
    // Apply the same scale to both dimensions to maintain aspect ratio
    game->current_scale_x = rounded_scale;
    game->current_scale_y = rounded_scale;
    
    // Don't scale below 0.1 or above 10.0 for sanity
    if (game->current_scale_x < 0.1) game->current_scale_x = 0.1;
    if (game->current_scale_x > 10.0) game->current_scale_x = 10.0;
    if (game->current_scale_y < 0.1) game->current_scale_y = 0.1;
    if (game->current_scale_y > 10.0) game->current_scale_y = 10.0;
}

void willy_game_on_window_resize(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    willy_game_calculate_scaling_factors(game);
    gtk_widget_queue_draw(game->drawing_area);
}

void willy_game_load_level(WillyGame *game, const char *level_name) {
    strcpy(game->current_level, level_name);
    
    // Check if level exists
    if (!level_loader_level_exists(game->level_loader, level_name)) {
        printf("Level %s does not exist!\n", level_name);
        return;
    }
    
    // Get Willy's starting position from the level
    level_loader_get_willy_start_position(game->level_loader, level_name, &game->willy_row, &game->willy_col);
    
    // Initialize balls at the ball pit position
    g_list_free_full(game->balls, (GDestroyNotify)ball_free);
    game->balls = NULL;
    
    int ball_pit_row, ball_pit_col;
    willy_game_find_ballpit_position(game, &ball_pit_row, &ball_pit_col);
    printf("Ball pit at: (%d, %d)\n", ball_pit_row, ball_pit_col);
    
    for (int i = 0; i < game_options.number_of_balls; i++) {
        Ball *ball = ball_new(ball_pit_row, ball_pit_col);
        game->balls = g_list_append(game->balls, ball);
    }
    
    printf("Loaded level: %s\n", level_name);
    printf("Willy starts at: (%d, %d)\n", game->willy_row, game->willy_col);
}

bool willy_game_check_movement_collision(WillyGame *game, int old_row, int old_col, int new_row, int new_col) {
    // Don't check collisions if moving to/from ballpit
    char *old_tile = willy_game_get_tile(game, old_row, old_col);
    char *new_tile = willy_game_get_tile(game, new_row, new_col);
    bool old_is_ballpit = (old_tile && strcmp(old_tile, "BALLPIT") == 0);
    bool new_is_ballpit = (new_tile && strcmp(new_tile, "BALLPIT") == 0);
    
    if (old_is_ballpit || new_is_ballpit) {
        g_free(old_tile);
        g_free(new_tile);
        return false; // No collision in ballpit areas
    }
    
    g_free(old_tile);
    g_free(new_tile);
    
    for (GList *l = game->balls; l != NULL; l = l->next) {
        Ball *ball = (Ball*)l->data;
        
        // Skip balls that are in ballpits
        char *ball_tile = willy_game_get_tile(game, ball->row, ball->col);
        if (ball_tile && strcmp(ball_tile, "BALLPIT") == 0) {
            g_free(ball_tile);
            continue;
        }
        g_free(ball_tile);
        
        // Only check collision if ball is at Willy's new position
        if (ball->row == new_row && ball->col == new_col) {
            return true; // Collision detected
        }
        
        // Check for crossing paths (ball and Willy swapping positions)
        if (ball->row == old_row && ball->col == old_col && 
            ball->row == new_row && ball->col == new_col && 
            old_row == new_row) { // Only check swapping if on same horizontal level
            return true; // Collision detected - crossing paths horizontally
        }
    }
    return false;
}

gboolean willy_game_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    
    const char *keyname = gdk_keyval_name(event->keyval);
    if (!keyname) return FALSE;
    
    // Add key to pressed keys set
    g_hash_table_insert(game->keys_pressed, g_strdup(keyname), GINT_TO_POINTER(TRUE));
    
    // Check for modifier keys
    bool ctrl_pressed = (event->state & GDK_CONTROL_MASK) != 0;
    bool shift_pressed = (event->state & GDK_SHIFT_MASK) != 0;
    bool alt_pressed = (event->state & GDK_MOD1_MASK) != 0;
    
    if (strcmp(keyname, "Escape") == 0) {
        willy_game_quit_game(game);
    } else if (strcmp(keyname, "F11") == 0) {
        GdkWindow *window = gtk_widget_get_window(game->window);
        if (window) {
            GdkWindowState state = gdk_window_get_state(window);
            if (state & GDK_WINDOW_STATE_FULLSCREEN) {
                gtk_window_unfullscreen(GTK_WINDOW(game->window));
            } else {
                gtk_window_fullscreen(GTK_WINDOW(game->window));
            }
        }
    } else if (game->current_state == GAME_STATE_INTRO) {
        if (strcmp(keyname, "Return") == 0 || strcmp(keyname, "Enter") == 0 || strcmp(keyname, "KP_Enter") == 0) {
            printf("Starting game...\n");
            willy_game_start_game(game);
        }
    } else if (game->current_state == GAME_STATE_PLAYING) {
        if (strcmp(keyname, "space") == 0) {
            willy_game_jump(game);
        } else if (strcmp(keyname, "Left") == 0 || (game_options.use_wasd && strcmp(keyname, "a") == 0)) {
            strcpy(game->continuous_direction, "LEFT");
            game->moving_continuously = true;
            strcpy(game->willy_direction, "LEFT");
        } else if (strcmp(keyname, "Right") == 0 || (game_options.use_wasd && strcmp(keyname, "d") == 0)) {
            strcpy(game->continuous_direction, "RIGHT");
            game->moving_continuously = true;
            strcpy(game->willy_direction, "RIGHT");
        } else if (strcmp(keyname, "Up") == 0 || (game_options.use_wasd && strcmp(keyname, "w") == 0)) {
            game->up_pressed = true;
        } else if (strcmp(keyname, "Down") == 0 || (game_options.use_wasd && strcmp(keyname, "s") == 0)) {
            game->down_pressed = true;
        } else if ((strcmp(keyname, "L") == 0 || strcmp(keyname, "l") == 0) && ctrl_pressed) {
            willy_game_complete_level_nobonus(game);
        } else if ((strcmp(keyname, "S") == 0 || strcmp(keyname, "s") == 0) && ctrl_pressed) {
            bool current_sound_state = sound_manager_is_sound_enabled(game->sound_manager);
            sound_manager_set_sound_enabled(game->sound_manager, !current_sound_state);
            
            const char *sound_status = current_sound_state ? "OFF" : "ON";
            printf("Sound toggled %s\n", sound_status);
            
            if (!current_sound_state) {
                sound_manager_play_sound(game->sound_manager, "bell.mp3");
            }
        } else if (strcmp(keyname, "F5") == 0) {
            redbg += 0.25;
            if (redbg > 1.0) {
                redbg = 0.0;
            }
        } else if (strcmp(keyname, "F6") == 0) {
            greenbg += 0.25;
            if (greenbg > 1.0) {
                greenbg = 0.0;
            }
        } else if (strcmp(keyname, "F7") == 0) {
            bluebg += 0.25;
            if (bluebg > 1.0) {
                bluebg = 0.0;
            }
        } else {
            game->moving_continuously = false;
            strcpy(game->continuous_direction, "");
        }
    } else if (game->current_state == GAME_STATE_GAME_OVER) {
        if (strcmp(keyname, "Return") == 0 || strcmp(keyname, "Enter") == 0 || strcmp(keyname, "KP_Enter") == 0) {
            game->current_state = GAME_STATE_INTRO;
        }
    } else if (game->current_state == GAME_STATE_HIGH_SCORE_ENTRY) {
        if (strcmp(keyname, "Return") == 0 || strcmp(keyname, "Enter") == 0 || strcmp(keyname, "KP_Enter") == 0) {
            if (strlen(game->name_input) > 0) {
                high_score_manager_add_score(game->score_manager, game->name_input, game->score);
            }
            game->current_state = GAME_STATE_HIGH_SCORE_DISPLAY;
        } else if (strcmp(keyname, "BackSpace") == 0) {
            size_t len = strlen(game->name_input);
            if (len > 0) {
                game->name_input[len - 1] = '\0';
            }
        } else if (strlen(keyname) == 1) {
            // Single character key
            size_t len = strlen(game->name_input);
            if (len < 19) { // Limit name length (reserve space for null terminator)
                game->name_input[len] = keyname[0];
                game->name_input[len + 1] = '\0';
            }
        }
    } else if (game->current_state == GAME_STATE_HIGH_SCORE_DISPLAY) {
        if (strcmp(keyname, "Escape") == 0) {
            willy_game_quit_game(game);
        } else {
            game->current_state = GAME_STATE_INTRO;
        }
    }
    
    return TRUE;
}

gboolean willy_game_on_key_release(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    
    const char *keyname = gdk_keyval_name(event->keyval);
    if (!keyname) return FALSE;
    
    // Remove key from pressed keys set
    g_hash_table_remove(game->keys_pressed, keyname);
    
    if (strcmp(keyname, "Up") == 0) {
        game->up_pressed = false;
    } else if (strcmp(keyname, "Down") == 0) {
        game->down_pressed = false;
    }
    
    return TRUE;
}

void willy_game_start_game(WillyGame *game) {
    game->current_state = GAME_STATE_PLAYING;
    game->level = game_options.starting_level;
    game->score = 0;
    game->lives = game_options.starting_lives;
    game->bonus = 1000;
    game->frame_count = 0;
    strcpy(game->continuous_direction, "");
    game->moving_continuously = false;
    game->up_pressed = false;
    game->down_pressed = false;
    game->life_adder = 0;
    
    // Check if the specified starting level exists, fall back to level 1 if not
    char level_name[32];
    snprintf(level_name, sizeof(level_name), "level%d", game->level);
    if (!level_loader_level_exists(game->level_loader, level_name)) {
        printf("Warning: Level %d does not exist, starting at level 1\n", game->level);
        game->level = 1;
        strcpy(level_name, "level1");
    }
    
    willy_game_load_level(game, level_name);
    willy_game_update_status_bar(game);
}

void willy_game_jump(WillyGame *game) {
    int y = game->willy_row;
    int x = game->willy_col;

    // Get the current and below tiles
    char *current_tile = willy_game_get_tile(game, y, x);
    char *below_tile = willy_game_get_tile(game, y + 1, x);

    // Can jump if standing on "UPSPRING" or if below tile is a "PIPE"
    bool can_jump = false;
    if (current_tile && strcmp(current_tile, "UPSPRING") == 0) {
        can_jump = true;
    } else if (below_tile && strncmp(below_tile, "PIPE", 4) == 0) {
        can_jump = true;
    } else if (y == GAME_MAX_HEIGHT - 1) {
        can_jump = true;
    }

    if (can_jump) {
        game->jumping = true;
        // Apply a stronger jump if standing on "UPSPRING"
        if (current_tile && strcmp(current_tile, "UPSPRING") == 0) {
            game->willy_velocity_y = -6;
        } else {
            game->willy_velocity_y = -5;
        }
        sound_manager_play_sound(game->sound_manager, "jump.mp3");
    }

    g_free(current_tile);
    g_free(below_tile);
}

char* willy_game_get_tile(WillyGame *game, int row, int col) {
    return level_loader_get_tile(game->level_loader, game->current_level, row, col);
}

void willy_game_set_tile(WillyGame *game, int row, int col, const char *tile) {
    level_loader_set_tile(game->level_loader, game->current_level, row, col, tile);
}

bool willy_game_can_move_to(WillyGame *game, int row, int col) {
    if (row < 0 || row >= GAME_SCREEN_HEIGHT || col < 0 || col >= GAME_SCREEN_WIDTH) {
        return false;
    }
    
    char *tile = willy_game_get_tile(game, row, col);
    if (!tile) return false;
    
    bool can_move = (strcmp(tile, "EMPTY") == 0 || strcmp(tile, "LADDER") == 0 || 
                     strcmp(tile, "PRESENT") == 0 || strcmp(tile, "BELL") == 0 || 
                     strcmp(tile, "UPSPRING") == 0 || strcmp(tile, "SIDESPRING") == 0 || 
                     strcmp(tile, "TACK") == 0 || strcmp(tile, "BALLPIT") == 0 || 
                     strcmp(tile, "WILLY_RIGHT") == 0 || strcmp(tile, "WILLY_LEFT") == 0);
    
    g_free(tile);
    return can_move;
}

bool willy_game_is_on_solid_ground(WillyGame *game) {
    int y = game->willy_row;
    int x = game->willy_col;
    
    if (y >= GAME_MAX_HEIGHT - 1) {
        return true;
    }
    
    char *current_tile = willy_game_get_tile(game, y, x);
    char *below_tile = willy_game_get_tile(game, y + 1, x);
    
    bool on_ladder = (current_tile && strcmp(current_tile, "LADDER") == 0);
    bool on_pipe = (below_tile && strncmp(below_tile, "PIPE", 4) == 0);
    
    g_free(current_tile);
    g_free(below_tile);
    
    return on_ladder || on_pipe;
}

void willy_game_update_willy_movement(WillyGame *game) {
    if (game->current_state != GAME_STATE_PLAYING) return;
    
    game->previous_willy_row = game->willy_row;
    game->previous_willy_col = game->willy_col;
    
    // Store Willy's current position for collision checking
    int old_row = game->willy_row;
    int old_col = game->willy_col;
    
    char *current_tile = willy_game_get_tile(game, game->willy_row, game->willy_col);
    bool on_ladder = (current_tile && strcmp(current_tile, "LADDER") == 0);
    bool moved_on_ladder = false;
    
    if (game->up_pressed) {
        int target_row = game->willy_row - 1;
        if (target_row >= 0) {
            char *above_tile = willy_game_get_tile(game, target_row, game->willy_col);
            
            if ((on_ladder && above_tile && strcmp(above_tile, "LADDER") == 0 && 
                 willy_game_can_move_to(game, target_row, game->willy_col)) ||
                (!on_ladder && above_tile && strcmp(above_tile, "LADDER") == 0 && 
                 willy_game_can_move_to(game, target_row, game->willy_col))) {
                
                if (!willy_game_check_movement_collision(game, old_row, old_col, target_row, game->willy_col)) {
                    game->willy_row--;
                    game->willy_velocity_y = 0;
                    moved_on_ladder = true;
                    game->moving_continuously = false;
                    strcpy(game->continuous_direction, "");
                    sound_manager_play_sound(game->sound_manager, "ladder.mp3");
                } else {
                    willy_game_die(game);
                    g_free(above_tile);
                    g_free(current_tile);
                    return;
                }
            }
            g_free(above_tile);
        }
    }
    
    if (game->down_pressed && !moved_on_ladder) {
        int target_row = game->willy_row + 1;
        if (target_row < GAME_SCREEN_HEIGHT) {
            char *below_tile = willy_game_get_tile(game, target_row, game->willy_col);
            
            if ((on_ladder && willy_game_can_move_to(game, target_row, game->willy_col)) ||
                (!on_ladder && below_tile && strcmp(below_tile, "LADDER") == 0 && 
                 willy_game_can_move_to(game, target_row, game->willy_col))) {
                
                if (!willy_game_check_movement_collision(game, old_row, old_col, target_row, game->willy_col)) {
                    game->willy_row++;
                    game->willy_velocity_y = 0;
                    moved_on_ladder = true;
                    game->moving_continuously = false;
                    strcpy(game->continuous_direction, "");
                    sound_manager_play_sound(game->sound_manager, "ladder.mp3");
                } else {
                    willy_game_die(game);
                    g_free(below_tile);
                    g_free(current_tile);
                    return;
                }
            }
            g_free(below_tile);
        }
    }
    
    if (!moved_on_ladder) {
        if (game->moving_continuously && strlen(game->continuous_direction) > 0) {
            bool hit_obstacle = false;
            
            if (strcmp(game->continuous_direction, "LEFT") == 0) {
                if (willy_game_can_move_to(game, game->willy_row, game->willy_col - 1)) {
                    if (!willy_game_check_movement_collision(game, old_row, old_col, game->willy_row, game->willy_col - 1)) {
                        game->willy_col--;
                    } else {
                        willy_game_die(game);
                        g_free(current_tile);
                        return;
                    }
                } else {
                    hit_obstacle = true;
                }
            } else if (strcmp(game->continuous_direction, "RIGHT") == 0) {
                if (willy_game_can_move_to(game, game->willy_row, game->willy_col + 1)) {
                    if (!willy_game_check_movement_collision(game, old_row, old_col, game->willy_row, game->willy_col + 1)) {
                        game->willy_col++;
                    } else {
                        willy_game_die(game);
                        g_free(current_tile);
                        return;
                    }
                } else {
                    hit_obstacle = true;
                }
            }
            
            if (hit_obstacle) {
                game->moving_continuously = false;
                strcpy(game->continuous_direction, "");
            }
        } else if (!game->moving_continuously) {
            if (g_hash_table_contains(game->keys_pressed, "Left")) {
                strcpy(game->willy_direction, "LEFT");
                if (willy_game_can_move_to(game, game->willy_row, game->willy_col - 1)) {
                    if (!willy_game_check_movement_collision(game, old_row, old_col, game->willy_row, game->willy_col - 1)) {
                        game->willy_col--;
                    } else {
                        willy_game_die(game);
                        g_free(current_tile);
                        return;
                    }
                }
            } else if (g_hash_table_contains(game->keys_pressed, "Right")) {
                strcpy(game->willy_direction, "RIGHT");
                if (willy_game_can_move_to(game, game->willy_row, game->willy_col + 1)) {
                    if (!willy_game_check_movement_collision(game, old_row, old_col, game->willy_row, game->willy_col + 1)) {
                        game->willy_col++;
                    } else {
                        willy_game_die(game);
                        g_free(current_tile);
                        return;
                    }
                }
            }
        }
    }
    
    g_free(current_tile);
    current_tile = willy_game_get_tile(game, game->willy_row, game->willy_col);
    on_ladder = (current_tile && strcmp(current_tile, "LADDER") == 0);
    
    if (!on_ladder) {
        if (!willy_game_is_on_solid_ground(game)) {
            game->willy_velocity_y += 1;
        } else {
            if (game->willy_velocity_y > 0) {
                game->willy_velocity_y = 0;
                game->jumping = false;
            }
        }
        
        if (game->willy_velocity_y != 0) {
            int new_y = game->willy_row + (game->willy_velocity_y > 0 ? 1 : -1);
            if (willy_game_can_move_to(game, new_y, game->willy_col)) {
                if (!willy_game_check_movement_collision(game, game->willy_row, game->willy_col, new_y, game->willy_col)) {
                    game->willy_row = new_y;
                } else {
                    willy_game_die(game);
                    g_free(current_tile);
                    return;
                }
            }
            
            if (game->willy_velocity_y < 0) {
                game->willy_velocity_y++;
            }
        }
    } else {
        game->willy_velocity_y = 0;
        game->jumping = false;
    }
    
    g_free(current_tile);
}

void willy_game_update_balls(WillyGame *game) {
    if (game->current_state != GAME_STATE_PLAYING) return;
    
    static struct timeval last_ball_spawn = {0, 0};
    static bool first_run = true;
    
    if (first_run) {
        gettimeofday(&last_ball_spawn, NULL);
        first_run = false;
    }
    
    // Get the primary ball pit position
    int primary_ball_pit_row, primary_ball_pit_col;
    willy_game_find_ballpit_position(game, &primary_ball_pit_row, &primary_ball_pit_col);

    // Move any balls in non-primary ball pit positions to the primary ball pit
    for (GList *l = game->balls; l != NULL; l = l->next) {
        Ball *ball = (Ball*)l->data;
        char *tile = willy_game_get_tile(game, ball->row, ball->col);
        if (tile && strcmp(tile, "BALLPIT") == 0 &&
            (ball->row != primary_ball_pit_row || ball->col != primary_ball_pit_col)) {
            ball->row = primary_ball_pit_row;
            ball->col = primary_ball_pit_col;
            strcpy(ball->direction, ""); // Reset movement after relocation
        }
        g_free(tile);
    }

    // Apply movement logic to all balls
    for (GList *l = game->balls; l != NULL; l = l->next) {
        Ball *ball = (Ball*)l->data;
        
        // Apply gravity to balls
        if (ball->row < GAME_MAX_HEIGHT - 1) {
            char *below_tile = willy_game_get_tile(game, ball->row + 1, ball->col);
            if (!below_tile || strncmp(below_tile, "PIPE", 4) != 0) {
                ball->row++;
                strcpy(ball->direction, "");
            } else {
                // Ball is on a platform, move horizontally
                if (strlen(ball->direction) == 0) {
                    strcpy(ball->direction, (rand() % 2) ? "RIGHT" : "LEFT");
                }

                if (strcmp(ball->direction, "RIGHT") == 0) {
                    if (ball->col + 1 < GAME_MAX_WIDTH) {
                        char *right_tile = willy_game_get_tile(game, ball->row, ball->col + 1);
                        if (!right_tile || strncmp(right_tile, "PIPE", 4) != 0) {
                            ball->col++;
                        } else {
                            strcpy(ball->direction, "LEFT");
                        }
                        g_free(right_tile);
                    } else {
                        strcpy(ball->direction, "LEFT");
                    }
                } else { // LEFT
                    if (ball->col - 1 >= 0) {
                        char *left_tile = willy_game_get_tile(game, ball->row, ball->col - 1);
                        if (!left_tile || strncmp(left_tile, "PIPE", 4) != 0) {
                            ball->col--;
                        } else {
                            strcpy(ball->direction, "RIGHT");
                        }
                        g_free(left_tile);
                    } else {
                        strcpy(ball->direction, "RIGHT");
                    }
                }
            }
            g_free(below_tile);
        }
    }

    // Ensure the ball count stays within the limit and apply random spawn delay
    struct timeval now;
    gettimeofday(&now, NULL);
    long delay_ms = (now.tv_sec - last_ball_spawn.tv_sec) * 1000 + (now.tv_usec - last_ball_spawn.tv_usec) / 1000;
    long random_delay = 500 + (rand() % 1500); // Random delay between 500ms - 2000ms
    
    int ball_count = g_list_length(game->balls);
    if (ball_count < game_options.number_of_balls && delay_ms > random_delay) {
        Ball *new_ball = ball_new(primary_ball_pit_row, primary_ball_pit_col);
        game->balls = g_list_append(game->balls, new_ball);
        last_ball_spawn = now; // Reset spawn timer
    }
}

void willy_game_check_collisions(WillyGame *game) {
    if (game->current_state != GAME_STATE_PLAYING) return;
    
    int y = game->willy_row;
    int x = game->willy_col;
    char *current_tile = willy_game_get_tile(game, y, x);
    
    // Check if Willy left a destroyable pipe (PIPE18) - destroy it after he leaves
    int prev_y = game->previous_willy_row;
    int prev_x = game->previous_willy_col;
    
    // Only check if Willy actually moved
    if (prev_y != y || prev_x != x) {
        // Check if there's a destroyable pipe below where Willy was previously standing
        if (prev_y + 1 < GAME_SCREEN_HEIGHT) {
            char *below_previous_tile = willy_game_get_tile(game, prev_y + 1, prev_x);
            if (below_previous_tile && strcmp(below_previous_tile, "PIPE18") == 0) {
                // Destroy the pipe after Willy leaves it
                willy_game_set_tile(game, prev_y + 1, prev_x, "EMPTY");
                
                // Play a destruction sound
                sound_manager_play_sound(game->sound_manager, "present.mp3");
                
                // Add points for destroying the pipe
                game->score += 50;
            }
            g_free(below_previous_tile);
        }
    }
    
    // Check ball collisions
    for (GList *l = game->balls; l != NULL; l = l->next) {
        Ball *ball = (Ball*)l->data;
        // Only check collision if Willy and ball are at the SAME row AND column
        // AND not in a ballpit
        if (ball->row == y && ball->col == x && 
            (!current_tile || strcmp(current_tile, "BALLPIT") != 0)) {
            sound_manager_play_sound(game->sound_manager, "tack.mp3");
            willy_game_die(game);
            g_free(current_tile);
            return;
        }
    }
    
    // Check tile interactions
    if (current_tile) {
        if (strcmp(current_tile, "TACK") == 0) {
            willy_game_die(game);
        } else if (strcmp(current_tile, "BELL") == 0) {
            sound_manager_play_sound(game->sound_manager, "bell.mp3");
            willy_game_complete_level(game);
        } else if (strcmp(current_tile, "PRESENT") == 0) {
            game->score += 100;
            sound_manager_play_sound(game->sound_manager, "present.mp3");
            willy_game_set_tile(game, y, x, "EMPTY");
        } else if (strcmp(current_tile, "UPSPRING") == 0) {
            sound_manager_play_sound(game->sound_manager, "jump.mp3");
            willy_game_jump(game);
        } else if (strcmp(current_tile, "SIDESPRING") == 0) {
            sound_manager_play_sound(game->sound_manager, "jump.mp3");
            // Reverse continuous direction if moving continuously
            if (game->moving_continuously) {
                if (strcmp(game->continuous_direction, "RIGHT") == 0) {
                    strcpy(game->continuous_direction, "LEFT");
                    strcpy(game->willy_direction, "LEFT");
                } else if (strcmp(game->continuous_direction, "LEFT") == 0) {
                    strcpy(game->continuous_direction, "RIGHT");
                    strcpy(game->willy_direction, "RIGHT");
                }
            } else {
                // Just reverse direction without continuous movement
                if (strcmp(game->willy_direction, "RIGHT") == 0) {
                    strcpy(game->willy_direction, "LEFT");
                } else {
                    strcpy(game->willy_direction, "RIGHT");
                }
            }
        }
    }
    
    // Check for jumping over balls (bonus points)
    if (game->jumping || game->willy_velocity_y != 0) {
        for (int i = 1; i < 5; i++) {
            int check_y = y + i;
            if (check_y < GAME_SCREEN_HEIGHT) {
                for (GList *l = game->balls; l != NULL; l = l->next) {
                    Ball *ball = (Ball*)l->data;
                    if (ball->row == check_y && ball->col == x) {
                        game->score += 20;
                        sound_manager_play_sound(game->sound_manager, "boop.mp3");
                        break;
                    }
                }
            }
        }
    }
    
    g_free(current_tile);
}

void willy_game_die(WillyGame *game) {
    // Play death sound
    sound_manager_play_sound(game->sound_manager, "tack.mp3");
    
    // Flash the screen
    if (!game_options.disable_flash) {
        willy_game_flash_death_screen(game);
    }

    game->lives--;
    if (game->lives <= 0) {
        willy_game_game_over(game);
    } else {
        willy_game_reset_level(game);
    }
}

void willy_game_complete_level_nobonus(WillyGame *game) {
    game->level++;
    strcpy(game->continuous_direction, "");
    game->moving_continuously = false;
    strcpy(game->willy_direction, "LEFT");
    
    // Try to load next level
    char next_level[32];
    snprintf(next_level, sizeof(next_level), "level%d", game->level);
    if (level_loader_level_exists(game->level_loader, next_level)) {
        willy_game_load_level(game, next_level);
    } else {
        // No more levels, restart from level 1 with increased difficulty
        game->level = 1;
        willy_game_load_level(game, "level1");
    }
    
    // Reset game state for new level
    game->bonus = 1000;
    game->frame_count = 0;
}

void willy_game_complete_level(WillyGame *game) {
    game->score += game->bonus;
    game->level++;
    strcpy(game->continuous_direction, "");
    game->moving_continuously = false;
    strcpy(game->willy_direction, "LEFT");

    // Try to load next level
    char next_level[32];
    snprintf(next_level, sizeof(next_level), "level%d", game->level);
    if (level_loader_level_exists(game->level_loader, next_level)) {
        willy_game_load_level(game, next_level);
    } else {
        // No more levels, restart from level 1 with increased difficulty
        game->level = 1;
        willy_game_load_level(game, "level1");
    }
    
    // Reset game state for new level
    game->bonus = 1000;
    game->frame_count = 0;
}

void willy_game_reset_level(WillyGame *game) {
    level_loader_reset_levels(game->level_loader);
    willy_game_load_level(game, game->current_level);
    
    game->willy_velocity_x = 0;
    game->willy_velocity_y = 0;
    game->jumping = false;
    game->bonus = 1000;
    game->frame_count = 0;
    strcpy(game->continuous_direction, "");
    game->moving_continuously = false;
    game->up_pressed = false;
    game->down_pressed = false;
}

void willy_game_game_over(WillyGame *game) {
    if (high_score_manager_is_high_score(game->score_manager, game->score)) {
        game->current_state = GAME_STATE_HIGH_SCORE_ENTRY;
        strcpy(game->name_input, "");
    } else {
        game->current_state = GAME_STATE_HIGH_SCORE_DISPLAY;
    }
}

void willy_game_new_game(WillyGame *game) {
    game->current_state = GAME_STATE_INTRO;
}

void willy_game_quit_game(WillyGame *game) {
    gtk_widget_hide(game->window);
    gtk_main_quit();
}

void willy_game_update_status_bar(WillyGame *game) {
    if (game->current_state == GAME_STATE_PLAYING) {
        char status_text[256];
        snprintf(status_text, sizeof(status_text), 
                "SCORE: %d    BONUS: %d    Level: %d    Willy the Worms Left: %d",
                game->score, game->bonus, game->level, game->lives);
        gtk_label_set_text(GTK_LABEL(game->status_bar), status_text);
    } else {
        gtk_label_set_text(GTK_LABEL(game->status_bar), "Willy the Worm - C GTK Edition");
    }
}

gboolean willy_game_game_tick(gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    
    if (game->current_state == GAME_STATE_PLAYING) {
        willy_game_update_willy_movement(game);
        willy_game_update_balls(game);
        willy_game_check_collisions(game);
        
        // Update bonus/timer
        game->frame_count++;
        if (game->frame_count >= game->fps) {
            game->frame_count = 0;
            int old_bonus = game->bonus;
            game->bonus = (game->bonus > 10) ? game->bonus - 10 : 0;
            
            if ((game->score / GAME_NEWLIFEPOINTS) > game->life_adder) {
                game->lives++;
                game->life_adder++;
                printf("Extra life awarded! Score: %d, Lives: %d\n", game->score, game->lives);
                
                // Play a sound for extra life
                sound_manager_play_sound(game->sound_manager, "bell.mp3");
            }

            // Check for time warnings
            if ((game->bonus <= 100 && old_bonus > 100) || (game->bonus <= 50 && old_bonus > 50)) {
                printf("Warning: Time running low!\n");
                sound_manager_play_sound(game->sound_manager, "bell.mp3");
            }

            // Check if timer ran out - Willy dies!
            if (game->bonus <= 0) {
                printf("Time's up! Bonus reached zero - Willy dies!\n");
                sound_manager_play_sound(game->sound_manager, "tack.mp3");
                willy_game_die(game);
                return TRUE; // Exit early since we're now in death/reset state
            }
        }
        willy_game_update_status_bar(game);
    }
    gtk_widget_queue_draw(game->drawing_area);
    
    return TRUE; // Continue the timer
}

gboolean willy_game_on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    WillyGame *game = (WillyGame*)user_data;
    
    // Only paint blue background for intro screen
    if (game->current_state == GAME_STATE_INTRO) {
        cairo_set_source_rgb(cr, redbg, greenbg, bluebg);
        cairo_paint(cr);
        willy_game_draw_intro_screen(game, cr);
    } else if (game->current_state == GAME_STATE_PLAYING) {
        willy_game_draw_game_screen(game, cr);
    } else if (game->current_state == GAME_STATE_GAME_OVER) {
        willy_game_draw_game_over_screen(game, cr);
    } else if (game->current_state == GAME_STATE_HIGH_SCORE_ENTRY) {
        willy_game_draw_high_score_entry_screen(game, cr);
    } else if (game->current_state == GAME_STATE_HIGH_SCORE_DISPLAY) {
        willy_game_draw_high_score_display_screen(game, cr);
    }
    
    return TRUE;
}

void willy_game_find_ballpit_position(WillyGame *game, int *row, int *col) {
    for (int r = 0; r < GAME_MAX_HEIGHT; r++) {
        for (int c = 0; c < GAME_MAX_WIDTH; c++) {
            char *tile = willy_game_get_tile(game, r, c);
            if (tile && strcmp(tile, "BALLPIT") == 0) {
                *row = r;
                *col = c;
                g_free(tile);
                return; // Return the first position found
            }
            g_free(tile);
        }
    }
    *row = -1;
    *col = -1; // Indicate that no BALLPIT was found
}

void willy_game_draw_game_screen(WillyGame *game, cairo_t *cr) {
    // Get menubar height to use as offset
    GtkRequisition menubar_req;
    gtk_widget_get_preferred_size(game->menubar, &menubar_req, NULL);
    int menubar_height = menubar_req.height;
    
    // Apply scaling transformation
    cairo_save(cr);
    cairo_translate(cr, 0, menubar_height);
    cairo_scale(cr, game->current_scale_x, game->current_scale_y);
    
    // Calculate the scaled character dimensions
    int scaled_char_width = GAME_CHAR_WIDTH * game->scale_factor;
    int scaled_char_height = GAME_CHAR_HEIGHT * game->scale_factor;
    
    // Draw ALL sprite positions with blue background, even empty ones
    for (int row = 0; row < GAME_MAX_HEIGHT; row++) {
        for (int col = 0; col < GAME_MAX_WIDTH; col++) {
            int x = col * scaled_char_width;
            int y = row * scaled_char_height;
            
            // Paint blue background for EVERY sprite position
            cairo_set_source_rgb(cr, redbg, greenbg, bluebg);
            cairo_rectangle(cr, x, y, scaled_char_width, scaled_char_height);
            cairo_fill(cr);
            
            // Get the tile at this position
            char *tile = willy_game_get_tile(game, row, col);
            
            // Draw sprite if not empty or Willy start position, but not at Willy's current position
            if (tile && strcmp(tile, "EMPTY") != 0 && strstr(tile, "WILLY") == NULL && 
                !(row == game->willy_row && col == game->willy_col)) {
                cairo_surface_t *sprite = sprite_loader_get_sprite(game->sprite_loader, tile);
                if (sprite) {
                    cairo_set_source_surface(cr, sprite, x, y);
                    cairo_paint(cr);
                }
            }
            g_free(tile);
        }
    }
    
    // Draw balls (but not the ones in ball pits or at Willy's position)
    for (GList *l = game->balls; l != NULL; l = l->next) {
        Ball *ball = (Ball*)l->data;
        char *ball_tile = willy_game_get_tile(game, ball->row, ball->col);
        bool is_ballpit = (ball_tile && strcmp(ball_tile, "BALLPIT") == 0);
        bool at_willy = (ball->row == game->willy_row && ball->col == game->willy_col);
        
        if (!is_ballpit && !at_willy) {
            // Make sure ball is in visible area
            if (ball->row >= 0 && ball->row < GAME_MAX_HEIGHT && 
                ball->col >= 0 && ball->col < GAME_MAX_WIDTH) {
                
                int x = ball->col * scaled_char_width;
                int y = ball->row * scaled_char_height;
                
                cairo_surface_t *sprite = sprite_loader_get_sprite(game->sprite_loader, "BALL");
                if (sprite) {
                    cairo_set_source_surface(cr, sprite, x, y);
                    cairo_paint(cr);
                }
            }
        }
        g_free(ball_tile);
    }
    
    // Draw Willy - make sure he's in visible area
    if (game->willy_row >= 0 && game->willy_row < GAME_MAX_HEIGHT &&
        game->willy_col >= 0 && game->willy_col < GAME_MAX_WIDTH) {
        
        int x = game->willy_col * scaled_char_width;
        int y = game->willy_row * scaled_char_height;
        
        const char *sprite_name = (strcmp(game->willy_direction, "LEFT") == 0) ? "WILLY_LEFT" : "WILLY_RIGHT";
        cairo_surface_t *sprite = sprite_loader_get_sprite(game->sprite_loader, sprite_name);
        if (sprite) {
            cairo_set_source_surface(cr, sprite, x, y);
            cairo_paint(cr);
        }
    }
    
    // Draw status information below the game area
    if (game->current_state == GAME_STATE_PLAYING) {
        // Calculate status bar area (below the main game area)
        int status_y = (GAME_SCREEN_HEIGHT + 1) * scaled_char_height;
        int status_height = 2 * scaled_char_height;
        
        // Draw blue background for status area
        cairo_set_source_rgb(cr, redbg, greenbg, bluebg);
        cairo_rectangle(cr, 0, status_y, GAME_SCREEN_WIDTH * scaled_char_width, status_height);
        cairo_fill(cr);
        
        // Create font for status text
        PangoLayout *layout = pango_cairo_create_layout(cr);
        PangoFontDescription *font_desc = pango_font_description_new();
        pango_font_description_set_family(font_desc, "Courier");
        
        // Scale font size based on the current scale
        int font_size = (int)(16 * fmin(game->current_scale_x, game->current_scale_y));
        if (font_size > 16) font_size = 16;
        if (font_size < 12) font_size = 12;
        pango_font_description_set_size(font_desc, font_size * PANGO_SCALE);
        
        pango_layout_set_font_description(layout, font_desc);
        
        // Create status text with fixed-width formatting
        char status_buffer[200];
        snprintf(status_buffer, sizeof(status_buffer), 
                "SCORE: %5d    BONUS: %4d    LEVEL: %2d    WILLY THE WORMS LEFT: %3d",
                game->score, game->bonus, game->level, game->lives);
        
        pango_layout_set_text(layout, status_buffer, -1);
        
        // Draw white text
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        
        // Position text in the status area
        int text_width, text_height;
        pango_layout_get_pixel_size(layout, &text_width, &text_height);
        
        // Center the text in the status area
        int text_x = (GAME_SCREEN_WIDTH * scaled_char_width - text_width) / 2;
        int text_y = status_y + (status_height - text_height) / 2;
        
        cairo_move_to(cr, text_x, text_y);
        pango_cairo_show_layout(cr, layout);
        
        pango_font_description_free(font_desc);
        g_object_unref(layout);
    }
    
    cairo_restore(cr);
}

void willy_game_draw_intro_screen(WillyGame *game, cairo_t *cr) {
    // Get menubar height to use as offset
    GtkRequisition menubar_req;
    gtk_widget_get_preferred_size(game->menubar, &menubar_req, NULL);
    int menubar_height = menubar_req.height;
    
    // Get current drawing area size
    GtkAllocation allocation;
    gtk_widget_get_allocation(game->drawing_area, &allocation);
    int area_width = allocation.width;
    int area_height = allocation.height - menubar_height;
    
    // Apply offset for menubar and clear background
    cairo_save(cr);
    cairo_translate(cr, 0, menubar_height);
    
    // Clear the intro area with blue background
    cairo_set_source_rgb(cr, redbg, greenbg, bluebg);
    cairo_rectangle(cr, 0, 0, area_width, area_height);
    cairo_fill(cr);
    
    // Text data
    const char *textdata[][10] = {
        {"", NULL},
        {"", NULL},
        {"Willy the Worm", NULL},
        {"", NULL},
        {"By Jason Hall", NULL},
        {"(original version by Alan Farmer 1985)", NULL},
        {"", NULL},
        {"This code is Free Open Source Software (FOSS)", NULL},
        {"Please feel free to do with it whatever you wish.", NULL},
        {"", NULL},
        {"If you do make changes though such as new levels,", NULL},
        {"please share them with the world.", NULL},
        {"", NULL},
        {"", NULL},
        {"Meet Willy the Worm ", "WILLY_RIGHT", ". Willy is a fun-", NULL},
        {"loving invertebrate who likes to climb", NULL},
        {"ladders ", "LADDER", " bounce on springs ", "UPSPRING", " ", "SIDESPRING", NULL},
        {"and find his presents ", "PRESENT", ".  But more", NULL},
        {"than anything, Willy loves to ring,", NULL},
        {"bells! ", "BELL", NULL},
        {"", NULL},
        {"You can press the arrow keys ← ↑ → ↓", NULL},
        {"to make Willy run and climb, or the", NULL},
        {"space bar to make him jump. Anything", NULL},
        {"else will make Willy stop and wait", NULL},
        {"", NULL},
        {"Good luck, and don't let Willy step on", NULL},
        {"a tack ", "TACK", " or get ran over by a ball! ", "BALL", NULL},
        {"", NULL},
        {"Press Enter to Continue", NULL},
        {NULL}
    };
    
    // Calculate appropriate sizing
    int num_lines = 0;
    while (textdata[num_lines][0] != NULL) num_lines++;
    
    int available_height = area_height - 40; // Leave some margin
    int line_height = available_height / num_lines;
    int base_font_size = fmax(10, fmin(20, line_height - 2));
    
    // Set up font
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_new();
    pango_font_description_set_family(font_desc, "Courier");
    pango_font_description_set_size(font_desc, base_font_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, font_desc);
    
    // Sprite size should match text height
    int sprite_size = base_font_size;
    
    for (int line_idx = 0; line_idx < num_lines; line_idx++) {
        if (!textdata[line_idx][0] || strlen(textdata[line_idx][0]) == 0) {
            continue;
        }
        
        int y_pos = 20 + line_idx * line_height; // 20px top margin
        
        // Calculate line width for centering
        int line_width = 0;
        for (int element_idx = 0; textdata[line_idx][element_idx] != NULL; element_idx++) {
            const char *element = textdata[line_idx][element_idx];
            if (strlen(element) == 0) continue;
            
            if (strcmp(element, "WILLY_RIGHT") == 0 || strcmp(element, "WILLY_LEFT") == 0 || 
                strcmp(element, "LADDER") == 0 || strcmp(element, "UPSPRING") == 0 || 
                strcmp(element, "SIDESPRING") == 0 || strcmp(element, "PRESENT") == 0 || 
                strcmp(element, "BELL") == 0 || strcmp(element, "TACK") == 0 || 
                strcmp(element, "BALL") == 0) {
                line_width += sprite_size;
            } else {
                pango_layout_set_text(layout, element, -1);
                int text_w, text_h;
                pango_layout_get_pixel_size(layout, &text_w, &text_h);
                line_width += text_w;
            }
        }
        
        // Center the line
        int start_x = (area_width - line_width) / 2;
        int current_x = start_x;
        
        // Draw elements
        for (int element_idx = 0; textdata[line_idx][element_idx] != NULL; element_idx++) {
            const char *element = textdata[line_idx][element_idx];
            if (strlen(element) == 0) continue;
            
            if (strcmp(element, "WILLY_RIGHT") == 0 || strcmp(element, "WILLY_LEFT") == 0 || 
                strcmp(element, "LADDER") == 0 || strcmp(element, "UPSPRING") == 0 || 
                strcmp(element, "SIDESPRING") == 0 || strcmp(element, "PRESENT") == 0 || 
                strcmp(element, "BELL") == 0 || strcmp(element, "TACK") == 0 || 
                strcmp(element, "BALL") == 0) {
                
                cairo_surface_t *sprite = sprite_loader_get_sprite(game->sprite_loader, element);
                if (sprite) {
                    cairo_save(cr);
                    cairo_translate(cr, current_x, y_pos);
                    
                    // Scale sprite to match text size
                    double sprite_scale = (double)sprite_size / (GAME_CHAR_WIDTH * game->scale_factor);
                    cairo_scale(cr, sprite_scale, sprite_scale);
                    
                    cairo_set_source_surface(cr, sprite, 0, 0);
                    cairo_paint(cr);
                    cairo_restore(cr);
                }
                current_x += sprite_size;
            } else {
                // Draw text
                pango_layout_set_text(layout, element, -1);
                int text_w, text_h;
                pango_layout_get_pixel_size(layout, &text_w, &text_h);
                
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                cairo_move_to(cr, current_x, y_pos);
                pango_cairo_show_layout(cr, layout);
                
                current_x += text_w;
            }
        }
    }
    
    pango_font_description_free(font_desc);
    g_object_unref(layout);
    cairo_restore(cr);
}

void willy_game_draw_game_over_screen(WillyGame *game, cairo_t *cr) {
    // Create font
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_new();
    pango_font_description_set_family(font_desc, "Monospace");
    pango_font_description_set_size(font_desc, 24 * PANGO_SCALE);
    pango_layout_set_font_description(layout, font_desc);
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // White text
    
    // Game Over text
    pango_layout_set_text(layout, "GAME OVER", -1);
    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    
    int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    int y_offset = (GAME_SCREEN_HEIGHT * GAME_CHAR_HEIGHT * game->scale_factor - text_height) / 2 - 50;
    
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    
    // Final Score
    char score_text[64];
    snprintf(score_text, sizeof(score_text), "Final Score: %d", game->score);
    pango_layout_set_text(layout, score_text, -1);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    y_offset += 60;
    
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    
    // Continue text
    pango_layout_set_text(layout, "Press Enter to return to intro", -1);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    y_offset += 80;
    
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    
    pango_font_description_free(font_desc);
    g_object_unref(layout);
}

void willy_game_draw_high_score_entry_screen(WillyGame *game, cairo_t *cr) {
    // Get menubar height to use as offset
    GtkRequisition menubar_req;
    gtk_widget_get_preferred_size(game->menubar, &menubar_req, NULL);
    int menubar_height = menubar_req.height;
    
    // Blue background
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
    cairo_paint(cr);
    
    // Create font
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font_desc = pango_font_description_new();
    pango_font_description_set_family(font_desc, "Courier");
    pango_font_description_set_size(font_desc, 20 * PANGO_SCALE);
    pango_layout_set_font_description(layout, font_desc);
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // White text
    
    int y_offset = menubar_height + 50;
    int line_height = 35;
    
    // Achievement message
    char *achievement = high_score_manager_get_achievement_message(game->score_manager, game->score);
    if (achievement && strlen(achievement) > 0) {
        pango_layout_set_text(layout, achievement, -1);
        int text_width, text_height;
        pango_layout_get_pixel_size(layout, &text_width, &text_height);
        int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
        cairo_move_to(cr, x_offset, y_offset);
        pango_cairo_show_layout(cr, layout);
        y_offset += line_height * 2;
    }
    g_free(achievement);
    
    // Score description
    char *description = high_score_manager_get_score_description(game->score_manager, game->score);
    pango_layout_set_text(layout, description, -1);
    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    y_offset += line_height * 2;
    g_free(description);
    
    // Score
    char score_text[64];
    snprintf(score_text, sizeof(score_text), "Your score for this game is %d...", game->score);
    pango_layout_set_text(layout, score_text, -1);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    y_offset += line_height * 2;
    
    // Name entry prompt
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "Enter your name >> %s", game->name_input);
    pango_layout_set_text(layout, prompt, -1);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    
    pango_font_description_free(font_desc);
    g_object_unref(layout);
}

void willy_game_draw_high_score_display_screen(WillyGame *game, cairo_t *cr) {
    // Get menubar height to use as offset
    GtkRequisition menubar_req;
    gtk_widget_get_preferred_size(game->menubar, &menubar_req, NULL);
    int menubar_height = menubar_req.height;
    
    // Blue background
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
    cairo_paint(cr);
    
    // Create fonts
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *header_font = pango_font_description_new();
    pango_font_description_set_family(header_font, "Courier");
    pango_font_description_set_size(header_font, 24 * PANGO_SCALE);
    
    PangoFontDescription *score_font = pango_font_description_new();
    pango_font_description_set_family(score_font, "Courier");
    pango_font_description_set_size(score_font, 16 * PANGO_SCALE);
    
    int y_offset = menubar_height + 20;
    int line_height = 25;
    
    // All-time Nightcrawlers header
    pango_layout_set_font_description(layout, header_font);
    cairo_set_source_rgb(cr, 1.0, 1.0, 0.0); // Yellow
    pango_layout_set_text(layout, "All-time Nightcrawlers", -1);
    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    y_offset += line_height + 10;
    
    // Draw black background for table
    int table_width = GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor / 2;
    int table_x = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - table_width) / 2;
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_rectangle(cr, table_x, y_offset, table_width, line_height * 10);
    cairo_fill(cr);
    
    // Permanent scores
    pango_layout_set_font_description(layout, score_font);
    cairo_set_source_rgb(cr, 1.0, 1.0, 0.0); // Yellow
    HighScore *permanent_scores = game->score_manager->permanent_scores;
    for (int i = 0; i < 10; i++) {
        char score_line[64];
        snprintf(score_line, sizeof(score_line), "%2d     %5d     %s",
                i + 1, permanent_scores[i].score, permanent_scores[i].name);
        
        pango_layout_set_text(layout, score_line, -1);
        cairo_move_to(cr, table_x + 10, y_offset);
        pango_cairo_show_layout(cr, layout);
        y_offset += line_height;
    }
    
    y_offset += 30;
    
    // Today's Best Pinworms header
    pango_layout_set_font_description(layout, header_font);
    cairo_set_source_rgb(cr, 0.0, 1.0, 1.0); // Cyan
    pango_layout_set_text(layout, "Today's Best Pinworms", -1);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    y_offset += line_height + 10;
    
    // Draw black background for daily table
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_rectangle(cr, table_x, y_offset, table_width, line_height * 10);
    cairo_fill(cr);
    
    // Daily scores
    pango_layout_set_font_description(layout, score_font);
    cairo_set_source_rgb(cr, 0.0, 1.0, 1.0); // Cyan
    HighScore *daily_scores = game->score_manager->daily_scores;
    for (int i = 0; i < 10; i++) {
        char score_line[64];
        snprintf(score_line, sizeof(score_line), "%2d     %5d     %s",
                i + 1, daily_scores[i].score, daily_scores[i].name);
        
        pango_layout_set_text(layout, score_line, -1);
        cairo_move_to(cr, table_x + 10, y_offset);
        pango_cairo_show_layout(cr, layout);
        y_offset += line_height;
    }
    
    y_offset += 30;
    
    // Instructions
    pango_layout_set_font_description(layout, score_font);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // White
    pango_layout_set_text(layout, "Hit any key to play again or ESC to exit", -1);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * game->scale_factor - text_width) / 2;
    cairo_move_to(cr, x_offset, y_offset);
    pango_cairo_show_layout(cr, layout);
    
    pango_font_description_free(header_font);
    pango_font_description_free(score_font);
    g_object_unref(layout);
}

void willy_game_flash_death_screen(WillyGame *game) {
    // Get the window for drawing
    GdkWindow *window = gtk_widget_get_window(game->drawing_area);
    if (!window) return;
    
    cairo_t *cr = gdk_cairo_create(window);
    
    // Flash white for 0.25 seconds
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); // White
    cairo_paint(cr);
    
    // Force immediate display update
    gdk_window_invalidate_rect(window, NULL, FALSE);
    
    // Process pending GTK events to ensure the white screen shows
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
    
    // Hold the white screen for 0.25 seconds
    usleep(250000); // 250ms in microseconds
    
    // Cleanup
    cairo_destroy(cr);
    
    // Return to normal - this will be handled by the next game tick/draw cycle
}

// Application Functions
int run_willy_game(const GameOptions *options) {
    // Set the global game_options to the provided options
    game_options = *options;
    
    // Print startup information
    printf("Willy the Worm - C GTK Edition\n");
    printf("Starting level: %d\n", game_options.starting_level);
    printf("Levels file: %s\n", game_options.levels_file);
    printf("Number of balls: %d\n", game_options.number_of_balls);
    printf("FPS: %d\n", game_options.fps);
    printf("Scale factor: %d\n", game_options.scale_factor);
    if (game_options.use_wasd) printf("Using WASD controls\n");
    if (game_options.disable_flash) printf("Death flash disabled\n");
    if (game_options.mouse_support) printf("Mouse support enabled\n");
    if (!game_options.sound_enabled) printf("Sound disabled\n");
    printf("\n");

    // Initialize GTK
    gtk_init(NULL, NULL);
    
    // Create the game
    WillyGame *game = willy_game_new();
    if (!game) {
        fprintf(stderr, "Failed to create game\n");
        return 1;
    }
    
    // Run the main loop
    gtk_main();
    
    // Cleanup
    willy_game_free(game);
    
    return 0;
}

int run_edwilly_game(const GameOptions *options) {
    // Set the global game_options to the provided options
    game_options = *options;
    
    // Print startup information
    printf("Willy the Worm - C GTK Edition\n");
    printf("Starting level: %d\n", game_options.starting_level);
    printf("Levels file: %s\n", game_options.levels_file);
    printf("Number of balls: %d\n", game_options.number_of_balls);
    printf("FPS: %d\n", game_options.fps);
    printf("Scale factor: %d\n", game_options.scale_factor);
    printf("Starting lives: %d\n", game_options.starting_lives);
    if (game_options.use_wasd) printf("Using WASD controls\n");
    if (game_options.disable_flash) printf("Death flash disabled\n");
    if (game_options.mouse_support) printf("Mouse support enabled\n");
    if (!game_options.sound_enabled) printf("Sound disabled\n");
    printf("\n");

    // Initialize GTK
    gtk_init(NULL, NULL);
    
    // Create the game window directly instead of a new application
    WillyGame *game_window = willy_game_new();
    if (!game_window) {
        fprintf(stderr, "Failed to create game window\n");
        return 1;
    }
    
    // Start the game at the intro screen
    gtk_widget_show_all(game_window->window);
    gtk_window_present(GTK_WINDOW(game_window->window));
    
    // Run a local event loop until the game window is closed
    while (gtk_widget_get_visible(game_window->window)) {
        // Process GTK events
        while (gtk_events_pending()) {
            gtk_main_iteration();
        }
        
        // Small delay to prevent busy waiting
        usleep(10000); // 10ms
    }
    
    printf("Game window closed, returning to editor\n");
    
    // Cleanup
    willy_game_free(game_window);
    
    return 0;
}
