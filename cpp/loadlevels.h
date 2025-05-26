#ifndef LOADLEVELS_H
#define LOADLEVELS_H

#include <map>
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <fstream>
#include <iostream>

// Game constants
const int GAME_SCREEN_WIDTH = 40;
const int GAME_SCREEN_HEIGHT = 30;

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

#endif // LOADLEVELS_H
