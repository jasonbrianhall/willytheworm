#include "willy.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>

// HighScoreManager implementation
HighScoreManager::HighScoreManager() {
    // Initialize empty score lists
    for(int i = 0; i < 10; i++) {
        permanent_scores.push_back({"nobody", 0});
        daily_scores.push_back({"nobody", 0});
    }
    
    load_scores();
}

HighScoreManager::~HighScoreManager() {
    save_scores();
}

std::string HighScoreManager::get_score_file_path() {
    // Get home directory
    const char* home = getenv("HOME");
    if(!home) {
        home = getenv("USERPROFILE"); // Windows fallback
        if(!home) {
            return "willy.scr"; // Fallback to current directory
        }
    }
    
    std::string home_dir(home);
    std::string willy_dir = home_dir + "/.willytheworm";
    
    // Create directory if it doesn't exist
    #ifdef _WIN32
        _mkdir(willy_dir.c_str());
    #else
        mkdir(willy_dir.c_str(), 0755);
    #endif
    
    return willy_dir + "/willy.scr";
}

bool HighScoreManager::is_new_day(const std::string& file_path) {
    struct stat file_stat;
    if(stat(file_path.c_str(), &file_stat) != 0) {
        return true; // File doesn't exist, treat as new day
    }
    
    // Get current time
    time_t now = time(0);
    tm* now_tm = localtime(&now);
    
    // Get file modification time
    tm* file_tm = localtime(&file_stat.st_mtime);
    
    // Compare dates (year, month, day)
    if(now_tm->tm_year != file_tm->tm_year ||
       now_tm->tm_mon != file_tm->tm_mon ||
       now_tm->tm_mday != file_tm->tm_mday) {
        return true;
    }
    
    return false;
}

void HighScoreManager::load_scores() {
    std::string file_path = get_score_file_path();

    try {
        std::ifstream file(file_path);
        if (!file.good()) {
            // File doesn't exist, use defaults
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_content = buffer.str();

        if (json_content.empty()) {
            return;
        }

        // Parse JSON and merge with existing scores
        std::vector<HighScore> previous_scores = permanent_scores; // Keep old scores
        parse_scores_json(json_content);

        // Merge previous and newly loaded scores, ensuring uniqueness
        for (const auto& old_score : previous_scores) {
            if (std::none_of(permanent_scores.begin(), permanent_scores.end(), [&](const HighScore& s) {
                return s.name == old_score.name && s.score == old_score.score;
            })) {
                permanent_scores.push_back(old_score);
            }
        }

        // Sort scores (highest first) and limit to top 10
        std::sort(permanent_scores.begin(), permanent_scores.end(), 
                  [](const HighScore& a, const HighScore& b) { return a.score > b.score; });
        if (permanent_scores.size() > 10) {
            permanent_scores.resize(10);
        }

        // Reset daily scores if it's a new day
        if (is_new_day(file_path)) {
            daily_scores.clear();
            for (int i = 0; i < 10; i++) {
                daily_scores.push_back({"nobody", 0});
            }
        }

    } catch (const std::exception& e) {
        std::cout << "Error loading high scores: " << e.what() << std::endl;
        // Preserve existing scores in case of failure
    }
}

void HighScoreManager::save_scores() {
    std::string file_path = get_score_file_path();
    
    try {
        std::ofstream file(file_path);
        if(!file) {
            std::cout << "Error: Cannot create high score file" << std::endl;
            return;
        }
        
        file << "{\n";
        
        // Save permanent scores
        file << "  \"hiscoreP\": [\n";
        for(size_t i = 0; i < permanent_scores.size(); i++) {
            file << "    [\"" << permanent_scores[i].name << "\", " << permanent_scores[i].score << "]";
            if(i < permanent_scores.size() - 1) file << ",";
            file << "\n";
        }
        file << "  ],\n";
        
        // Save daily scores
        file << "  \"hiscoreT\": [\n";
        for(size_t i = 0; i < daily_scores.size(); i++) {
            file << "    [\"" << daily_scores[i].name << "\", " << daily_scores[i].score << "]";
            if(i < daily_scores.size() - 1) file << ",";
            file << "\n";
        }
        file << "  ]\n";
        
        file << "}\n";
        
    } catch(const std::exception& e) {
        std::cout << "Error saving high scores: " << e.what() << std::endl;
    }
}

void HighScoreManager::parse_scores_json(const std::string& json_content) {
    // Simple JSON parser for our specific format
    std::istringstream stream(json_content);
    std::string line;
    bool in_permanent = false;
    bool in_daily = false;
    
    while(std::getline(stream, line)) {
        // Remove whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if(line.find("\"hiscoreP\"") != std::string::npos) {
            in_permanent = true;
            in_daily = false;
            permanent_scores.clear();
        } else if(line.find("\"hiscoreT\"") != std::string::npos) {
            in_daily = true;
            in_permanent = false;
            daily_scores.clear();
        } else if(line.find("[\"") != std::string::npos) {
            // Parse score entry: ["name", score]
            HighScore score_entry = parse_score_line(line);
            if(!score_entry.name.empty()) {
                if(in_permanent) {
                    permanent_scores.push_back(score_entry);
                } else if(in_daily) {
                    daily_scores.push_back(score_entry);
                }
            }
        }
    }
    
    // Ensure we have exactly 10 entries in each list
    while(permanent_scores.size() < 10) {
        permanent_scores.push_back({"nobody", 0});
    }
    while(daily_scores.size() < 10) {
        daily_scores.push_back({"nobody", 0});
    }
    
    // Sort scores (highest first)
    std::sort(permanent_scores.begin(), permanent_scores.end(), 
              [](const HighScore& a, const HighScore& b) { return a.score > b.score; });
    std::sort(daily_scores.begin(), daily_scores.end(), 
              [](const HighScore& a, const HighScore& b) { return a.score > b.score; });
}

