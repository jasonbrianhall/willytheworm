// level_generator.cpp
//
// Standalone random level generator for "Willy the Worm".
// No GTK/SDL/Cairo dependencies -- just the STL.
//
// Build:
//   g++ -std=c++17 -O2 level_generator.cpp -o level_generator
//
// Usage:
//   ./level_generator [num_levels] [output_file] [seed]
//
//   num_levels   how many levels to generate (default 10)
//   output_file  where to write them (default levels_generated.json)
//   seed         RNG seed for reproducible levels (default: random)
//
// The output is a JSON file in the same shape your game's
// LevelLoader::save_levels() writes, so it loads straight into
// WillyGame/WillyEditor via -L <file>.
//
// Every generated level is validated with a BFS that mirrors the
// game's actual movement rules (can_move_to / is_on_solid_ground /
// gravity) before it's accepted, so the bell is always reachable
// from Willy's start.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// ---- constants mirrored from willy.h ----
static const int GAME_SCREEN_WIDTH = 40;
static const int GAME_SCREEN_HEIGHT = 25; // valid rows for can_move_to: 0..24
static const int GAME_MAX_HEIGHT = 26;    // row 25 may hold the bottom floor

using Grid = std::vector<std::vector<std::string>>; // grid[row][col]

static const std::vector<std::string> PIPE_VARIANTS = {
    "PIPE1", "PIPE3", "PIPE6", "PIPE7", "PIPE8", "PIPE9", "PIPE10",
    "PIPE12", "PIPE15", "PIPE16", "PIPE18", "PIPE19", "PIPE20", "PIPE22",
    "PIPE23", "PIPE24", "PIPE25", "PIPE27", "PIPE29", "PIPE31", "PIPE34",
    "PIPE35", "PIPE38", "PIPE39", "PIPE40"};

static bool is_pipe(const std::string &t) {
  return t.rfind("PIPE", 0) == 0; // starts with "PIPE"
}

// Tiles Willy can occupy (mirrors WillyGame::can_move_to)
static bool is_walkable(const std::string &t) {
  return t == "EMPTY" || t == "LADDER" || t == "PRESENT" || t == "BELL" ||
         t == "UPSPRING" || t == "SIDESPRING" || t == "TACK" ||
         t == "BALLPIT" || t == "WILLY_RIGHT" || t == "WILLY_LEFT";
}

struct Level {
  Grid grid;
  std::pair<int, int> willy_pos;
  std::pair<int, int> bell_pos;
  std::pair<int, int> ballpit_pos; // {-1,-1} if none

  Level() : grid(26, std::vector<std::string>(40, "EMPTY")) {}
};

// ---------------------------------------------------------------------
// Physics-accurate reachability check (mirrors can_move_to /
// is_on_solid_ground / gravity from willy.cpp)
// ---------------------------------------------------------------------
static bool is_on_ground(const Grid &g, int row, int col) {
  if (row >= GAME_MAX_HEIGHT - 1)
    return true;
  if (g[row][col] == "LADDER")
    return true;
  if (row + 1 < (int)g.size() && is_pipe(g[row + 1][col]))
    return true;
  return false;
}

static bool can_occupy(const Grid &g, int row, int col) {
  if (row < 0 || row >= GAME_SCREEN_HEIGHT || col < 0 ||
      col >= GAME_SCREEN_WIDTH)
    return false;
  return is_walkable(g[row][col]);
}

