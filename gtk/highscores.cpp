#include "willy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>   // For _mkdir
#else
    #include <unistd.h>
    #include <sys/types.h>
#endif

// HighScoreManager implementation
HighScoreManager* high_score_manager_new(void) {
    HighScoreManager *manager = g_malloc0(sizeof(HighScoreManager));
    
    // Initialize empty score lists
    for (int i = 0; i < 10; i++) {
        strcpy(manager->permanent_scores[i].name, "nobody");
        manager->permanent_scores[i].score = 0;
        strcpy(manager->daily_scores[i].name, "nobody");
        manager->daily_scores[i].score = 0;
    }
    
    high_score_manager_load_scores(manager);
    return manager;
}

void high_score_manager_free(HighScoreManager *manager) {
    if (!manager) return;
    
    high_score_manager_save_scores(manager);
    g_free(manager);
}

char* high_score_manager_get_score_file_path(void) {
    // Get home directory
    const char *home = getenv("HOME");
    if (!home) {
        home = getenv("USERPROFILE"); // Windows fallback
        if (!home) {
            return g_strdup("willy.scr"); // Fallback to current directory
        }
    }
    
    char *willy_dir = g_strdup_printf("%s/.willytheworm", home);
    
    // Create directory if it doesn't exist
#ifdef _WIN32
    _mkdir(willy_dir);
#else
    mkdir(willy_dir, 0755);
#endif
    
    char *file_path = g_strdup_printf("%s/willy.scr", willy_dir);
    g_free(willy_dir);
    
    return file_path;
}

bool high_score_manager_is_new_day(const char *file_path) {
    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0) {
        return true; // File doesn't exist, treat as new day
    }
    
    // Get current time
    time_t now = time(NULL);
    struct tm *now_tm = localtime(&now);
    
    // Get file modification time
    struct tm *file_tm = localtime(&file_stat.st_mtime);
    
    // Compare dates (year, month, day)
    if (now_tm->tm_year != file_tm->tm_year ||
        now_tm->tm_mon != file_tm->tm_mon ||
        now_tm->tm_mday != file_tm->tm_mday) {
        return true;
    }
    
    return false;
}

void high_score_manager_load_scores(HighScoreManager *manager) {
    char *file_path = high_score_manager_get_score_file_path();

    FILE *file = fopen(file_path, "r");
    if (!file) {
        // File doesn't exist, use defaults
        g_free(file_path);
        return;
    }

    // Read entire file into string
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        g_free(file_path);
        return;
    }

    char *json_content = g_malloc(file_size + 1);
    size_t bytes_read = fread(json_content, 1, file_size, file);
    json_content[bytes_read] = '\0';
    fclose(file);

    if (bytes_read == 0) {
        g_free(json_content);
        g_free(file_path);
        return;
    }

    // Store previous scores for merging
    HighScore previous_scores[10];
    memcpy(previous_scores, manager->permanent_scores, sizeof(previous_scores));

    // Parse JSON and merge with existing scores
    high_score_manager_parse_scores_json(manager, json_content);

    // Merge previous and newly loaded scores, ensuring uniqueness
    for (int i = 0; i < 10; i++) {
        const HighScore *old_score = &previous_scores[i];
        bool found = false;
        
        for (int j = 0; j < 10; j++) {
            const HighScore *new_score = &manager->permanent_scores[j];
            if (strcmp(old_score->name, new_score->name) == 0 && 
                old_score->score == new_score->score) {
                found = true;
                break;
            }
        }
        
        if (!found && strcmp(old_score->name, "nobody") != 0) {
            // Find an empty slot or replace the lowest score
            int insert_pos = -1;
            for (int j = 0; j < 10; j++) {
                if (strcmp(manager->permanent_scores[j].name, "nobody") == 0) {
                    insert_pos = j;
                    break;
                }
            }
            
            if (insert_pos == -1) {
                // Find lowest score to replace
                int lowest_score = manager->permanent_scores[0].score;
                insert_pos = 0;
                for (int j = 1; j < 10; j++) {
                    if (manager->permanent_scores[j].score < lowest_score) {
                        lowest_score = manager->permanent_scores[j].score;
                        insert_pos = j;
                    }
                }
                
                if (old_score->score > lowest_score) {
                    strcpy(manager->permanent_scores[insert_pos].name, old_score->name);
                    manager->permanent_scores[insert_pos].score = old_score->score;
                }
            } else {
                strcpy(manager->permanent_scores[insert_pos].name, old_score->name);
                manager->permanent_scores[insert_pos].score = old_score->score;
            }
        }
    }

    // Sort scores (highest first) - simple bubble sort
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9 - i; j++) {
            if (manager->permanent_scores[j].score < manager->permanent_scores[j + 1].score) {
                HighScore temp = manager->permanent_scores[j];
                manager->permanent_scores[j] = manager->permanent_scores[j + 1];
                manager->permanent_scores[j + 1] = temp;
            }
        }
    }

    // Reset daily scores if it's a new day
    if (high_score_manager_is_new_day(file_path)) {
        for (int i = 0; i < 10; i++) {
            strcpy(manager->daily_scores[i].name, "nobody");
            manager->daily_scores[i].score = 0;
        }
    }

    g_free(json_content);
    g_free(file_path);
}

