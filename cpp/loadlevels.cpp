#include "loadlevels.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

// LevelLoader implementation
LevelLoader::LevelLoader() {}

LevelLoader::~LevelLoader() {}

std::string LevelLoader::find_levels_file(const std::string& filename) {
    std::vector<std::string> possible_paths = {
        filename,
        "data/" + filename,
        "/usr/games/willytheworm/data/" + filename
    };
    
    for(const auto& path : possible_paths) {
        std::ifstream file(path);
        if(file.good()) {
            return path;
        }
    }
    return "";
}

bool LevelLoader::load_levels(const std::string& filename) {
    std::string levels_path = find_levels_file(filename);
    
    if(levels_path.empty()) {
        std::cout << "Could not find " << filename << ", creating default levels..." << std::endl;
        create_default_levels();
        return false;
    }
    
    try {
        std::ifstream file(levels_path);
        if(!file) {
            throw std::runtime_error("Cannot open levels file");
        }
        
        // Read entire file into string
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_content = buffer.str();
        
        // Parse JSON content
        if(!parse_json_file(json_content)) {
            throw std::runtime_error("Failed to parse JSON content");
        }
        
        // Create backup copy
        original_level_data = level_data;
        
        std::cout << "Loaded levels from " << levels_path << std::endl;
        return true;
        
    } catch(const std::exception& e) {
        std::cout << "Error loading levels file: " << e.what() << std::endl;
        std::cout << "Creating default levels..." << std::endl;
        create_default_levels();
        return false;
    }
}

void LevelLoader::create_default_levels() {
    // Clear existing data
    level_data.clear();
    original_level_data.clear();
    ball_pit_data.clear();
    
    // Create level1
    std::string level_name = "level1";
    
    // Initialize empty level
    for(int row = 0; row < GAME_SCREEN_HEIGHT; row++) {
        for(int col = 0; col < GAME_SCREEN_WIDTH; col++) {
            level_data[level_name][std::to_string(row)][std::to_string(col)] = "EMPTY";
        }
    }
    
    // Bottom platform
    for(int col = 5; col < 35; col++) {
        level_data[level_name]["24"][std::to_string(col)] = "PIPE1";
    }
    
    // Some ladders
    for(int row = 20; row < 25; row++) {
        level_data[level_name][std::to_string(row)]["10"] = "LADDER";
        level_data[level_name][std::to_string(row)]["30"] = "LADDER";
    }
    
    // Mid platforms
    for(int col = 8; col < 15; col++) {
        level_data[level_name]["20"][std::to_string(col)] = "PIPE1";
    }
    for(int col = 25; col < 32; col++) {
        level_data[level_name]["20"][std::to_string(col)] = "PIPE1";
    }
    
    // Top platforms
    for(int col = 15; col < 25; col++) {
        level_data[level_name]["16"][std::to_string(col)] = "PIPE1";
    }
    
    // Add more ladders
    for(int row = 16; row < 21; row++) {
        level_data[level_name][std::to_string(row)]["20"] = "LADDER";
    }
    
    // Add some game elements
    level_data[level_name]["23"]["12"] = "PRESENT";
    level_data[level_name]["23"]["28"] = "PRESENT";
    level_data[level_name]["19"]["9"] = "PRESENT";
    level_data[level_name]["19"]["31"] = "PRESENT";
    level_data[level_name]["15"]["20"] = "PRESENT";
    
    level_data[level_name]["19"]["15"] = "UPSPRING";
    level_data[level_name]["19"]["25"] = "SIDESPRING";
    level_data[level_name]["15"]["17"] = "UPSPRING";
    
    level_data[level_name]["12"]["20"] = "BELL";  // Goal
    
    level_data[level_name]["23"]["18"] = "TACK";  // Danger
    level_data[level_name]["23"]["22"] = "TACK";  // Danger
    
    // Ball pit
    level_data[level_name]["24"]["20"] = "BALLPIT";
    
    // Set Willy's starting position
    level_data[level_name]["23"]["7"] = "WILLY_RIGHT";
    
    // Set up ball pit data
    ball_pit_data[level_name + "PIT"]["PRIMARYBALLPIT"] = {24, 20};
    
    // Create level2 (slightly different)
    level_name = "level2";
    
    // Copy level1 as base
    level_data[level_name] = level_data["level1"];
    
    // Modify some elements for level2
    level_data[level_name]["15"]["20"] = "TACK";  // Make it harder
    level_data[level_name]["19"]["20"] = "PRESENT"; // Add present on ladder
    level_data[level_name]["23"]["15"] = "SIDESPRING"; // Add more springs
    
    // Set up ball pit data for level2
    ball_pit_data[level_name + "PIT"]["PRIMARYBALLPIT"] = {24, 20};
    
    // Create backup copy
    original_level_data = level_data;
    
    std::cout << "Created default levels" << std::endl;
}

