#include "willy.h"

SoundManager::SoundManager() : sound_enabled(true), initialized(false) {}

SoundManager::~SoundManager() {
    cleanup();
}

bool SoundManager::initialize() {
    if (initialized) return true;
    
    // Initialize SDL Audio
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cout << "SDL Audio initialization failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize SDL_mixer
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        std::cout << "SDL_mixer initialization failed: " << Mix_GetError() << std::endl;
        SDL_Quit();
        return false;
    }
    
    initialized = true;
    std::cout << "SDL Audio initialized successfully" << std::endl;
    return true;
}

void SoundManager::cleanup() {
    if (!initialized) return;
    
    std::lock_guard<std::mutex> lock(sound_mutex);
    
    // Free all cached sounds
    for (auto& pair : sound_cache) {
        if (pair.second) {
            Mix_FreeChunk(pair.second);
        }
    }
    sound_cache.clear();
    
    // Cleanup SDL_mixer and SDL
    Mix_CloseAudio();
    SDL_Quit();
    
    initialized = false;
    std::cout << "SDL Audio cleaned up" << std::endl;
}

std::string SoundManager::find_sound_file(const std::string& filename) {
    std::vector<std::string> possible_paths = {
        filename,
        "audio/" + filename,
        "data/audio/" + filename,
        "/usr/games/willytheworm/audio/" + filename
    };
    
    for (const auto& path : possible_paths) {
        std::ifstream file(path);
        if (file.good()) {
            return path;
        }
    }
    
    std::cout << "Sound file not found: " << filename << std::endl;
    return "";
}

void SoundManager::play_sound(const std::string& filename) {
    if (!sound_enabled || !initialized) {
        return;
    }
    
    // Use a separate thread for sound loading/playing to avoid blocking
    std::thread sound_thread([this, filename]() {
        std::lock_guard<std::mutex> lock(sound_mutex);
        
        // Check if sound is already cached
        auto it = sound_cache.find(filename);
        Mix_Chunk* sound = nullptr;
        
        if (it != sound_cache.end()) {
            sound = it->second;
        } else {
            // Load the sound file
            std::string sound_path = find_sound_file(filename);
            if (!sound_path.empty()) {
                sound = Mix_LoadWAV(sound_path.c_str());
                if (sound) {
                    sound_cache[filename] = sound;
                    std::cout << "Loaded sound: " << filename << std::endl;
                } else {
                    std::cout << "Failed to load sound " << filename << ": " << Mix_GetError() << std::endl;
                    return;
                }
            } else {
                return;
            }
        }
        
        // Play the sound
        if (sound) {
            if (Mix_PlayChannel(-1, sound, 0) == -1) {
                std::cout << "Failed to play sound " << filename << ": " << Mix_GetError() << std::endl;
            }
        }
    });
    
    sound_thread.detach(); // Let the thread run independently
}