void high_score_manager_save_scores(HighScoreManager *manager) {
    if (!manager) return;
    
    char *file_path = high_score_manager_get_score_file_path();
    
    FILE *file = fopen(file_path, "w");
    if (!file) {
        printf("Error: Cannot create high score file: %s\n", strerror(errno));
        g_free(file_path);
        return;
    }
    
    fprintf(file, "{\n");
    
    // Save permanent scores
    fprintf(file, "  \"hiscoreP\": [\n");
    for (int i = 0; i < 10; i++) {
        fprintf(file, "    [\"%s\", %d]", 
                manager->permanent_scores[i].name, 
                manager->permanent_scores[i].score);
        if (i < 9) fprintf(file, ",");
        fprintf(file, "\n");
    }
    fprintf(file, "  ],\n");
    
    // Save daily scores
    fprintf(file, "  \"hiscoreT\": [\n");
    for (int i = 0; i < 10; i++) {
        fprintf(file, "    [\"%s\", %d]", 
                manager->daily_scores[i].name, 
                manager->daily_scores[i].score);
        if (i < 9) fprintf(file, ",");
        fprintf(file, "\n");
    }
    fprintf(file, "  ]\n");
    
    fprintf(file, "}\n");
    
    fclose(file);
    g_free(file_path);
}

void high_score_manager_parse_scores_json(HighScoreManager *manager, const char *json_content) {
    // Simple JSON parser for our specific format
    char *line;
    char *json_copy = g_strdup(json_content);
    char *saveptr;
    bool in_permanent = false;
    bool in_daily = false;
    int permanent_index = 0;
    int daily_index = 0;
    
    line = strtok_r(json_copy, "\n", &saveptr);
    while (line != NULL) {
        // Remove leading and trailing whitespace
        while (*line == ' ' || *line == '\t') line++;
        
        char *end = line + strlen(line) - 1;
        while (end > line && (*end == ' ' || *end == '\t' || *end == '\r')) {
            *end = '\0';
            end--;
        }
        
        if (strstr(line, "\"hiscoreP\"") != NULL) {
            in_permanent = true;
            in_daily = false;
            permanent_index = 0;
            // Initialize permanent scores
            for (int i = 0; i < 10; i++) {
                strcpy(manager->permanent_scores[i].name, "nobody");
                manager->permanent_scores[i].score = 0;
            }
        } else if (strstr(line, "\"hiscoreT\"") != NULL) {
            in_daily = true;
            in_permanent = false;
            daily_index = 0;
            // Initialize daily scores
            for (int i = 0; i < 10; i++) {
                strcpy(manager->daily_scores[i].name, "nobody");
                manager->daily_scores[i].score = 0;
            }
        } else if (strstr(line, "[\"") != NULL) {
            // Parse score entry: ["name", score]
            HighScore score_entry = high_score_manager_parse_score_line(line);
            if (strlen(score_entry.name) > 0) {
                if (in_permanent && permanent_index < 10) {
                    manager->permanent_scores[permanent_index] = score_entry;
                    permanent_index++;
                } else if (in_daily && daily_index < 10) {
                    manager->daily_scores[daily_index] = score_entry;
                    daily_index++;
                }
            }
        }
        
        line = strtok_r(NULL, "\n", &saveptr);
    }
    
    g_free(json_copy);
    
    // Sort scores (highest first) - bubble sort for permanent scores
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9 - i; j++) {
            if (manager->permanent_scores[j].score < manager->permanent_scores[j + 1].score) {
                HighScore temp = manager->permanent_scores[j];
                manager->permanent_scores[j] = manager->permanent_scores[j + 1];
                manager->permanent_scores[j + 1] = temp;
            }
        }
    }
    
    // Sort daily scores too
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9 - i; j++) {
            if (manager->daily_scores[j].score < manager->daily_scores[j + 1].score) {
                HighScore temp = manager->daily_scores[j];
                manager->daily_scores[j] = manager->daily_scores[j + 1];
                manager->daily_scores[j + 1] = temp;
            }
        }
    }
}

