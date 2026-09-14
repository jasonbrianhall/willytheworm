#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>

// ============================================================================
// WOPR OVERLAY  (F7)
//
// A War Games-inspired retro terminal overlay with:
//   - 300-baud simulated login sequence (FALKEN / JOSHUA)
//   - WOPR game menu
//   - Playable: Tic-Tac-Toe, Chess, Minesweeper, Falken's Maze,
//               Global Thermonuclear War, Zork, Willy the Worm,
//               Strategic Defense Initiative
// ============================================================================

enum class WoprPhase {
    INACTIVE,
    MODEM_SCREECH,    // FSK audio plays, progress bar fills
    LOGIN_PROMPT,     // Typing USERNAME: / PASSWORD: at 300 baud
    LOGIN_INPUT,      // Waiting for user keystrokes
    LOGIN_REJECTED,   // Wrong creds → "IDENTIFICATION NOT RECOGNIZED"
    CONNECTING,       // "CONNECTING TO WOPR..." crawl
    GAME_MENU,        // Main WOPR game list / command shell
    PLAYING_TTT,
    PLAYING_CHESS,
    PLAYING_CHECKERS,
    PLAYING_MINES,
    PLAYING_MAZE,
    PLAYING_WAR,
    PLAYING_ZORK,
    PLAYING_BASIC,
    PLAYING_WILLY,    // Willy the Worm
    PLAYING_SDI,      // Strategic Defense Initiative
    FAREWELL,         // "A STRANGE GAME..." on exit
};

enum class WoprGame {
    NONE = 0,
    CHESS,
    CHECKERS,
    FALKEN_MAZE,
    GLOBAL_WAR,
    TIC_TAC_TOE,
    MINESWEEPER,
    ZORK,
    BASIC,
    WIZARD,
    WILLY_WORM,
    SDI,
    // Sentinel
    COUNT
};

// ─── Shared overlay state ──────────────────────────────────────────────────

struct WoprState {
    WoprPhase phase       = WoprPhase::INACTIVE;
    bool      visible     = false;

    // Baud-rate text crawler
    std::string  crawl_target;   // full text to reveal
    int          crawl_pos  = 0; // chars revealed so far
    double       crawl_acc  = 0; // accumulated fractional chars
    double       crawl_baud = 300.0;

    // Login state
    std::string  input_buf;      // what user has typed this field
    bool         username_done = false;
    std::string  username_entered;
    std::string  password_entered;
    int          reject_count  = 0;

    // Full terminal buffer (lines already committed)
    std::vector<std::string> lines;

    // Game menu
    int          menu_sel  = 0;

    // Modem screech
    double       screech_t = 0.0;   // elapsed seconds
    double       screech_dur = 3.5; // total screech duration

    // Phase timer (used for delays/transitions)
    double       phase_timer = 0.0;

    // Scroll offset for the terminal log (lines from bottom; 0 = pinned to bottom)
    int          scroll_offset = 0;

    // Auto-login: types FALKEN / JOSHUA automatically with keystroke sounds
    bool         auto_login       = true;   // enabled by default
    double       auto_type_acc    = 0.0;    // accumulated time towards next keystroke
    double       auto_type_delay  = 0.0;    // randomised interval to next char (seconds)
    double       auto_submit_wait = 0.0;    // countdown after last char before Enter

    // Sub-game state — opaque blobs owned by sub-modules
    void        *sub_state = nullptr;
};

extern WoprState g_wopr;

// Current terminal foreground color — set via COLOR command
extern uint8_t g_term_r, g_term_g, g_term_b;

// ─── Lifecycle ────────────────────────────────────────────────────────────

void wopr_open();
void wopr_close();

// ─── Per-frame update (call from main loop, dt in seconds) ────────────────
void wopr_update(double dt);

// ─── Render (call after terminal render, before menu) ─────────────────────
void wopr_render(int win_w, int win_h);

// ─── Input — returns true if event consumed ───────────────────────────────
bool wopr_keydown(SDL_Keycode sym, const char *text);

// Mouse — call from SDL_MOUSEBUTTONDOWN / SDL_MOUSEMOTION / SDL_MOUSEBUTTONUP / SDL_MOUSEWHEEL
bool wopr_mousedown(int x, int y, int button);
bool wopr_mousemove(int x, int y);
bool wopr_mouseup(int x, int y, int button);
bool wopr_mousewheel(int delta);   // delta>0 = scroll up (towards older lines)

// ─── Audio ────────────────────────────────────────────────────────────────
bool  wopr_audio_init();
void  wopr_audio_shutdown();
void  wopr_audio_play_screech();
void  wopr_audio_stop();
void  wopr_audio_play_keystroke();   // short click for auto-login typing

// ─── Sub-game interfaces (implemented in wopr_*.cpp) ──────────────────────

