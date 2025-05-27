#include "willy.h"
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
    
    std::cout << "Searching for levels file: " << filename << std::endl;
    for(const auto& path : possible_paths) {
        std::cout << "  Checking: " << path << std::endl;
        std::ifstream file(path);
        if(file.good()) {
            std::cout << "  Found at: " << path << std::endl;
            return path;
        }
    }
    std::cout << "Could not find levels file in any location" << std::endl;
    return "";
}

bool LevelLoader::load_levels(const std::string& filename) {
    std::string levels_path = find_levels_file(filename);
    
    if(levels_path.empty()) {
        std::cout << "ERROR: Could not find " << filename << std::endl;
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
        
        std::cout << "Successfully read " << json_content.length() << " characters from " << levels_path << std::endl;
        std::cout << "First 200 characters of JSON content:" << std::endl;
        std::cout << json_content.substr(0, 200) << "..." << std::endl;
        
        // Parse JSON content
        if(!parse_json_file(json_content)) {
            throw std::runtime_error("Failed to parse JSON content");
        }
        
        // Create backup copy
        original_level_data = level_data;
        
        std::cout << "Successfully loaded " << level_data.size() << " entries from " << levels_path << std::endl;
        std::cout << "Loaded levels:" << std::endl;
        for(const auto& [level_name, level_content] : level_data) {
            if(level_name.find("level") != std::string::npos && level_name.find("PIT") == std::string::npos) {
                std::cout << "  - " << level_name << " (rows: " << level_content.size() << ")" << std::endl;
            }
        }
        
        return true;
        
    } catch(const std::exception& e) {
        std::cout << "ERROR loading levels file: " << e.what() << std::endl;
        return false;
    }
}

void LevelLoader::create_default_levels() {
    // This function has been removed as requested
    std::cout << "ERROR: create_default_levels() called but not implemented" << std::endl;
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
    std::cout << "get_max_levels() returning: " << max_levels << std::endl;
    return max_levels;
}

