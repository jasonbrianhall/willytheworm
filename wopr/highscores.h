#pragma once
#include <string>
#include <vector>
#include <map>

struct Score {
    std::string name;
    int time;           // ranking metric: a duration (lower wins) or a point score
                         // (higher wins) — see Highscores::higherIsBetter
    std::string difficulty;
};

class Highscores {
public:
    // higherIsBetter=false (default): ranks ascending, e.g. a timed puzzle where the
    // fastest time wins. Pass true for point-scoring games where the highest score wins.
    explicit Highscores(bool higherIsBetter = false);
    void addScore(const Score& score);
    const std::vector<Score>& getScores() const;
    std::vector<Score> getScoresByDifficulty(const std::string& difficulty) const;
    bool isHighScore(int time, const std::string& difficulty) const;

private:
    void loadScores();
    void saveScores();
    bool better(int a, int b) const;  // true if a outranks b

    std::string scorePath;
    std::vector<Score> scores;  // All scores
    std::map<std::string, std::vector<Score>> scoresByDifficulty;  // Scores grouped by difficulty
    const size_t MAX_SCORES_PER_DIFFICULTY = 10;
    bool higherIsBetter;
};