// Tic-Tac-Toe
void wopr_ttt_enter(WoprState *w);
void wopr_ttt_update(WoprState *w, double dt);
void wopr_ttt_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_ttt_keydown(WoprState *w, SDL_Keycode sym);
void wopr_ttt_free(WoprState *w);
void wopr_ttt_mousedown(WoprState *w, int x, int y, int button);
void wopr_ttt_mousemove(WoprState *w, int x, int y);

// Chess
void wopr_chess_enter(WoprState *w);
void wopr_chess_update(WoprState *w, double dt);
void wopr_chess_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_chess_keydown(WoprState *w, SDL_Keycode sym);
void wopr_chess_free(WoprState *w);
void wopr_chess_mousedown(WoprState *w, int x, int y, int button);
void wopr_chess_mousemove(WoprState *w, int x, int y);

// Checkers
void wopr_checkers_enter(WoprState *w);
void wopr_checkers_update(WoprState *w, double dt);
void wopr_checkers_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_checkers_keydown(WoprState *w, SDL_Keycode sym);
void wopr_checkers_free(WoprState *w);
void wopr_checkers_mousedown(WoprState *w, int x, int y, int button);
void wopr_checkers_mousemove(WoprState *w, int x, int y);

// Minesweeper
void wopr_mines_enter(WoprState *w);
void wopr_mines_update(WoprState *w, double dt);
void wopr_mines_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_mines_keydown(WoprState *w, SDL_Keycode sym);
void wopr_mines_free(WoprState *w);
void wopr_mines_mousedown(WoprState *w, int x, int y, int button);
void wopr_mines_mousemove(WoprState *w, int x, int y);

// Falken's Maze
void wopr_maze_enter(WoprState *w);
void wopr_maze_update(WoprState *w, double dt);
void wopr_maze_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_maze_keydown(WoprState *w, SDL_Keycode sym);
void wopr_maze_free(WoprState *w);

// Global Thermonuclear War
void wopr_war_enter(WoprState *w);
void wopr_war_update(WoprState *w, double dt);
void wopr_war_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_war_keydown(WoprState *w, SDL_Keycode sym);
void wopr_war_free(WoprState *w);
void wopr_war_mousedown(WoprState *w, int x, int y, int button);
void wopr_war_mousemove(WoprState *w, int x, int y);

// Zork / Dungeon
void wopr_zork_enter(WoprState *w);
void wopr_zork_update(WoprState *w, double dt);
void wopr_zork_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_zork_keydown(WoprState *w, SDL_Keycode sym);
void wopr_zork_text(WoprState *w, const char *text);
void wopr_zork_free(WoprState *w);

// BASIC
void wopr_basic_enter(WoprState *w);
void wopr_basic_update(WoprState *w, double dt);
void wopr_basic_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_basic_keydown(WoprState *w, SDL_Keycode sym);
bool wopr_basic_mousewheel(WoprState *w, int delta);
bool wopr_basic_is_waiting_input(WoprState *w);
const char *wopr_basic_get_prompt(uint8_t *r, uint8_t *g, uint8_t *b);
int         wopr_basic_get_prompt_row(void);
int         wopr_basic_get_screen_top(void);
void wopr_basic_text(WoprState *w, const char *text);
void wopr_basic_free(WoprState *w);

// wopr_basic's line_mtx guards WoprState::lines against concurrent writes
// from the BASIC interpreter thread (commit_line() etc). wopr_render() reads
// w->lines from the main thread and must hold this lock while doing so, or
// a push_back()/erase() on the interpreter thread mid-read can invalidate
// the vector out from under the render loop (observed as a bounds-checked
// STL "vector subscript out of range" under a tight PRINT/GOTO loop).
// No-ops safely if no BASIC session is active.
void wopr_basic_lines_lock(void);
void wopr_basic_lines_unlock(void);

// Wizard's Castle (BASIC program, shares the BASIC sub-game machinery)
void wopr_wizard_enter(WoprState *w);

// Willy the Worm
void wopr_willy_enter(WoprState *w);
void wopr_willy_update(WoprState *w, double dt);
void wopr_willy_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_willy_keydown(WoprState *w, SDL_Keycode sym);
void wopr_willy_keyup(WoprState *w, SDL_Keycode sym);
void wopr_willy_free(WoprState *w);
void wopr_willy_mousedown(WoprState *w, int x, int y, int button);
void wopr_willy_mousemove(WoprState *w, int x, int y);
void wopr_willy_mouseup(WoprState *w, int x, int y, int button);
void wopr_willy_textinput(WoprState *w, const char *text);

// Strategic Defense Initiative (Missile Command)
void wopr_sdi_enter(WoprState *w);
void wopr_sdi_update(WoprState *w, double dt);
void wopr_sdi_render(WoprState *w, int x, int y, int cw, int ch, int cols);
bool wopr_sdi_keydown(WoprState *w, SDL_Keycode sym);
void wopr_sdi_free(WoprState *w);
void wopr_sdi_mousedown(WoprState *w, int x, int y, int button);
void wopr_sdi_mousemove(WoprState *w, int x, int y);