bool LevelLoader::level_exists(const std::string& level_name) const {
    bool exists = level_data.find(level_name) != level_data.end();
    std::cout << "Checking if level '" << level_name << "' exists: " << (exists ? "YES" : "NO") << std::endl;
    if(!exists) {
        std::cout << "Available levels:" << std::endl;
        for(const auto& [name, content] : level_data) {
            std::cout << "  - '" << name << "'" << std::endl;
        }
    }
    return exists;
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
/*    std::string pit_name = level_name + "PIT";
    auto pit_it = ball_pit_data.find(pit_name);
    if(pit_it != ball_pit_data.end()) {
        auto primary_it = pit_it->second.find("BALLPIT");
        if(primary_it != pit_it->second.end()) {
            return primary_it->second;
        }
    }*/
    printf("Returning the default Ball pit\n");
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
    std::cout << "=== Starting JSON parsing ===" << std::endl;
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
    int line_number = 0;
    
    while(std::getline(stream, line)) {
        line_number++;
        std::string original_line = line;
        line = trim(line);
        
        std::cout << "Line " << line_number << ": '" << line << "'" << std::endl;
        
        if(line.empty() || line[0] == '{' || line[0] == '}') {
            std::cout << "  -> Skipping empty/brace line" << std::endl;
            continue;
        }
        
        // Check for level start - handle case where opening quote was trimmed
        if((line.find("level") != std::string::npos || line.find("\"level") != std::string::npos) && line.find(":") != std::string::npos) {
            size_t level_pos = line.find("level");
            if(level_pos != std::string::npos) {
                // Find the end of the level name (before the quote and colon)
                size_t end_pos = line.find("\":", level_pos);
                if(end_pos == std::string::npos) {
                    end_pos = line.find(":", level_pos);
                }
                if(end_pos != std::string::npos) {
                    current_level = line.substr(level_pos, end_pos - level_pos);
                    in_level = true;
                    in_pit = (current_level.find("PIT") != std::string::npos);
                    std::cout << "  -> Found level: '" << current_level << "'" << (in_pit ? " (PIT)" : "") << std::endl;
                } else {
                    std::cout << "  -> Failed to parse level name from line (no colon found)" << std::endl;
                }
            } else {
                std::cout << "  -> Failed to find 'level' in line" << std::endl;
            }
            continue;
        }
        
        if(in_level && !in_pit) {
            // Check for row start - lines that end with ": {" and contain only digits before the quote
            if(line.find(":") != std::string::npos && line.find("{") != std::string::npos && 
               line.find("EMPTY") == std::string::npos && line.find("PIPE") == std::string::npos &&
               line.find("LADDER") == std::string::npos && line.find("PRESENT") == std::string::npos &&
               line.find("BELL") == std::string::npos && line.find("WILLY") == std::string::npos &&
               line.find("BALLPIT") == std::string::npos && line.find("UPSPRING") == std::string::npos) {
                
                size_t colon_pos = line.find(":");
                if(colon_pos != std::string::npos) {
                    // Extract everything before the colon, removing quotes
                    std::string row_candidate = line.substr(0, colon_pos);
                    // Remove quotes if present
                    if(!row_candidate.empty() && row_candidate.back() == '"') {
                        row_candidate.pop_back();
                    }
                    if(!row_candidate.empty() && row_candidate.front() == '"') {
                        row_candidate = row_candidate.substr(1);
                    }
                    
                    // Check if it's all digits (valid row number)
                    bool all_digits = !row_candidate.empty() && std::all_of(row_candidate.begin(), row_candidate.end(), ::isdigit);
                    if(all_digits) {
                        current_row = row_candidate;
                        in_row = true;
                        std::cout << "  -> Found row: '" << current_row << "'" << std::endl;
                        continue;
                    }
                }
                std::cout << "  -> Not a row start line" << std::endl;
            }
            
            // Parse tile data - lines with quotes, colon, and tile values
            if(in_row && line.find(":") != std::string::npos && 
               (line.find("EMPTY") != std::string::npos || line.find("PIPE") != std::string::npos ||
                line.find("LADDER") != std::string::npos || line.find("PRESENT") != std::string::npos ||
                line.find("BELL") != std::string::npos || line.find("WILLY") != std::string::npos ||
                line.find("BALLPIT") != std::string::npos || line.find("UPSPRING") != std::string::npos ||
                line.find("SIDESPRING") != std::string::npos || line.find("TACK") != std::string::npos ||
                line.find("BALL") != std::string::npos)) {
                
                size_t colon_pos = line.find(":");
                if(colon_pos != std::string::npos) {
                    // Extract column number (everything before colon)
                    std::string col_candidate = line.substr(0, colon_pos);
                    // Remove quotes
                    if(!col_candidate.empty() && col_candidate.back() == '"') {
                        col_candidate.pop_back();
                    }
                    if(!col_candidate.empty() && col_candidate.front() == '"') {
                        col_candidate = col_candidate.substr(1);
                    }
                    
                    std::string tile = extract_string_value(line);
                    if(!tile.empty() && !col_candidate.empty()) {
                        level_data[current_level][current_row][col_candidate] = tile;
                        std::cout << "  -> Set tile [" << current_level << "][" << current_row << "][" << col_candidate << "] = '" << tile << "'" << std::endl;
                    } else {
                        std::cout << "  -> Failed to extract tile value or column from line" << std::endl;
                    }
                } else {
                    std::cout << "  -> No colon found in tile line" << std::endl;
                }
            }
        } else if(in_pit) {
            // Parse ball pit data
            if(line.find("PRIMARYBALLPIT") != std::string::npos) {
                auto coords = extract_array_value(line);
                if(coords.size() >= 2) {
                    ball_pit_data[current_level]["PRIMARYBALLPIT"] = {coords[0], coords[1]};
                    std::cout << "  -> Set ball pit for " << current_level << " at [" << coords[0] << ", " << coords[1] << "]" << std::endl;
                } else {
                    std::cout << "  -> Failed to extract coordinates from ball pit line" << std::endl;
                }
            }
        }
        
        // Reset states when we encounter closing braces or end sections
        if(line.find("}") != std::string::npos) {
            if(in_row) {
                std::cout << "  -> End of row '" << current_row << "'" << std::endl;
                in_row = false;
            } else if(in_level) {
                std::cout << "  -> End of level '" << current_level << "'" << std::endl;
                in_level = false;
                in_pit = false;
            }
        }
    }
    
    std::cout << "=== JSON parsing complete ===" << std::endl;
    std::cout << "Parsed " << level_data.size() << " total entries" << std::endl;
    
    int level_count = 0;
    for(const auto& [name, content] : level_data) {
        if(name.find("level") != std::string::npos && name.find("PIT") == std::string::npos) {
            level_count++;
            std::cout << "Level: " << name << " has " << content.size() << " rows" << std::endl;
        }
    }
    std::cout << "Found " << level_count << " actual game levels" << std::endl;
    
    // Create backup copy
    original_level_data = level_data;
    return level_count > 0;
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