std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> 
LevelLoader::get_level_data() const {
    return level_data;
}

std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> 
LevelLoader::get_original_level_data() const {
    return original_level_data;
}

std::map<std::string, std::map<std::string, std::pair<int, int>>> 
LevelLoader::get_ball_pit_data() const {
    return ball_pit_data;
}

void LevelLoader::reset_levels() {
    level_data = original_level_data;
}

int LevelLoader::get_max_levels() const {
    int max_levels = 0;
    for(const auto& [level_name, level_content] : level_data) {
        if(level_name.find("level") != std::string::npos && level_name.find("PIT") == std::string::npos) {
            max_levels++;
        }
    }
    return max_levels;
}

bool LevelLoader::level_exists(const std::string& level_name) const {
    return level_data.find(level_name) != level_data.end();
}

std::string LevelLoader::get_tile(const std::string& level_name, int row, int col) const {
    auto level_it = level_data.find(level_name);
    if(level_it != level_data.end()) {
        auto row_it = level_it->second.find(std::to_string(row));
        if(row_it != level_it->second.end()) {
            auto col_it = row_it->second.find(std::to_string(col));
            if(col_it != row_it->second.end()) {
                return col_it->second;
            }
        }
    }
    return "EMPTY";
}

void LevelLoader::set_tile(const std::string& level_name, int row, int col, const std::string& tile) {
    if(row >= 0 && row < GAME_SCREEN_HEIGHT && col >= 0 && col < GAME_SCREEN_WIDTH) {
        level_data[level_name][std::to_string(row)][std::to_string(col)] = tile;
    }
}

std::pair<int, int> LevelLoader::get_willy_start_position(const std::string& level_name) const {
    auto level_it = level_data.find(level_name);
    if(level_it != level_data.end()) {
        for(const auto& [row_str, row_content] : level_it->second) {
            for(const auto& [col_str, tile] : row_content) {
                if(tile.find("WILLY") != std::string::npos) {
                    return {std::stoi(row_str), std::stoi(col_str)};
                }
            }
        }
    }
    return {23, 7}; // Default position
}

std::pair<int, int> LevelLoader::get_ball_pit_position(const std::string& level_name) const {
    std::string pit_name = level_name + "PIT";
    auto pit_it = ball_pit_data.find(pit_name);
    if(pit_it != ball_pit_data.end()) {
        auto primary_it = pit_it->second.find("PRIMARYBALLPIT");
        if(primary_it != pit_it->second.end()) {
            return primary_it->second;
        }
    }
    return {24, 20}; // Default position
}

// Simple JSON parsing helpers
std::string LevelLoader::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\"");
    if(start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r\"");
    return str.substr(start, end - start + 1);
}

std::string LevelLoader::extract_string_value(const std::string& line) {
    size_t colon = line.find(':');
    if(colon == std::string::npos) return "";
    
    std::string value = line.substr(colon + 1);
    // Remove trailing comma if present
    size_t comma = value.find_last_of(',');
    if(comma != std::string::npos && comma == value.length() - 1) {
        value = value.substr(0, comma);
    }
    
    return trim(value);
}

std::vector<int> LevelLoader::extract_array_value(const std::string& line) {
    std::vector<int> result;
    size_t start = line.find('[');
    size_t end = line.find(']');
    
    if(start == std::string::npos || end == std::string::npos) {
        return result;
    }
    
    std::string array_content = line.substr(start + 1, end - start - 1);
    std::stringstream ss(array_content);
    std::string item;
    
    while(std::getline(ss, item, ',')) {
        item = trim(item);
        if(!item.empty()) {
            try {
                result.push_back(std::stoi(item));
            } catch(...) {
                // Skip invalid numbers
            }
        }
    }
    
    return result;
}