HighScore HighScoreManager::parse_score_line(const std::string& line) {
    // Parse a line like: ["name", score]
    size_t start = line.find("[\"");
    if(start == std::string::npos) return {"", 0};
    
    start += 2; // Skip ["
    size_t name_end = line.find("\",", start);
    if(name_end == std::string::npos) return {"", 0};
    
    std::string name = line.substr(start, name_end - start);
    
    size_t score_start = name_end + 3; // Skip ", 
    size_t score_end = line.find("]", score_start);
    if(score_end == std::string::npos) return {"", 0};
    
    std::string score_str = line.substr(score_start, score_end - score_start);
    int score = 0;
    try {
        score = std::stoi(score_str);
    } catch(...) {
        return {"", 0};
    }
    
    return {name, score};
}

bool HighScoreManager::is_high_score(int score) {
    return score > daily_scores.back().score;
}

bool HighScoreManager::is_permanent_high_score(int score) {
    return score > permanent_scores.back().score;
}

void HighScoreManager::add_score(const std::string& name, int score) {
    // Add to daily scores
    daily_scores.push_back({name, score});
    std::sort(daily_scores.begin(), daily_scores.end(), 
              [](const HighScore& a, const HighScore& b) { return a.score > b.score; });
    daily_scores.resize(10); // Keep only top 10
    
    // Add to permanent scores
    permanent_scores.push_back({name, score});
    std::sort(permanent_scores.begin(), permanent_scores.end(), 
              [](const HighScore& a, const HighScore& b) { return a.score > b.score; });
    permanent_scores.resize(10); // Keep only top 10
    
    save_scores();
}

std::string HighScoreManager::get_score_description(int score) {
    if(score < 1000) {
        return "Didn't you even read the instructions?";
    } else if(score < 2000) {
        return "If you can't say anything nice...";
    } else if(score < 3000) {
        return "Okay. Maybe you're not so bad after all.";
    } else if(score < 4000) {
        return "Wow! Absolutely mediocre!";
    } else if(score < 5000) {
        return "Pretty darn good, for a vertebrate!";
    } else if(score < 6000) {
        return "Well done! Do you often eat garbage?";
    } else {
        return "Absolutely fantastic! You should consider a career as an earthworm!";
    }
}

std::string HighScoreManager::get_achievement_message(int score) {
    if(is_permanent_high_score(score)) {
        return "You're an Official Nightcrawler!";
    } else if(is_high_score(score)) {
        return "You're a Daily Pinworm!";
    }
    return "";
}

std::vector<HighScore> HighScoreManager::get_permanent_scores() const {
    return permanent_scores;
}

std::vector<HighScore> HighScoreManager::get_daily_scores() const {
    return daily_scores;
}