HighScore high_score_manager_parse_score_line(const char *line) {
    HighScore result = {"", 0};
    
    // Parse a line like: ["name", score]
    const char *start = strstr(line, "[\"");
    if (!start) return result;
    
    start += 2; // Skip ["
    const char *name_end = strstr(start, "\",");
    if (!name_end) return result;
    
    // Extract name
    size_t name_len = name_end - start;
    if (name_len >= sizeof(result.name)) {
        name_len = sizeof(result.name) - 1;
    }
    strncpy(result.name, start, name_len);
    result.name[name_len] = '\0';
    
    // Extract score
    const char *score_start = name_end + 3; // Skip ", 
    const char *score_end = strchr(score_start, ']');
    if (!score_end) return result;
    
    char score_str[32];
    size_t score_len = score_end - score_start;
    if (score_len >= sizeof(score_str)) {
        score_len = sizeof(score_str) - 1;
    }
    strncpy(score_str, score_start, score_len);
    score_str[score_len] = '\0';
    
    result.score = atoi(score_str);
    
    return result;
}

bool high_score_manager_is_high_score(HighScoreManager *manager, int score) {
    if (!manager) return false;
    return score > manager->daily_scores[9].score;
}

bool high_score_manager_is_permanent_high_score(HighScoreManager *manager, int score) {
    if (!manager) return false;
    return score > manager->permanent_scores[9].score;
}

void high_score_manager_add_score(HighScoreManager *manager, const char *name, int score) {
    if (!manager || !name) return;
    
    // Add to daily scores
    // Find insertion point
    int insert_pos = 10;
    for (int i = 0; i < 10; i++) {
        if (score > manager->daily_scores[i].score) {
            insert_pos = i;
            break;
        }
    }
    
    if (insert_pos < 10) {
        // Shift scores down
        for (int i = 9; i > insert_pos; i--) {
            manager->daily_scores[i] = manager->daily_scores[i - 1];
        }
        
        // Insert new score
        strncpy(manager->daily_scores[insert_pos].name, name, sizeof(manager->daily_scores[insert_pos].name) - 1);
        manager->daily_scores[insert_pos].name[sizeof(manager->daily_scores[insert_pos].name) - 1] = '\0';
        manager->daily_scores[insert_pos].score = score;
    }
    
    // Add to permanent scores
    insert_pos = 10;
    for (int i = 0; i < 10; i++) {
        if (score > manager->permanent_scores[i].score) {
            insert_pos = i;
            break;
        }
    }
    
    if (insert_pos < 10) {
        // Shift scores down
        for (int i = 9; i > insert_pos; i--) {
            manager->permanent_scores[i] = manager->permanent_scores[i - 1];
        }
        
        // Insert new score
        strncpy(manager->permanent_scores[insert_pos].name, name, sizeof(manager->permanent_scores[insert_pos].name) - 1);
        manager->permanent_scores[insert_pos].name[sizeof(manager->permanent_scores[insert_pos].name) - 1] = '\0';
        manager->permanent_scores[insert_pos].score = score;
    }
    
    high_score_manager_save_scores(manager);
}

char* high_score_manager_get_score_description(HighScoreManager *manager, int score) {
    (void)manager; // Unused parameter
    
    if (score < 1000) {
        return g_strdup("Didn't you even read the instructions?");
    } else if (score < 2000) {
        return g_strdup("If you can't say anything nice...");
    } else if (score < 3000) {
        return g_strdup("Okay. Maybe you're not so bad after all.");
    } else if (score < 4000) {
        return g_strdup("Wow! Absolutely mediocre!");
    } else if (score < 5000) {
        return g_strdup("Pretty darn good, for a vertebrate!");
    } else if (score < 6000) {
        return g_strdup("Well done! Do you often eat garbage?");
    } else {
        return g_strdup("Absolutely fantastic! You should consider a career as an earthworm!");
    }
}

char* high_score_manager_get_achievement_message(HighScoreManager *manager, int score) {
    if (!manager) return g_strdup("");
    
    if (high_score_manager_is_permanent_high_score(manager, score)) {
        return g_strdup("You're an Official Nightcrawler!");
    } else if (high_score_manager_is_high_score(manager, score)) {
        return g_strdup("You're a Daily Pinworm!");
    }
    return g_strdup("");
}

// High score drawing functions (previously in WillyGame class)
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
    for (int i = 0; i < 10; i++) {
        char score_line[64];
        snprintf(score_line, sizeof(score_line), "%2d     %5d     %s",
                i + 1, game->score_manager->permanent_scores[i].score, 
                game->score_manager->permanent_scores[i].name);
        
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
    for (int i = 0; i < 10; i++) {
        char score_line[64];
        snprintf(score_line, sizeof(score_line), "%2d     %5d     %s",
                i + 1, game->score_manager->daily_scores[i].score, 
                game->score_manager->daily_scores[i].name);
        
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