bool LevelLoader::parse_json_file(const std::string& content) {
    level_data.clear();
    original_level_data.clear();
    ball_pit_data.clear();
    
    std::istringstream stream(content);
    std::string line;
    std::string current_level;
    std::string current_row;
    bool in_level = false;
    bool in_row = false;
    bool in_pit = false;
    
    while(std::getline(stream, line)) {
        line = trim(line);
        if(line.empty() || line[0] == '{' || line[0] == '}') continue;
        
        // Check for level start
        if(line.find("\"level") != std::string::npos && line.find(":") != std::string::npos) {
            size_t quote1 = line.find('"');
            size_t quote2 = line.find('"', quote1 + 1);
            if(quote1 != std::string::npos && quote2 != std::string::npos) {
                current_level = line.substr(quote1 + 1, quote2 - quote1 - 1);
                in_level = true;
                in_pit = (current_level.find("PIT") != std::string::npos);
            }
            continue;
        }
        
        if(in_level && !in_pit) {
            // Check for row start
            if(line.find("\"") != std::string::npos && line.find(":") != std::string::npos && 
               line.find("EMPTY") == std::string::npos && line.find("PIPE") == std::string::npos) {
                size_t quote1 = line.find('"');
                size_t quote2 = line.find('"', quote1 + 1);
                if(quote1 != std::string::npos && quote2 != std::string::npos) {
                    current_row = line.substr(quote1 + 1, quote2 - quote1 - 1);
                    in_row = true;
                }
                continue;
            }
            
            // Parse tile data
            if(in_row && line.find("\"") != std::string::npos && line.find(":") != std::string::npos) {
                size_t quote1 = line.find('"');
                size_t quote2 = line.find('"', quote1 + 1);
                if(quote1 != std::string::npos && quote2 != std::string::npos) {
                    std::string col = line.substr(quote1 + 1, quote2 - quote1 - 1);
                    std::string tile = extract_string_value(line);
                    if(!tile.empty()) {
                        level_data[current_level][current_row][col] = tile;
                    }
                }
            }
        } else if(in_pit) {
            // Parse ball pit data
            if(line.find("PRIMARYBALLPIT") != std::string::npos) {
                auto coords = extract_array_value(line);
                if(coords.size() >= 2) {
                    ball_pit_data[current_level]["PRIMARYBALLPIT"] = {coords[0], coords[1]};
                }
            }
        }
        
        // Reset states when we encounter closing braces or end sections
        if(line.find("}") != std::string::npos) {
            in_row = false;
            if(in_level) {
                in_level = false;
                in_pit = false;
            }
        }
    }
    
    // Create backup copy
    original_level_data = level_data;
    return true;
}

bool LevelLoader::save_levels(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if(!file) {
            return false;
        }
        
        file << "{\n";
        
        // Save level data
        bool first_level = true;
        for(const auto& [level_name, level_content] : level_data) {
            if(!first_level) file << ",\n";
            first_level = false;
            
            file << "  \"" << level_name << "\": {\n";
            
            bool first_row = true;
            for(const auto& [row_str, row_content] : level_content) {
                if(!first_row) file << ",\n";
                first_row = false;
                
                file << "    \"" << row_str << "\": {\n";
                
                bool first_col = true;
                for(const auto& [col_str, tile] : row_content) {
                    if(!first_col) file << ",\n";
                    first_col = false;
                    
                    file << "      \"" << col_str << "\": \"" << tile << "\"";
                }
                
                file << "\n    }";
            }
            
            file << "\n  }";
        }
        
        // Save ball pit data
        for(const auto& [pit_name, pit_content] : ball_pit_data) {
            file << ",\n  \"" << pit_name << "\": {\n";
            
            bool first_pit = true;
            for(const auto& [pit_type, coordinates] : pit_content) {
                if(!first_pit) file << ",\n";
                first_pit = false;
                
                file << "    \"" << pit_type << "\": [" << coordinates.first << ", " << coordinates.second << "]";
            }
            
            file << "\n  }";
        }
        
        file << "\n}\n";
        return true;
        
    } catch(const std::exception& e) {
        std::cout << "Error saving levels: " << e.what() << std::endl;
        return false;
    }
}