void WillyGame::draw_high_score_entry_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Get menubar height to use as offset
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    int menubar_height = menubar_min.height;
    
    // Blue background
    cr->set_source_rgb(0.0, 0.0, 1.0);
    cr->paint();
    
    // Create font
    Pango::FontDescription font_desc;
    font_desc.set_family("Courier");
    font_desc.set_size(20 * PANGO_SCALE);
    
    auto layout = create_pango_layout("");
    layout->set_font_description(font_desc);
    
    cr->set_source_rgb(1.0, 1.0, 1.0); // White text
    
    int y_offset = menubar_height + 50;
    int line_height = 35;
    
    // Achievement message
    std::string achievement = score_manager->get_achievement_message(score);
    if(!achievement.empty()) {
        layout->set_text(achievement);
        int text_width, text_height;
        layout->get_pixel_size(text_width, text_height);
        int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
        cr->move_to(x_offset, y_offset);
        layout->show_in_cairo_context(cr);
        y_offset += line_height * 2;
    }
    
    // Score description
    std::string description = score_manager->get_score_description(score);
    layout->set_text(description);
    int text_width, text_height;
    layout->get_pixel_size(text_width, text_height);
    int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
    y_offset += line_height * 2;
    
    // Score
    std::string score_text = "Your score for this game is " + std::to_string(score) + "...";
    layout->set_text(score_text);
    layout->get_pixel_size(text_width, text_height);
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
    y_offset += line_height * 2;
    
    // Name entry prompt
    std::string prompt = "Enter your name >> " + name_input;
    layout->set_text(prompt);
    layout->get_pixel_size(text_width, text_height);
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
}

void WillyGame::draw_high_score_display_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Get menubar height to use as offset
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    int menubar_height = menubar_min.height;
    
    // Blue background
    cr->set_source_rgb(0.0, 0.0, 1.0);
    cr->paint();
    
    // Create fonts
    Pango::FontDescription header_font;
    header_font.set_family("Courier");
    header_font.set_size(24 * PANGO_SCALE);
    
    Pango::FontDescription score_font;
    score_font.set_family("Courier");
    score_font.set_size(16 * PANGO_SCALE);
    
    auto layout = create_pango_layout("");
    
    int y_offset = menubar_height + 20;
    int line_height = 25;
    
    // All-time Nightcrawlers header
    layout->set_font_description(header_font);
    cr->set_source_rgb(1.0, 1.0, 0.0); // Yellow
    layout->set_text("All-time Nightcrawlers");
    int text_width, text_height;
    layout->get_pixel_size(text_width, text_height);
    int x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
    y_offset += line_height + 10;
    
    // Draw black background for table
    int table_width = GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor / 2;
    int table_x = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - table_width) / 2;
    cr->set_source_rgb(0.0, 0.0, 0.0);
    cr->rectangle(table_x, y_offset, table_width, line_height * 10);
    cr->fill();
    
    // Permanent scores
    layout->set_font_description(score_font);
    cr->set_source_rgb(1.0, 1.0, 0.0); // Yellow
    auto permanent_scores = score_manager->get_permanent_scores();
    for(int i = 0; i < 10 && i < (int)permanent_scores.size(); i++) {
        std::ostringstream oss;
        oss << std::setw(2) << (i + 1) << "     " 
            << std::setw(5) << permanent_scores[i].score << "     " 
            << permanent_scores[i].name;
        
        layout->set_text(oss.str());
        cr->move_to(table_x + 10, y_offset);
        layout->show_in_cairo_context(cr);
        y_offset += line_height;
    }
    
    y_offset += 30;
    
    // Today's Best Pinworms header
    layout->set_font_description(header_font);
    cr->set_source_rgb(0.0, 1.0, 1.0); // Cyan
    layout->set_text("Today's Best Pinworms");
    layout->get_pixel_size(text_width, text_height);
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
    y_offset += line_height + 10;
    
    // Draw black background for daily table
    cr->set_source_rgb(0.0, 0.0, 0.0);
    cr->rectangle(table_x, y_offset, table_width, line_height * 10);
    cr->fill();
    
    // Daily scores
    layout->set_font_description(score_font);
    cr->set_source_rgb(0.0, 1.0, 1.0); // Cyan
    auto daily_scores = score_manager->get_daily_scores();
    for(int i = 0; i < 10 && i < (int)daily_scores.size(); i++) {
        std::ostringstream oss;
        oss << std::setw(2) << (i + 1) << "     " 
            << std::setw(5) << daily_scores[i].score << "     " 
            << daily_scores[i].name;
        
        layout->set_text(oss.str());
        cr->move_to(table_x + 10, y_offset);
        layout->show_in_cairo_context(cr);
        y_offset += line_height;
    }
    
    y_offset += 30;
    
    // Instructions
    layout->set_font_description(score_font);
    cr->set_source_rgb(1.0, 1.0, 1.0); // White
    layout->set_text("Hit any key to play again or ESC to exit");
    layout->get_pixel_size(text_width, text_height);
    x_offset = (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
    cr->move_to(x_offset, y_offset);
    layout->show_in_cairo_context(cr);
}