static bool bell_reachable(const Level &lvl) {
  const Grid &g = lvl.grid;
  std::vector<std::vector<bool>> visited(GAME_SCREEN_HEIGHT,
                                         std::vector<bool>(GAME_SCREEN_WIDTH, false));
  std::vector<std::pair<int, int>> stack;
  stack.push_back(lvl.willy_pos);
  visited[lvl.willy_pos.first][lvl.willy_pos.second] = true;

  while (!stack.empty()) {
    auto [row, col] = stack.back();
    stack.pop_back();

    if (std::make_pair(row, col) == lvl.bell_pos)
      return true;

    bool grounded = is_on_ground(g, row, col);

    auto try_push = [&](int r, int c) {
      if (can_occupy(g, r, c) && !visited[r][c]) {
        visited[r][c] = true;
        stack.push_back({r, c});
      }
    };

    // Horizontal movement is always available regardless of grounding
    // (gravity is a separate falling effect layered on top of it -- see
    // update_willy_movement in willy.cpp).
    try_push(row, col - 1);
    try_push(row, col + 1);

    // Vertical movement mirrors the real (asymmetric) game rules exactly:
    //  - UP always requires the TARGET tile itself to be LADDER,
    //    regardless of whether Willy is currently on a ladder.
    //  - DOWN requires either Willy is currently on a ladder (target can
    //    be any walkable tile), or the target tile below is LADDER
    //    (stepping onto a ladder from a normal walkway).
    if (row - 1 >= 0 && g[row - 1][col] == "LADDER")
      try_push(row - 1, col);
    if (row + 1 < (int)g.size() &&
        (g[row][col] == "LADDER" || g[row + 1][col] == "LADDER"))
      try_push(row + 1, col);

    // Gravity: falls one row per tick when not grounded/on a ladder.
    if (!grounded)
      try_push(row + 1, col);
  }
  return false;
}

