#include "highscores.h"
#include <fstream>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(dir) _mkdir(dir)
    #define PATH_SEP "\\"
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define MKDIR(dir) mkdir(dir, 0700)
    #define PATH_SEP "/"
#endif

Highscores::Highscores(bool higherIsBetter_) : higherIsBetter(higherIsBetter_) {
    #ifdef MSDOS
        const char* home = ".";
    #elif defined(_WIN32)
        const char* home = getenv("APPDATA");
    #else
        const char* home = getenv("HOME");
    #endif

    if (!home) {
        home = ".";
    }
    
    scorePath = std::string(home) + PATH_SEP + "scores.txt";
    loadScores();
}

bool Highscores::better(int a, int b) const {
    return higherIsBetter ? (a > b) : (a < b);
}

void Highscores::addScore(const Score& score) {
    scores.push_back(score);
    scoresByDifficulty[score.difficulty].push_back(score);
    
    // Sort scores for this difficulty
    auto& difficultyScores = scoresByDifficulty[score.difficulty];
    std::sort(difficultyScores.begin(), difficultyScores.end(),
        [this](const Score& a, const Score& b) {
            return better(a.time, b.time);
        });
    
    // Keep only top MAX_SCORES_PER_DIFFICULTY scores for this difficulty
    if (difficultyScores.size() > MAX_SCORES_PER_DIFFICULTY) {
        difficultyScores.resize(MAX_SCORES_PER_DIFFICULTY);
    }
    
    // Update main scores vector to reflect all difficulty-specific scores
    scores.clear();
    for (const auto& pair : scoresByDifficulty) {
        scores.insert(scores.end(), pair.second.begin(), pair.second.end());
    }
    
    saveScores();
}

const std::vector<Score>& Highscores::getScores() const {
    return scores;
}

std::vector<Score> Highscores::getScoresByDifficulty(const std::string& difficulty) const {
    auto it = scoresByDifficulty.find(difficulty);
    if (it != scoresByDifficulty.end()) {
        return it->second;
    }
    return std::vector<Score>();
}

bool Highscores::isHighScore(int time, const std::string& difficulty) const {
    auto it = scoresByDifficulty.find(difficulty);
    if (it == scoresByDifficulty.end()) {
        return true;  // First score for this difficulty
    }
    
    const auto& difficultyScores = it->second;
    if (difficultyScores.size() < MAX_SCORES_PER_DIFFICULTY) {
        return true;  // Less than max scores for this difficulty
    }
    
    return better(time, difficultyScores.back().time);  // Compare with worst score in top 10
}

void Highscores::loadScores() {
    std::ifstream file(scorePath);
    if (!file) return;
    
    scores.clear();
    scoresByDifficulty.clear();
    
    std::string line;
    while (std::getline(file, line)) {
        size_t pos1 = line.find('|');
        size_t pos2 = line.find('|', pos1 + 1);
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            Score score;
            score.name = line.substr(0, pos1);
            score.time = std::stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
            score.difficulty = line.substr(pos2 + 1);
            scores.push_back(score);
            scoresByDifficulty[score.difficulty].push_back(score);
        }
    }
    
    // Sort scores for each difficulty
    for (auto& pair : scoresByDifficulty) {
        auto& difficultyScores = pair.second;
        std::sort(difficultyScores.begin(), difficultyScores.end(),
            [this](const Score& a, const Score& b) {
                return better(a.time, b.time);
            });
        if (difficultyScores.size() > MAX_SCORES_PER_DIFFICULTY) {
            difficultyScores.resize(MAX_SCORES_PER_DIFFICULTY);
        }
    }
    
    // Rebuild main scores vector
    scores.clear();
    for (const auto& pair : scoresByDifficulty) {
        scores.insert(scores.end(), pair.second.begin(), pair.second.end());
    }
}

void Highscores::saveScores() {
    std::ofstream file(scorePath);
    if (!file) return;
    
    for (const auto& score : scores) {
        file << score.name << '|' << score.time << '|' << score.difficulty << '\n';
    }
}