// ---------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------
static bool generate_level(std::mt19937 &rng, Level &lvl) {
  Grid &g = lvl.grid;
  g.assign(26, std::vector<std::string>(40, "EMPTY"));

  std::uniform_int_distribution<int> floor_count_dist(5, 7);
  std::uniform_int_distribution<int> spacing_dist(3, 4);
  std::uniform_int_distribution<int> col_dist(2, GAME_SCREEN_WIDTH - 3);
  std::uniform_int_distribution<int> pipe_dist(0, (int)PIPE_VARIANTS.size() - 1);
  std::uniform_real_distribution<double> chance(0.0, 1.0);

  // 1. Choose floor rows, bottom to top.
  // Row 25 must stay completely empty -- GAME_SCREEN_HEIGHT is 25, so
  // Willy can only ever occupy rows 0-24, and the status bar is
  // positioned assuming only those rows hold content. The original
  // level1 confirms this convention: its bottom floor is row 24, and
  // row 25 is "{}" (empty). Putting a floor at row 25 makes it render
  // on top of the status bar.
  std::vector<int> floor_rows;
  int row = 24; // bottom-most floor sits on row 24 (foundation)
  int floor_count = floor_count_dist(rng);
  for (int i = 0; i < floor_count && row >= 3; i++) {
    floor_rows.push_back(row);
    row -= spacing_dist(rng);
  }
  if (floor_rows.size() < 2)
    return false;

  // 2. Fill each floor with a solid run of the default pipe variant.
  std::string level_pipe = "PIPE27"; // default pipe type
  for (int fr : floor_rows) {
    std::string pipe_type = (chance(rng) < 0.7) ? level_pipe
                                                 : PIPE_VARIANTS[pipe_dist(rng)];
    for (int c = 0; c < GAME_SCREEN_WIDTH; c++) {
      g[fr][c] = pipe_type;
    }
  }

  // 3. Connect every pair of ADJACENT floors with its own single-floor
  //    ladder (never a multi-floor shaft). Willy crosses the whole level
  //    via a chain of these one-hop ladders rather than one tall shaft.
  //    Each ladder extends one row above the upper floor (so Willy can
  //    climb up and off it -- the game's climb-up move requires the
  //    target tile itself to be LADDER) and stops one row above the
  //    lower floor (landing on its walkway without punching that floor).
  std::vector<int> floor_ladder_col(floor_rows.size(), -1); // per-floor-index ladder column, unused beyond bookkeeping
  for (size_t i = 0; i + 1 < floor_rows.size(); i++) {
    int lower = floor_rows[i];     // bigger row number, physically lower
    int upper = floor_rows[i + 1]; // smaller row number, physically higher
    int lcol = col_dist(rng);
    for (int r = upper - 1; r <= lower - 1; r++) {
      g[r][lcol] = "LADDER";
    }
    floor_ladder_col[i] = lcol;
    floor_ladder_col[i + 1] = lcol;
  }

  // 4. A couple of extra redundant single-floor ladders at random
  //    adjacent pairs, purely for variety/shortcuts -- still only ever
  //    spanning one floor gap each.
  int extra_ladders = std::uniform_int_distribution<int>(1, 3)(rng);
  for (int b = 0; b < extra_ladders && floor_rows.size() >= 2; b++) {
    int idx = std::uniform_int_distribution<int>(0, (int)floor_rows.size() - 2)(rng);
    int lower = floor_rows[idx];
    int upper = floor_rows[idx + 1];
    int bcol = col_dist(rng);
    for (int r = upper - 1; r <= lower - 1; r++) {
      g[r][bcol] = "LADDER";
    }
  }

  int top_row = floor_rows.back();
  int bottom_row = floor_rows.front();

  // 5. Occasional single-column drop-through gaps in floors (not on any
  //    ladder column) purely as fall-through shortcuts/hazards. The
  //    bottom-most floor (row 24) is exempt -- it must stay solid pipe
  //    across its full width with no gaps at all.
  int bottom_floor_row = floor_rows.front();
  for (int fr : floor_rows) {
    if (fr == bottom_floor_row)
      continue;
    int gaps = std::uniform_int_distribution<int>(0, 2)(rng);
    for (int i = 0; i < gaps; i++) {
      int gc = col_dist(rng);
      if (g[fr][gc] == "LADDER")
        continue;
      g[fr][gc] = "EMPTY";
    }
  }

  // 6. Willy's start: bottom floor's walkway row, on solid ground, not on
  //    a ladder column.
  int willy_row = bottom_row - 1;
  int willy_col;
  do {
    willy_col = col_dist(rng);
  } while (g[bottom_row][willy_col] == "LADDER" || !is_pipe(g[bottom_row][willy_col]));
  bool face_right = chance(rng) < 0.5;
  g[willy_row][willy_col] = face_right ? "WILLY_RIGHT" : "WILLY_LEFT";
  lvl.willy_pos = {willy_row, willy_col};

  // 7. Bell: top floor's walkway row, on solid ground, not on a ladder
  //    column.
  int bell_row = top_row - 1;
  if (bell_row < 0)
    return false;
  int bell_col;
  do {
    bell_col = col_dist(rng);
  } while (g[top_row][bell_col] == "LADDER" || !is_pipe(g[top_row][bell_col]));
  g[bell_row][bell_col] = "BELL";
  lvl.bell_pos = {bell_row, bell_col};

  // 7b. Primary ball pit: top floor's walkway, where balls emerge.
  //     find_ballpit_position() (willy.cpp) scans row 0 downward and
  //     picks the FIRST "BALLPIT" tile as the spawn point, so placing
  //     this one on the topmost floor guarantees it's picked as primary.
  {
    int attempts = 0;
    int c;
    do {
      c = col_dist(rng);
      attempts++;
    } while (attempts < 50 &&
             (g[top_row][c] == "LADDER" || !is_pipe(g[top_row][c]) ||
              g[bell_row][c] != "EMPTY"));
    if (attempts < 50) {
      g[bell_row][c] = "BALLPIT";
      lvl.ballpit_pos = {bell_row, c}; // recorded as the PRIMARY pit
    }
  }

  // 7c. Secondary ball pit: bottom floor's walkway. Any ball that rolls
  //     into it gets auto-teleported back to the primary (top) pit by
  //     update_balls() in willy.cpp, giving balls a place to "respawn"
  //     and cascade again instead of settling permanently at the bottom.
  {
    int attempts = 0;
    int c;
    do {
      c = col_dist(rng);
      attempts++;
    } while (attempts < 50 &&
             (g[bottom_row][c] == "LADDER" || !is_pipe(g[bottom_row][c]) ||
              g[willy_row][c] != "EMPTY"));
    if (attempts < 50) {
      g[willy_row][c] = "BALLPIT";
    }
  }

  // 8. Scatter presents, tacks, springs on walkway rows (on solid ground,
  //    away from Willy/Bell/ladder cells).
  for (int fr : floor_rows) {
    int wr = fr - 1;
    if (wr < 0 || wr == willy_row || wr == bell_row)
      continue;

    int presents = std::uniform_int_distribution<int>(1, 3)(rng);
    int tacks = std::uniform_int_distribution<int>(0, 2)(rng);
    int springs = (chance(rng) < 0.3) ? 1 : 0;

    auto place = [&](const std::string &tile) {
      for (int attempt = 0; attempt < 10; attempt++) {
        int c = col_dist(rng);
        if (g[wr][c] != "EMPTY")
          continue;
        if (!is_pipe(g[fr][c]))
          continue; // needs solid ground beneath it
        g[wr][c] = tile;
        return;
      }
    };

    for (int i = 0; i < presents; i++) place("PRESENT");
    for (int i = 0; i < tacks; i++) place("TACK");
    if (springs) place(chance(rng) < 0.5 ? "UPSPRING" : "SIDESPRING");
  }

  return true;
}

static Level make_valid_level(std::mt19937 &rng, bool &solved,
                              int max_attempts = 200) {
  for (int attempt = 0; attempt < max_attempts; attempt++) {
    Level lvl;
    if (!generate_level(rng, lvl))
      continue;
    if (bell_reachable(lvl)) {
      solved = true;
      return lvl;
    }
  }
  std::cerr << "WARNING: failed to generate a verified-solvable level after "
            << max_attempts << " attempts; using last attempt anyway.\n";
  Level lvl;
  generate_level(rng, lvl);
  solved = false;
  return lvl;
}

// ---------------------------------------------------------------------
// JSON output (same shape as LevelLoader::save_levels)
// ---------------------------------------------------------------------
static void write_levels_json(const std::vector<Level> &levels,
                              const std::string &path) {
  std::ofstream file(path);
  if (!file) {
    std::cerr << "ERROR: cannot open " << path << " for writing\n";
    return;
  }

  file << "{\n";

  bool first_level = true;
  for (size_t i = 0; i < levels.size(); i++) {
    const Level &lvl = levels[i];
    std::string level_name = "level" + std::to_string(i + 1);

    if (!first_level)
      file << ",\n";
    first_level = false;

    file << "  \"" << level_name << "\": {\n";

    bool first_row = true;
    for (int r = 0; r < (int)lvl.grid.size(); r++) {
      // Collect non-EMPTY columns for this row
      std::vector<std::pair<int, std::string>> cells;
      for (int c = 0; c < GAME_SCREEN_WIDTH; c++) {
        if (lvl.grid[r][c] != "EMPTY") {
          cells.push_back({c, lvl.grid[r][c]});
        }
      }
      if (cells.empty())
        continue;

      if (!first_row)
        file << ",\n";
      first_row = false;

      file << "    \"" << r << "\": {\n";
      bool first_col = true;
      for (auto &[c, tile] : cells) {
        if (!first_col)
          file << ",\n";
        first_col = false;
        file << "      \"" << c << "\": \"" << tile << "\"";
      }
      file << "\n    }";
    }

    file << "\n  }";

    // Ball pit entry (matches ball_pit_data / levelNPIT convention)
    if (lvl.ballpit_pos.first >= 0) {
      file << ",\n  \"" << level_name << "PIT\": {\n";
      file << "    \"PRIMARYBALLPIT\": [" << lvl.ballpit_pos.first << ", "
           << lvl.ballpit_pos.second << "]\n";
      file << "  }";
    }
  }

  file << "\n}\n";
}

int main(int argc, char *argv[]) {
  int num_levels = 10;
  std::string output_file = "levels_generated.json";
  unsigned int seed = std::random_device{}();

  if (argc > 1)
    num_levels = std::max(1, std::atoi(argv[1]));
  if (argc > 2)
    output_file = argv[2];
  if (argc > 3)
    seed = (unsigned int)std::stoul(argv[3]);

  std::cout << "Generating " << num_levels << " level(s) with seed " << seed
            << "...\n";

  std::mt19937 rng(seed);
  std::vector<Level> levels;
  for (int i = 0; i < num_levels; i++) {
    bool solved = false;
    Level lvl = make_valid_level(rng, solved);
    levels.push_back(lvl);
    std::cout << "  level" << (i + 1) << ": willy=(" << lvl.willy_pos.first
              << "," << lvl.willy_pos.second << ") bell=(" << lvl.bell_pos.first
              << "," << lvl.bell_pos.second << ") -- "
              << (solved ? "solvable" : "UNVERIFIED") << "\n";
  }

  write_levels_json(levels, output_file);
  std::cout << "Wrote " << output_file << "\n";
  std::cout << "Run the game with: ./willy -L " << output_file << "\n";

  return 0;
}
