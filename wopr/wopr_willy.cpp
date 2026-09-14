// =============================================================================
// wopr_willy.cpp  —  Willy the Worm inside the WOPR overlay
//
// Ported directly from the C++ willy.cpp / willy.h.
// Renders via gl_draw_rect() using willy.chr bitmap sprites.
// Both assets are compiled in via wopr_willy_assets.h (compressed with zlib).
//
// To regenerate the header:
//   python3 gen_willy_assets.py levels.json willy.chr wopr_willy_assets.h
// =============================================================================

#include "wopr.h"
#include "highscores.h"
#include "wopr_render.h"
#include <SDL2/SDL.h>
#include "miniz.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "wopr_willy_assets.h"
#include "wopr_willy_assets_original.h"
#define WILLY_ASSETS_EMBEDDED 1

// =============================================================================
// CHR SPRITES
// 128 sprites × 8 bytes. Bit7 of byte0 = top-left pixel.
// Index mapping (from spriteloader.cpp):
//   0=WILLY_RIGHT  1=WILLY_LEFT  2=PRESENT   3=LADDER
//   4=TACK         5=UPSPRING    6=SIDESPRING 7=BALL  8=BELL
//   51-90=PIPE1-PIPE40  126=BALLPIT  127=EMPTY
// =============================================================================

static uint8_t s_chr[128][8];
static bool    s_chr_loaded = false;

static bool ww_chr_load_mem(const void *data, size_t len) {
    if (len < 128*8) return false;
    const uint8_t *p = static_cast<const uint8_t*>(data);
    for (int i = 0; i < 128; i++)
        for (int r = 0; r < 8; r++)
            s_chr[i][r] = p[i*8+r];
    s_chr_loaded = true;
    return true;
}

static bool ww_chr_load_disk() {
    const char *paths[] = {"willy.chr","data/willy.chr",
                           "/usr/games/willytheworm/data/willy.chr",nullptr};
    for (int i = 0; paths[i]; i++) {
        std::ifstream f(paths[i], std::ios::binary);
        if (!f.good()) continue;
        f.seekg(0,std::ios::end); size_t sz=f.tellg(); f.seekg(0);
        if (sz < 128*8) continue;
        std::vector<uint8_t> buf(sz);
        f.read(reinterpret_cast<char*>(buf.data()), sz);
        return ww_chr_load_mem(buf.data(), sz);
    }
    return false;
}

static bool ww_chr_load() {
    if (WILLY_ASSETS_EMBEDDED && willy_chr_z_len > 1) {
        std::vector<uint8_t> raw(willy_chr_raw_len);
        mz_ulong out = (mz_ulong)willy_chr_raw_len;
        if (mz_uncompress(raw.data(), &out,
                          willy_chr_z, (mz_ulong)willy_chr_z_len) == MZ_OK)
            if (ww_chr_load_mem(raw.data(), out)) return true;
    }
    return ww_chr_load_disk();
}

static int ww_sprite_idx(const std::string &tile) {
    if (tile=="WILLY_RIGHT") return 0;
    if (tile=="WILLY_LEFT")  return 1;
    if (tile=="PRESENT")     return 2;
    if (tile=="LADDER")      return 3;
    if (tile=="TACK")        return 4;
    if (tile=="UPSPRING")    return 5;
    if (tile=="SIDESPRING")  return 6;
    if (tile=="BALL")        return 7;
    if (tile=="BELL")        return 8;
    if (tile=="BALLPIT")     return 126;
    if (tile=="EMPTY")       return -1;
    if (tile.size()>4 && tile.substr(0,4)=="PIPE") {
        try { int n=std::stoi(tile.substr(4)); if(n>=1&&n<=40) return 50+n; }
        catch(...) {}
    }
    return -1;
}

// Draw one 8×8 sprite scaled to cell_w×cell_h. All sprites white (palette(1)
// textcolor(3) in the original = white on blue background).
static void ww_draw_sprite(int idx, float px, float py,
                           float cw, float ch) {
    if (!s_chr_loaded || idx<0 || idx>=128) return;
    float pw = cw/8.f, ph = ch/8.f;
    for (int row=0; row<8; row++) {
        uint8_t b = s_chr[idx][row];
        if (!b) continue;
        for (int col=0; col<8; col++)
            if ((b>>(7-col))&1)
                gl_draw_rect(px+col*pw, py+row*ph, pw, ph, 1.f,1.f,1.f,1.f);
    }
}

// =============================================================================
// SOUND  (programmatic SDL square-wave, no audio files needed)
// =============================================================================

static SDL_AudioDeviceID s_aud = 0;
static int  s_aud_rate = 22050;

static const int  ABUF = 16384;
static float      s_abuf[ABUF];
static int        s_aread = 0, s_awrite = 0;

static void ww_audio_cb(void*, uint8_t *stream, int len) {
    int16_t *out = reinterpret_cast<int16_t*>(stream);
    int n = len/2;
    for (int i=0; i<n; i++) {
        if (s_aread != s_awrite) {
            out[i] = (int16_t)(s_abuf[s_aread]*16000.f);
            s_aread = (s_aread+1)%ABUF;
        } else { out[i]=0; }
    }
}

static void ww_audio_init() {
    if (s_aud) return;
    SDL_AudioSpec want{}, got{};
    want.freq=s_aud_rate; want.format=AUDIO_S16SYS;
    want.channels=1; want.samples=512; want.callback=ww_audio_cb;
    s_aud = SDL_OpenAudioDevice(nullptr,0,&want,&got,0);
    if (s_aud) { s_aud_rate=got.freq; SDL_PauseAudioDevice(s_aud,0); }
}

static void ww_audio_shutdown() {
    if (s_aud) { SDL_CloseAudioDevice(s_aud); s_aud=0; }
}

// Muted from the "Do you want sound effects?" startup prompt.
static bool s_sound_on = true;

// Queue a square-wave tone: freq Hz for dur_ms milliseconds.
static void ww_beep(float freq, float dur_ms) {
    if (!s_aud || !s_sound_on) return;
    int n = (int)(s_aud_rate * dur_ms / 1000.f);
    SDL_LockAudioDevice(s_aud);
    float ph=0.f, inc=(freq>0.f)?(2.f*(float)M_PI*freq/s_aud_rate):0.f;
    for (int i=0; i<n; i++) {
        float v = (freq>0.f)?(ph<(float)M_PI?0.35f:-0.35f):0.f;
        int nx=(s_awrite+1)%ABUF;
        if (nx!=s_aread) { s_abuf[s_awrite]=v; s_awrite=nx; }
        ph+=inc; if(ph>2.f*(float)M_PI) ph-=2.f*(float)M_PI;
    }
    SDL_UnlockAudioDevice(s_aud);
}

// Sound effects matching Pascal source exactly:
// winsound:   for m=1..5: sound(2000) delay(45) nosound delay(30)
// losesound:  for m=1..5: sound(220) nosound delay(m)
//             for m=12..1: sound(2000) nosound delay(m div 2)
// addpoints:  sound(1200) delay(10) sound(1660) delay(10) nosound
// extra life: for c=500..1500: sound(c)  nosound
// climb:      sound((25-wy)*100) [very short, per step]

static void ww_snd_win()   { for(int i=0;i<5;i++){ww_beep(2000,45);ww_beep(0,30);} }
static void ww_snd_lose()  {
    for(int m=1;m<=5;m++) { ww_beep(220,2); ww_beep(0,(float)m); }
    for(int m=12;m>=1;m--){ ww_beep(2000,2); ww_beep(0,(float)(m/2)); }
}
static void ww_snd_pts()   { ww_beep(1200,10); ww_beep(1660,10); }
static void ww_snd_life()  { for(int c=500;c<=1500;c+=10) ww_beep((float)c,1.f); }
static void ww_snd_climb(int wy) { ww_beep((float)((25-wy)*100), 16.f); }
// Splat: rapid descending noise burst
static void ww_snd_splat() {
    for(int f=800; f>=80; f-=40) { ww_beep((float)f, 18.f); }
    ww_beep(0.f, 60.f);
}

// =============================================================================
// LEVEL LOADER
// JSON format: { "levelN": { "ROW": { "COL": "TILE" } },
//               "levelNPIT": { "PRIMARYBALLPIT": [row,col] } }
// Row/col keys are plain integer strings.
// =============================================================================

static const int W_COLS    = 40;
static const int W_ROWS    = 25;   // GAME_SCREEN_HEIGHT
static const int W_MAXROWS = 26;   // GAME_MAX_HEIGHT
static const int W_MAXCOLS = 40;
static const int W_NEWLIFE = 2000; // GAME_NEWLIFEPOINTS

struct WLevel {
    std::string grid[W_MAXROWS][W_MAXCOLS];
    std::string orig[W_MAXROWS][W_MAXCOLS];
    std::pair<int,int> willy_start = {23,0};
    std::pair<int,int> ballpit     = {0,0};
    bool has_ballpit = false;
};

struct WLevels {
    std::map<std::string,WLevel> levels;

    static std::string trim(const std::string &s) {
        size_t a=s.find_first_not_of(" \t\r\n");
        if(a==std::string::npos) return "";
        size_t b=s.find_last_not_of(" \t\r\n");
        return s.substr(a,b-a+1);
    }
    static bool is_uint(const std::string &s) {
        if(s.empty()) return false;
        for(char c:s) if(!isdigit((unsigned char)c)) return false;
        return true;
    }
    static std::string str_val(const std::string &t, size_t after) {
        size_t a=t.find('"',after); if(a==std::string::npos) return "";
        size_t b=t.find('"',a+1);  if(b==std::string::npos) return "";
        return t.substr(a+1,b-a-1);
    }

    bool parse(const std::string &json) {
        std::istringstream ss(json);
        std::string line, cur_lv; bool in_pit=false; int cur_row=-1;
        while(std::getline(ss,line)) {
            std::string t=trim(line);
            if(t.empty()||t[0]!='"') continue;
            size_t col=t.find("\":"); if(col==std::string::npos) continue;
            std::string key=t.substr(1,col-1);
            int ind=0; for(char c:line){if(c==' ')ind++;else break;}

            if(ind==4) { // top-level key
                cur_row=-1;
                if(key.find("level")==0) {
                    size_t p=key.find("PIT");
                    if(p!=std::string::npos){cur_lv=key.substr(0,p);in_pit=true;}
                    else {
                        cur_lv=key; in_pit=false;
                        if(!levels.count(cur_lv)) {
                            WLevel lv;
                            for(int r=0;r<W_MAXROWS;r++)
                                for(int c=0;c<W_MAXCOLS;c++)
                                    lv.grid[r][c]="EMPTY";
                            levels[cur_lv]=lv;
                        }
                    }
                }
            } else if(ind==8 && !cur_lv.empty()) {
                if(in_pit) {
                    if(key=="PRIMARYBALLPIT") {
                        // Array may be multi-line: read ahead to collect [r, c]
                        // Try same line first
                        std::vector<int> nums;
                        auto collect = [&](const std::string &src) {
                            std::string tmp = src;
                            size_t i=0;
                            while(i<tmp.size()) {
                                while(i<tmp.size() && !isdigit((unsigned char)tmp[i])) i++;
                                if(i>=tmp.size()) break;
                                size_t j=i;
                                while(j<tmp.size() && isdigit((unsigned char)tmp[j])) j++;
                                nums.push_back(std::stoi(tmp.substr(i,j-i)));
                                i=j;
                            }
                        };
                        // Gather lines until we have 2 numbers or hit ]
                        std::string gathered = t;
                        collect(gathered);
                        while(nums.size()<2) {
                            std::string nextline;
                            if(!std::getline(ss,nextline)) break;
                            std::string nt=trim(nextline);
                            if(nt.find(']')!=std::string::npos && nums.size()>=2) break;
                            collect(nt);
                            if(nt.find(']')!=std::string::npos) break;
                        }
                        if(nums.size()>=2) {
                            int r=nums[0], c=nums[1];
                            // Ensure the level entry exists
                            if(!levels.count(cur_lv)) {
                                WLevel lv;
                                for(int rr=0;rr<W_MAXROWS;rr++)
                                    for(int cc=0;cc<W_MAXCOLS;cc++)
                                        lv.grid[rr][cc]="EMPTY";
                                levels[cur_lv]=lv;
                            }
                            levels[cur_lv].ballpit={r,c};
                            levels[cur_lv].has_ballpit=true;
                        }
                    }
                } else { cur_row=is_uint(key)?std::stoi(key):-1; }
            } else if(ind==12 && !in_pit && cur_row>=0 && cur_row<W_MAXROWS && is_uint(key)) {
                int c=std::stoi(key); if(c<0||c>=W_MAXCOLS) continue;
                std::string tile=str_val(t,col+1); if(tile.empty()) continue;
                auto &lv=levels[cur_lv];
                if(tile=="WILLY_RIGHT"||tile=="WILLY_LEFT") {
                    lv.willy_start={cur_row,c};
                    lv.grid[cur_row][c]="EMPTY";
                } else { lv.grid[cur_row][c]=tile; }
            }
        }
        for(auto &[name,lv]:levels)
            for(int r=0;r<W_MAXROWS;r++) for(int c=0;c<W_MAXCOLS;c++) {
                if(lv.grid[r][c].empty()) lv.grid[r][c]="EMPTY";
                lv.orig[r][c]=lv.grid[r][c];
            }
        return !levels.empty();
    }

    bool load_str(const std::string &s) { return parse(s); }
    bool load_disk(const std::string &fn) {
        for(auto &p:{fn,"data/"+fn,"/usr/games/willytheworm/data/"+fn}) {
            std::ifstream f(p); if(!f.good()) continue;
            std::stringstream b; b<<f.rdbuf(); return parse(b.str());
        }
        return false;
    }
    bool exists(const std::string &n) const { return levels.count(n)>0; }

    const std::string &get(const std::string &lv,int r,int c) const {
        static const std::string E="EMPTY";
        auto it=levels.find(lv); if(it==levels.end()) return E;
        if(r<0||r>=W_MAXROWS||c<0||c>=W_MAXCOLS) return E;
        return it->second.grid[r][c];
    }
    void set(const std::string &lv,int r,int c,const std::string &t) {
        auto it=levels.find(lv);
        if(it==levels.end()||r<0||r>=W_MAXROWS||c<0||c>=W_MAXCOLS) return;
        it->second.grid[r][c]=t;
    }
    void reset(const std::string &lv) {
        auto it=levels.find(lv); if(it==levels.end()) return;
        for(int r=0;r<W_MAXROWS;r++) for(int c=0;c<W_MAXCOLS;c++)
            it->second.grid[r][c]=it->second.orig[r][c];
    }
    std::pair<int,int> willy_start(const std::string &lv) const {
        auto it=levels.find(lv);
        return it!=levels.end()?it->second.willy_start:std::make_pair(23,0);
    }
    std::pair<int,int> ballpit(const std::string &lv) const {
        auto it=levels.find(lv);
        if(it==levels.end()||!it->second.has_ballpit) return {0,0};
        return it->second.ballpit;
    }
};

static bool ww_load_levels(WLevels &lv, bool use_original) {
    if(use_original) {
        if(WILLY_ASSETS_EMBEDDED && willy_levels_original_z_len>1) {
            std::vector<uint8_t> raw(willy_levels_original_raw_len);
            mz_ulong out=(mz_ulong)willy_levels_original_raw_len;
            if(mz_uncompress(raw.data(),&out,
                             willy_levels_original_z,(mz_ulong)willy_levels_original_z_len)==MZ_OK) {
                if(lv.load_str(std::string(raw.begin(),raw.begin()+out))) return true;
            }
        }
        return lv.load_disk("original_levels.json");
    }
    if(WILLY_ASSETS_EMBEDDED && willy_levels_z_len>1) {
        std::vector<uint8_t> raw(willy_levels_raw_len);
        mz_ulong out=(mz_ulong)willy_levels_raw_len;
        if(mz_uncompress(raw.data(),&out,
                         willy_levels_z,(mz_ulong)willy_levels_z_len)==MZ_OK) {
            if(lv.load_str(std::string(raw.begin(),raw.begin()+out))) return true;
        }
    }
    return lv.load_disk("levels.json");
}

// =============================================================================
// BALL  (mirrors C++ Ball struct and update_balls logic)
// =============================================================================
struct WBall { int row,col; std::string dir; };

// =============================================================================
// STATE
// =============================================================================
enum class WSub { PROMPT_LEVELS, PROMPT_COLOR, PROMPT_SOUND, INTRO, PLAYING, DEAD_WHITE, WIN_PAUSE, GAME_OVER, NAME_ENTRY, HIGHSCORES };

// ─── Background color palette ───────────────────────────────────────────────
// Cycled at runtime with '[' / ']'. Index 0 is the original blue; a couple of
// single-hue "monochrome monitor" presets are included for the color-monitor
// startup prompt.
struct WWBgColor { float r,g,b; const char *name; };
static const WWBgColor WW_BG_COLORS[] = {
    {0.00f, 0.00f, 0.55f, "BLUE"},
    {0.00f, 0.00f, 0.00f, "BLACK"},
    {0.00f, 0.22f, 0.00f, "GREEN"},
    {0.35f, 0.18f, 0.00f, "AMBER"},
    {0.25f, 0.00f, 0.25f, "MAGENTA"},
    {0.00f, 0.25f, 0.30f, "CYAN"},
};
static const int WW_BG_COLOR_COUNT = (int)(sizeof(WW_BG_COLORS)/sizeof(WW_BG_COLORS[0]));
static const int WW_BG_MONO_IDX    = 1; // "BLACK" — used when color monitor = No

// Persistent highscore table, shared for the life of the process. Willy is a
// point-scoring game (higher score wins), unlike a timed game, so it's opened
// with higherIsBetter=true. Constructed lazily on first use so we don't touch
// disk unless the Willy sub-game is actually entered.
static Highscores &willy_highscores() {
    static Highscores h(/*higherIsBetter=*/true);
    return h;
}
static const char *const WILLY_SCORE_CATEGORY = "willy";

// Center a plain-ASCII line by character count against the monospace cell
// grid (cs = cell width in px) — mirrors how the INTRO screen positions text,
// rather than trusting gl_text_width() for whole-line centering.
static float ww_center_x(int win_w, float cs, const char *text) {
    return ((float)win_w - (float)strlen(text) * cs) * 0.5f;
}

// Mirrors willy.py's score_desc thresholds
static const char *ww_score_desc(int score) {
    if(score < 1000) return "Didn't you even read the instructions?";
    if(score < 2000) return "If you can't say anything nice...";
    if(score < 3000) return "Okay. Maybe you're not so bad after all.";
    if(score < 4000) return "Wow! Absolutely mediocre!";
    if(score < 5000) return "Pretty darn good, for a vertebrate!";
    if(score < 6000) return "Well done! Do you often eat garbage?";
    return "Absolutely fantastic! You should consider a career as an earthworm!";
}

// mirrors willy.py's hiscore_msg: a top-3 finish gets the "official" line,
// otherwise merely making the top 10 gets the lesser one. We only keep one
// persistent table (no daily/all-time split like willy.py's hiscoreT/hiscoreP),
// so rank within that table stands in for the distinction.
static std::string ww_hiscore_tier_msg(int score) {
    auto top = willy_highscores().getScoresByDifficulty(WILLY_SCORE_CATEGORY);
    int rank = 1;
    for(const auto &sc : top) if(sc.time > score) rank++;
    return (rank <= 3) ? "You're an Official Nightcrawler!" : "You're a Daily Pinworm!";
}

struct WillyWoprState {
    WLevels     levels;
    std::string cur_level;
    int         level_num = 1;

    // Willy — mirrors WillyGame fields exactly
    int  wy=23, wx=0;
    int  pwy=23, pwx=0;
    std::string willy_direction = "RIGHT";

    int  willy_velocity_y = 0;
    bool jumping          = false;
    int  fall_speed       = 0;   // consecutive falling ticks, resets on landing

    // Movement — mirrors moving_continuously / continuous_direction / up_pressed / down_pressed
    bool        moving_continuously = false;
    std::string continuous_direction;
    bool        up_pressed   = false;
    bool        down_pressed = false;
    bool        grab_ladder  = false;  // one-shot: set on keypress while airborne

    // Balls
    std::vector<WBall> balls;
    int max_balls = 6;

    // Ballpit spawn pos
    int vx=0, vy=0;

    // Score / lives
    int score=0, lives=5, bonus=1000, life_adder=0;
    int bcount=0;

    // Timing (10fps game logic, same as game_options.fps default)
    double tick_acc  = 0.0;
    double tick_rate = 0.1;

    // Ball spawn timing
    double ball_spawn_acc   = 0.0;
    double ball_spawn_delay = 1.0;

    WSub   sub = WSub::PROMPT_LEVELS;
    double sub_timer = 0.0;
    int    flash_count = 0;
    int    death_wy = 0, death_wx = 0; // cell where Willy died (for localized flash)

    // Startup prompts ("Original 1980's levels or new levels?" / color monitor / sound)
    bool use_original_levels = false;
    bool color_monitor = true;
    bool sound_enabled  = true;
    int  bg_color_idx   = 0;   // index into WW_BG_COLORS, changeable with '[' / ']'

    // Highscore name entry
    std::string name_entry;
    bool        just_scored = false;  // true once addScore() has been called for this game
    std::string hiscore_tier_msg;     // "You're an Official Nightcrawler!" etc, set on death

    // Render geometry
    float rx0=0,ry0=0,rcw=0,rch=0;

    // Intro screen — bounding rect of the GitHub URL for click-to-open
    float link_x=0,link_y=0,link_w=0,link_h=0;

    std::mt19937 rng{std::random_device{}()};
};

// =============================================================================
// HELPERS  (mirrors willy.cpp helper functions)
// =============================================================================

static const std::string &wg(WillyWoprState *s,int r,int c) {
    return s->levels.get(s->cur_level,r,c);
}
static void ws(WillyWoprState *s,int r,int c,const std::string &t) {
    s->levels.set(s->cur_level,r,c,t);
}
static bool is_pipe(const std::string &t) {
    return t.size()>=4 && t.substr(0,4)=="PIPE";
}

// willy_game_can_move_to
static bool can_move(WillyWoprState *s,int r,int c) {
    if(r<0||r>=W_ROWS||c<0||c>=W_COLS) return false;
    const std::string &t=wg(s,r,c);
    return (t=="EMPTY"||t=="LADDER"||t=="PRESENT"||t=="BELL"||
            t=="UPSPRING"||t=="SIDESPRING"||t=="TACK"||t=="BALLPIT"||
            t=="WILLY_RIGHT"||t=="WILLY_LEFT");
}

// willy_game_is_on_solid_ground
static bool on_solid(WillyWoprState *s) {
    if(s->wy >= W_MAXROWS-1) return true;
    const std::string &cur  = wg(s,s->wy,s->wx);
    const std::string &below= wg(s,s->wy+1,s->wx);
    return (cur=="LADDER") || is_pipe(below);
}

// willy_game_jump — sets vertical velocity only, never touches horizontal state
static void do_jump(WillyWoprState *s) {
    if(s->willy_velocity_y != 0) return; // already airborne, ignore
    const std::string &cur  = wg(s,s->wy,s->wx);
    // On a ladder: only allow jump if standing on solid ground (pipe below),
    // i.e. bottom rung — otherwise space does nothing.
    if(cur=="LADDER" && !is_pipe(wg(s,s->wy+1,s->wx))) return;
    const std::string &below= wg(s,s->wy+1,s->wx);
    bool can_jump = (cur=="UPSPRING") || is_pipe(below) || (s->wy==W_MAXROWS-1);
    if(can_jump) {
        s->jumping = true;
        // Pascal jcount goes 1-7: 3 up steps + 1 flat + 3 down = 3 rows height
        s->willy_velocity_y = (cur=="UPSPRING") ? -4 : -3;
        // moving_continuously and continuous_direction are intentionally NOT touched
        // Play jump sound: ascending tone per Pascal (25-wy)*100
        ww_snd_climb(s->wy - 1);
    }
}

// =============================================================================
// DIE / COMPLETE
// =============================================================================
static void ww_die(WillyWoprState *s) {
    s->lives--;
    s->death_wy  = s->wy;
    s->death_wx  = s->wx;
    ww_snd_splat();
    s->sub       = WSub::DEAD_WHITE;
    s->sub_timer = 0.0;
    s->flash_count = 0;
}

static void ww_complete(WillyWoprState *s) {
    s->score += s->bonus;
    ww_snd_win();
    s->sub       = WSub::WIN_PAUSE;
    s->sub_timer = 0.0;
}

static void ww_load_level(WillyWoprState *s, int num);

static void ww_next_level(WillyWoprState *s) {
    int saved_score  = s->score;
    int saved_lives  = s->lives;
    int saved_ladder = s->life_adder;
    int saved_maxb   = s->max_balls;
    int next = s->level_num + 1;
    if(!s->levels.exists("level"+std::to_string(next))) {
        next = 1;
        saved_maxb += 2;
        if(saved_maxb > 9) saved_maxb = 9;
    }
    ww_load_level(s, next);
    s->score      = saved_score;
    s->lives      = saved_lives;
    s->life_adder = saved_ladder;
    s->max_balls  = saved_maxb;
}

static void ww_load_level(WillyWoprState *s, int num) {
    s->level_num  = num;
    s->cur_level  = "level"+std::to_string(num);
    s->levels.reset(s->cur_level);

    auto start = s->levels.willy_start(s->cur_level);
    s->wy=start.first; s->wx=start.second;
    s->pwy=s->wy; s->pwx=s->wx;
    s->willy_direction = "RIGHT";

    s->willy_velocity_y = 0;
    s->jumping          = false;
    s->fall_speed       = 0;
    s->moving_continuously = false;
    s->continuous_direction.clear();
    s->up_pressed = s->down_pressed = false;

    auto pit = s->levels.ballpit(s->cur_level);
    s->vy=pit.first; s->vx=pit.second;

    s->balls.clear();
    s->ball_spawn_acc   = 0.0;
    s->ball_spawn_delay = 1.0;
    s->bonus    = 1000;
    s->bcount   = 0;
    s->sub      = WSub::PLAYING;
    s->sub_timer = 0.0;
}

// =============================================================================
// GAME TICK  — direct port of willy_game_update_willy_movement,
//              willy_game_update_balls, willy_game_check_collisions
// =============================================================================

static void ww_tick(WillyWoprState *s) {

    // ── bonus countdown (every 15 ticks, same as C++ bcount mod 15) ──────────
    s->bcount++;
    if(s->bcount%15==0) {
        s->bonus -= 10;
        if(s->bonus < 0) s->bonus = 0;
        if(s->bonus == 0) { ww_die(s); return; }
    }

    // ── willy_game_update_willy_movement ──────────────────────────────────────
    s->pwy=s->wy; s->pwx=s->wx;

    // up_pressed / down_pressed: since WOPR has no keyup dispatch, we keep
    // them set until the movement they request is no longer possible.
    // This makes holding UP climb the full ladder continuously.
    bool up_this_tick   = s->up_pressed;
    bool down_this_tick = s->down_pressed;
    // Do NOT clear them here — they stay set until ladder ends or key released.

    std::string cur_tile = wg(s,s->wy,s->wx);
    bool on_ladder  = (cur_tile=="LADDER");
    bool moved_ladder = false;

    // Up / down on ladder
    // If UP is pressed and Willy is already on a ladder, stop horizontal movement
    // immediately — climbing takes priority over left/right drift.
    if(up_this_tick && on_ladder) {
        s->moving_continuously = false;
        s->continuous_direction.clear();
    }
    if(up_this_tick) {
        int tr=s->wy-1;
        if(tr>=0) {
            std::string above=wg(s,tr,s->wx);
            if((on_ladder||above=="LADDER") && above=="LADDER" && can_move(s,tr,s->wx)) {
                // Already on / at base of ladder — climb up one step
                s->wy--; s->willy_velocity_y=0; moved_ladder=true;
                s->moving_continuously=false; s->continuous_direction.clear();
                ww_snd_climb(s->wy);
            } else if(!on_ladder) {
                // Not on a ladder — only seek one if Willy is grounded (not mid-jump).
                // While airborne the grab_ladder snap handles it; seeking while jumping
                // causes Willy to reverse direction past a ladder and double-back.
                if(s->willy_velocity_y == 0 && on_solid(s)) {
                    // Find the nearest ladder column, but only in the direction Willy
                    // is already moving (or either direction if standing still).
                    int best_col  = -1;
                    int best_dist = W_COLS;
                    for(int dc = 1; dc < W_COLS; dc++) {
                        for(int sign : {-1, 1}) {
                            int nc = s->wx + sign * dc;
                            if(nc < 0 || nc >= W_COLS) continue;
                            // Don't seek against current movement direction
                            if(s->moving_continuously) {
                                if(s->continuous_direction=="LEFT"  && sign > 0) continue;
                                if(s->continuous_direction=="RIGHT" && sign < 0) continue;
                            }
                            std::string at_nc  = wg(s, s->wy,   nc);
                            std::string abv_nc = wg(s, s->wy-1, nc);
                            if((at_nc=="LADDER" || abv_nc=="LADDER") && dc < best_dist) {
                                best_dist = dc;
                                best_col  = nc;
                            }
                        }
                        if(best_col >= 0) break;
                    }
                    if(best_col >= 0) {
                        int step = (best_col > s->wx) ? 1 : -1;
                        int nx   = s->wx + step;
                        if(can_move(s, s->wy, nx)) {
                            s->wx = nx;
                            s->willy_direction = (step > 0) ? "RIGHT" : "LEFT";
                            moved_ladder = true;
                        }
                    }
                    // If no ladder found in movement direction, just wait — don't clear up_pressed
                }
            } else {
                // On a ladder but can't go further up (top reached)
                s->up_pressed = false;
            }
        } else {
            s->up_pressed=false;
        }
    }
    if(down_this_tick && !moved_ladder) {
        int tr=s->wy+1;
        if(tr<W_ROWS) {
            std::string below=wg(s,tr,s->wx);
            bool can_down = (on_ladder && can_move(s,tr,s->wx)) ||
                            (below=="LADDER" && can_move(s,tr,s->wx));
            if(can_down) {
                // Already on / at top of ladder — descend one step
                s->wy++; s->willy_velocity_y=0; moved_ladder=true;
                s->moving_continuously=false; s->continuous_direction.clear();
                ww_snd_climb(s->wy);
            } else if(!on_ladder) {
                // Not on a ladder — find nearest reachable ladder column and walk toward it
                int best_col  = -1;
                int best_dist = W_COLS;
                for(int dc = 1; dc < W_COLS; dc++) {
                    for(int sign : {-1, 1}) {
                        int nc = s->wx + sign * dc;
                        if(nc < 0 || nc >= W_COLS) continue;
                        std::string at_nc  = wg(s, s->wy,   nc);
                        std::string blw_nc = (s->wy+1 < W_ROWS) ? wg(s, s->wy+1, nc) : "EMPTY";
                        if((at_nc=="LADDER" || blw_nc=="LADDER") && dc < best_dist) {
                            best_dist = dc;
                            best_col  = nc;
                        }
                    }
                    if(best_col >= 0) break;
                }
                if(best_col >= 0 && on_solid(s)) {
                    int step = (best_col > s->wx) ? 1 : -1;
                    int nx   = s->wx + step;
                    if(can_move(s, s->wy, nx)) {
                        s->wx = nx;
                        s->willy_direction = (step > 0) ? "RIGHT" : "LEFT";
                        moved_ladder = true;
                    }
                } else {
                    s->down_pressed = false;
                }
            } else {
                // On a ladder but already at bottom
                s->down_pressed = false;
            }
        } else {
            s->down_pressed=false;
        }
    }

    // Horizontal movement — moving_continuously persists through jumps
    if(!moved_ladder) {
        if(s->moving_continuously && !s->continuous_direction.empty()) {
            bool hit = false;
            if(s->continuous_direction=="LEFT") {
                if(can_move(s,s->wy,s->wx-1)) s->wx--;
                else hit=true;
            } else if(s->continuous_direction=="RIGHT") {
                if(can_move(s,s->wy,s->wx+1)) s->wx++;
                else hit=true;
            }
            if(hit) { s->moving_continuously=false; s->continuous_direction.clear(); }
        }
    }

    // Gravity / jump (vertical velocity) — mirrors willy_game_update_willy_movement
    cur_tile  = wg(s,s->wy,s->wx);
    on_ladder = (cur_tile=="LADDER");

    // Mid-air ladder snap: only when Willy is airborne, hits a LADDER tile,
    // AND a key was explicitly pressed this tick (grab_ladder). Passive flight
    // passes through ladders freely — you must press a key to grab.
    if(on_ladder && s->willy_velocity_y != 0 && s->grab_ladder) {
        s->willy_velocity_y = 0;
        s->jumping = false;
        s->moving_continuously = false;
        s->continuous_direction.clear();
    }
    s->grab_ladder = false; // consumed — clear every tick

    if(!on_ladder) {
        if(s->willy_velocity_y < 0) {
            // Moving upward (jumping) — just advance velocity toward 0, no gravity yet
            int ny = s->wy - 1;
            if(can_move(s,ny,s->wx)) s->wy = ny;
            s->willy_velocity_y++;
            s->fall_speed = 0;  // airborne going up, reset fall counter
        } else {
            // Not jumping upward — apply gravity
            if(!on_solid(s)) {
                s->willy_velocity_y += 1;
            } else {
                if(s->willy_velocity_y > 0) {
                    // Just landed — play thud scaled to fall distance
                    if(s->fall_speed > 1) ww_snd_climb(s->wy);
                    s->fall_speed = 0;
                    s->willy_velocity_y=0; s->jumping=false;
                }
            }
            if(s->willy_velocity_y > 0) {
                int ny = s->wy + 1;
                if(can_move(s,ny,s->wx)) {
                    s->wy = ny;
                    s->fall_speed++;
                }
            }
        }
    } else {
        if(s->fall_speed > 1) ww_snd_climb(s->wy);
        s->fall_speed = 0;
        s->willy_velocity_y=0; s->jumping=false;
    }

    // ── willy_game_check_collisions ───────────────────────────────────────────
    int y=s->wy, x=s->wx;
    cur_tile = wg(s,y,x);

    // Destroyable pipe left behind
    if((s->pwy!=y||s->pwx!=x) && s->pwy+1<W_ROWS) {
        if(wg(s,s->pwy+1,s->pwx)=="PIPE18") {
            ws(s,s->pwy+1,s->pwx,"EMPTY");
            s->score += 50;
        }
    }

    // Ball collision — current position, plus vertical cross-through.
    // Cross-through only fires when Willy moved vertically AND the ball is on
    // the exact same column AND the ball's row lies strictly between Willy's
    // old and new rows (i.e. they passed through each other).
    for(auto &b:s->balls) {
        std::string bt=wg(s,b.row,b.col);
        if(bt=="BALLPIT") continue;
        // Same cell now
        if(b.row==y && b.col==x) { ww_die(s); return; }
        // Cross-through: only when same column and ball is between old and new row
        if(b.col==x && s->pwy!=y) {
            int rlo=std::min(s->pwy,y), rhi=std::max(s->pwy,y);
            if(b.row>rlo && b.row<rhi) { ww_die(s); return; }
        }
    }

    // Tile interactions
    if(cur_tile=="TACK") { ww_die(s); return; }
    if(cur_tile=="BELL") { ww_complete(s); return; }
    if(cur_tile=="PRESENT") {
        s->score += 100;
        s->life_adder += 100;
        if(s->life_adder >= W_NEWLIFE) { s->lives++; s->life_adder-=W_NEWLIFE; ww_snd_life(); }
        ww_snd_pts();
        ws(s,y,x,"EMPTY");
    }
    if(cur_tile=="UPSPRING") {
        // Force a bounce regardless of whether Willy is already airborne —
        // landing on a spring from above must always launch him back up.
        s->jumping          = true;
        s->willy_velocity_y = -4;   // stronger than a normal jump (-3)
        s->fall_speed       = 0;
        ww_snd_climb(s->wy - 1);
    }
    if(cur_tile=="SIDESPRING") {
        if(s->moving_continuously) {
            if(s->continuous_direction=="RIGHT") { s->continuous_direction="LEFT"; s->willy_direction="LEFT"; }
            else { s->continuous_direction="RIGHT"; s->willy_direction="RIGHT"; }
        } else {
            s->willy_direction = (s->willy_direction=="RIGHT")?"LEFT":"RIGHT";
        }
    }

    // Jump-over bonus
    if(s->jumping || s->willy_velocity_y!=0) {
        for(int i=1;i<5;i++) {
            int cy=y+i; if(cy>=W_ROWS) break;
            for(auto &b:s->balls)
                if(b.row==cy && b.col==x) { s->score+=20; ww_snd_pts(); break; }
        }
    }

    // ── willy_game_update_balls ───────────────────────────────────────────────
    // Gravity + horizontal roll, mirrors C++ exactly
    auto pit = s->levels.ballpit(s->cur_level);
    int pit_r=pit.first, pit_c=pit.second;

    // Relocate any ball that fell into a secondary ballpit
    for(auto &b:s->balls) {
        std::string t=wg(s,b.row,b.col);
        if(t=="BALLPIT" && (b.row!=pit_r||b.col!=pit_c)) {
            b.row=pit_r; b.col=pit_c; b.dir.clear();
        }
    }

    for(auto &b:s->balls) {
        if(b.row < W_MAXROWS-1) {
            std::string below=wg(s,b.row+1,b.col);
            if(!is_pipe(below)) {
                // fall
                b.row++; b.dir.clear();
            } else {
                // on platform — move horizontally
                if(b.dir.empty()) {
                    std::uniform_real_distribution<float> d(0,1);
                    b.dir = (d(s->rng)<0.5f)?"RIGHT":"LEFT";
                }
                if(b.dir=="RIGHT") {
                    if(b.col+1<W_MAXCOLS) {
                        std::string r=wg(s,b.row,b.col+1);
                        if(!is_pipe(r)) b.col++;
                        else b.dir="LEFT";
                    } else b.dir="LEFT";
                } else {
                    if(b.col-1>=0) {
                        std::string l=wg(s,b.row,b.col-1);
                        if(!is_pipe(l)) b.col--;
                        else b.dir="RIGHT";
                    } else b.dir="RIGHT";
                }
            }
        }
    }

    // Post-ball-move collision: ball landed on Willy after its move this tick.
    // No cross-through here — balls only move horizontally on platforms so
    // a vertical swap with Willy can't happen during ball movement.
    {
        int wy=s->wy, wx=s->wx;
        std::string wt=wg(s,wy,wx);
        for(auto &b:s->balls) {
            std::string bt=wg(s,b.row,b.col);
            if(bt=="BALLPIT") continue;
            if(b.row==wy && b.col==wx && wt!="BALLPIT") { ww_die(s); return; }
        }
    }
}

// =============================================================================
// RENDER
// =============================================================================

void wopr_willy_render(WoprState *w, int px, int py, int cw, int ch, int /*cols*/) {
    if(!w->sub_state) return;
    WillyWoprState *s = static_cast<WillyWoprState*>(w->sub_state);

    // Cell size: use actual window dimensions so all rows + status bar fit.
    int ww=1280, wh=720;
    SDL_Window *win=SDL_GL_GetCurrentWindow();
    if(win) SDL_GetWindowSize(win,&ww,&wh);

    // Current background color (cycled with '[' / ']', seeded by the color-monitor prompt)
    const WWBgColor &bg = WW_BG_COLORS[s->bg_color_idx % WW_BG_COLOR_COUNT];

    // ── LEVEL PACK STARTUP PROMPT ───────────────────────────────────────────
    if(s->sub == WSub::PROMPT_LEVELS) {
        gl_draw_rect(0.f,0.f,(float)ww,(float)wh, 0.f,0.f,0.f,1.f);
        float cs = (float)cw;
        float y  = (float)wh * 0.38f;

        const char *q1 = "PLAY THE ORIGINAL 1980's LEVELS";
        const char *q2 = "OR THE NEW LEVELS?";
        gl_draw_text(q1, ww_center_x(ww,cs,q1), y, 1.f,1.f,1.f,1.f,1.f);
        y += cs * 1.6f;
        gl_draw_text(q2, ww_center_x(ww,cs,q2), y, 1.f,1.f,1.f,1.f,1.f);
        y += cs * 2.5f;

        const char *hint = "PRESS O FOR ORIGINAL  -  N FOR NEW";
        gl_draw_text(hint, ww_center_x(ww,cs,hint), y, 0.6f,0.6f,0.6f,1.f,1.f);

        gl_flush_verts();
        return;
    }

    // ── COLOR / SOUND STARTUP PROMPTS ───────────────────────────────────────
    // Always plain black-and-white here — we don't know yet whether the
    // person even has a color monitor, so showing color on this screen
    // would defeat the point of asking.
    if(s->sub == WSub::PROMPT_COLOR || s->sub == WSub::PROMPT_SOUND) {
        gl_draw_rect(0.f,0.f,(float)ww,(float)wh, 0.f,0.f,0.f,1.f);
        float cs = (float)cw;
        float y  = (float)wh * 0.42f;

        const char *q1 = "ARE YOU USING A COLOR MONITOR?";
        const char *q2 = "DO YOU WANT SOUND EFFECTS?";
        bool onQ1 = (s->sub == WSub::PROMPT_COLOR);

        gl_draw_text(q1, ww_center_x(ww,cs,q1), y, 1.f,1.f,1.f,1.f,1.f);
        if(!onQ1) {
            std::string ans = std::string("  ") + (s->color_monitor ? "YES" : "NO");
            gl_draw_text(ans.c_str(), ww_center_x(ww,cs,q1) + (float)strlen(q1)*cs, y, 1.f,1.f,1.f,1.f,1.f);
        }
        y += cs * 2.f;

        if(!onQ1) gl_draw_text(q2, ww_center_x(ww,cs,q2), y, 1.f,1.f,1.f,1.f,1.f);
        y += cs * 2.5f;

        const char *hint = "PRESS Y OR N";
        gl_draw_text(hint, ww_center_x(ww,cs,hint), y, 0.6f,0.6f,0.6f,1.f,1.f);

        gl_flush_verts();
        return;
    }

    // ── INTRO SCREEN ─────────────────────────────────────────────────────────
    if(s->sub == WSub::INTRO) {
        gl_draw_rect(0.f,0.f,(float)ww,(float)wh, bg.r,bg.g,bg.b,1.f);

        // Every glyph is g_font_size × g_font_size pixels (see wopr_render.h).
        float cs = (float)cw;

        struct Seg   { const char *txt; int spr; };
        struct ILine { Seg s[6]; int n; };  // n==0 → blank line (still takes vertical space)

        const ILine L[] = {

            {{{"Willy the Worm (WOPR Port)", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"By Jason Hall", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"https://github.com/jasonbrianhall/willytheworm", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"(original version by Alan Farmer 1985)", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"", -1}}, 1},

            {{{"This code is Free Open Source Software (FOSS)", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"Please feel free to do with it whatever you wish.", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"If you do make changes though such as new levels,", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"please share them with the world.", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"Meet Willy the Worm. Willy is a fun-loving invertebrate who likes to climb", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"ladders, bounce on springs, and find his presents.", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"But more than anything, Willy loves to ring bells!", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"", -1}}, 1},

            {{{"Use WASD or the arrow keys \xe2\x86\x90 \xe2\x86\x91 \xe2\x86\x92 \xe2\x86\x93", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"to make Willy run and climb, or the", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"space bar to make him jump. Anything", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"else will make Willy stop and wait", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"", -1}}, 1},

            {{{"Good luck, and don't let Willy step on", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"a tack or get ran over by a ball!", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"", -1}}, 1},

            {{{"Cheat keys: Ctrl+L adds a life,  Ctrl+N skips to next level.", -1}}, 1},
            {{{"", -1}}, 1},
            {{{"", -1}}, 1},

            {{{"Press Enter to Continue", -1}}, 1},
        };

        const int NL = (int)(sizeof(L)/sizeof(L[0]));

        // Count UTF-8 codepoints — each renders as exactly 1 character cell.
        auto count_cols = [](const char *str) -> int {
            int cols = 0;
            const unsigned char *p = reinterpret_cast<const unsigned char*>(str);
            while(*p) {
                if     ((*p & 0x80)==0x00) p+=1;
                else if((*p & 0xE0)==0xC0) p+=2;
                else if((*p & 0xF0)==0xE0) p+=3;
                else                       p+=4;
                cols++;
            }
            return cols;
        };

        auto line_cols = [&](int i) -> int {
            int w = 0;
            for(int j = 0; j < L[i].n; j++) {
                if(L[i].s[j].spr < 0 && L[i].s[j].txt)
                    w += count_cols(L[i].s[j].txt);
                else if(L[i].s[j].spr >= 0)
                    w += 1;
            }
            return w;
        };

        float start_y = ((float)wh - NL * cs) * 0.5f;
        if(start_y < 0.f) start_y = 0.f;

        static const char *GITHUB_URL = "https://github.com/jasonbrianhall/willytheworm";

        for(int i = 0; i < NL; i++) {
            float y = start_y + i * cs;
            if(L[i].n == 0) continue;  // blank — y already advances via i*cs, nothing to draw

            float x = ((float)ww - line_cols(i) * cs) * 0.5f;
            for(int j = 0; j < L[i].n; j++) {
                const Seg &sg = L[i].s[j];
                if(sg.spr < 0 && sg.txt) {
                    bool is_url = (sg.txt == GITHUB_URL ||
                                   std::string(sg.txt) == GITHUB_URL);
                    if(is_url) {
                        // Render in cyan and record exact bounding rect for click handling.
                        // Use gl_text_width/height at the same scale (1.f) so the stored
                        // rect precisely matches the pixels drawn, regardless of cw truncation.
                        gl_draw_text(sg.txt, x, y, 0.f,1.f,1.f,1.f, 1.f);
                        s->link_x = x; s->link_y = y;
                        s->link_w = gl_text_width(sg.txt, 1.f);
                        s->link_h = gl_text_height(1.f);
                    } else {
                        gl_draw_text(sg.txt, x, y, 1.f,1.f,1.f,1.f, 1.f);
                    }
                    x += count_cols(sg.txt) * cs;
                } else if(sg.spr >= 0) {
                    ww_draw_sprite(sg.spr, x, y, cs, cs);
                    x += cs;
                }
            }
        }

        gl_flush_verts();
        return;
    }

    // ── GAME OVER ─────────────────────────────────────────────────────────
    if(s->sub == WSub::GAME_OVER) {
        gl_draw_rect(0.f,0.f,(float)ww,(float)wh, bg.r,bg.g,bg.b,1.f);
        float cs = (float)cw;
        float y = (float)wh * 0.30f;
        char buf[128];

        gl_draw_text("GAME OVER", ww_center_x(ww,cs,"GAME OVER"), y, 1.f,1.f,1.f,1.f,1.f);
        y += cs * 2.f;

        snprintf(buf, sizeof(buf), "FINAL SCORE: %d", s->score);
        gl_draw_text(buf, ww_center_x(ww,cs,buf), y, 1.f,1.f,1.f,1.f,1.f);
        y += cs * 1.6f;

        const char *desc = ww_score_desc(s->score);
        gl_draw_text(desc, ww_center_x(ww,cs,desc), y, 0.8f,0.8f,0.8f,1.f,1.f);
        y += cs * 1.8f;

        if(!s->hiscore_tier_msg.empty()) {
            gl_draw_text(s->hiscore_tier_msg.c_str(), ww_center_x(ww,cs,s->hiscore_tier_msg.c_str()), y, 1.f,1.f,0.f,1.f,1.f);
            y += cs * 2.f;
            const char *prompt = "PRESS ENTER TO ENTER YOUR NAME";
            gl_draw_text(prompt, ww_center_x(ww,cs,prompt), y, 1.f,1.f,1.f,1.f,1.f);
        } else {
            y += cs * 0.6f;
            const char *prompt = "PRESS ENTER TO PLAY AGAIN";
            gl_draw_text(prompt, ww_center_x(ww,cs,prompt), y, 1.f,1.f,1.f,1.f,1.f);
        }

        gl_flush_verts();
        return;
    }

    // ── NAME ENTRY (new high score) ────────────────────────────────────────
    if(s->sub == WSub::NAME_ENTRY) {
        gl_draw_rect(0.f,0.f,(float)ww,(float)wh, bg.r,bg.g,bg.b,1.f);
        float cs = (float)cw;
        char buf[96];
        std::string tier = s->hiscore_tier_msg.empty() ? "NEW HIGH SCORE!" : s->hiscore_tier_msg;
        snprintf(buf, sizeof(buf), "%s  (%d POINTS)", tier.c_str(), s->score);
        float y = (float)wh * 0.4f;
        gl_draw_text(buf, ww_center_x(ww,cs,buf), y, 1.f,1.f,0.f,1.f,1.f);
        y += cs * 2.f;
        std::string prompt = "ENTER YOUR NAME: " + s->name_entry + "_";
        gl_draw_text(prompt.c_str(), ww_center_x(ww,cs,prompt.c_str()), y, 1.f,1.f,1.f,1.f,1.f);
        y += cs * 2.f;
        const char *hint = "ENTER TO SUBMIT  -  ESC TO SKIP";
        gl_draw_text(hint, ww_center_x(ww,cs,hint), y, 0.6f,0.6f,0.6f,1.f,1.f);
        gl_flush_verts();
        return;
    }

    // ── HIGH SCORE TABLE ─────────────────────────────────────────────────
    if(s->sub == WSub::HIGHSCORES) {
        gl_draw_rect(0.f,0.f,(float)ww,(float)wh, bg.r,bg.g,bg.b,1.f);
        float cs = (float)cw;
        auto top = willy_highscores().getScoresByDifficulty(WILLY_SCORE_CATEGORY);

        const char *title = "WOPR NIGHTCRAWLERS - TOP SCORES";
        float y = (float)wh * 0.12f;
        gl_draw_text(title, ww_center_x(ww,cs,title), y, 1.f,1.f,0.f,1.f,1.f);
        y += cs * 2.f;

        for(size_t i=0; i<top.size(); i++) {
            char line[96];
            snprintf(line, sizeof(line), "%2d. %-16s %6d", (int)(i+1), top[i].name.c_str(), top[i].time);
            gl_draw_text(line, ww_center_x(ww,cs,line), y, 1.f,1.f,1.f,1.f,1.f);
            y += cs * 2.0f;
        }

        y += cs;
        const char *foot = "PRESS ENTER TO CONTINUE";
        gl_draw_text(foot, ww_center_x(ww,cs,foot), y, 1.f,1.f,1.f,1.f,1.f);
        gl_flush_verts();
        return;
    }

    // Shrink grid so the bottom row is fully clear of the status bar.
    // Reserve ch*2 below the grid: ch for the gap + ch for the text line.
    float avail_w = (float)(ww - px*2);
    float avail_h = (float)(wh - py) - (float)ch * 2.f - 8.f;
    float cell = std::min(avail_h/(float)W_ROWS, avail_w/(float)W_COLS);
    if(cell<4.f) cell=4.f;

    s->rx0=px; s->ry0=py; s->rcw=cell; s->rch=cell;

    float gh=cell*W_ROWS; (void)(cell*W_COLS);

    // Fill the entire window with blue — game area + margins + status strip
    gl_draw_rect(0.f, 0.f, (float)ww, (float)wh, bg.r, bg.g, bg.b, 1.f);

    // Death: expanding white burst around the death cell, fading out
    if(s->sub==WSub::DEAD_WHITE) {
        // sub_timer runs 0..1 s — use it to drive radius and alpha
        float t    = (float)(s->sub_timer);          // 0..1
        float rad  = (t * 6.f + 0.5f) * cell;        // starts tight, expands ~6 cells
        float alpha= 1.f - t;                         // fades from opaque to transparent
        float cx   = (float)px + (s->death_wx + 0.5f) * cell;
        float cy   = (float)py + (s->death_wy + 0.5f) * cell;
        // Draw a square burst (looks retro and fast to render)
        for(int ring = 0; ring < 4; ring++) {
            float r2 = rad * (1.f - ring * 0.22f);
            float a2 = alpha * (1.f - ring * 0.2f);
            if(a2 <= 0.f) break;
            gl_draw_rect(cx - r2, cy - r2, r2*2.f, r2*2.f, 1.f,1.f,1.f, a2);
        }
    }

    // Level grid — all sprites white
    for(int r=0;r<W_ROWS;r++) {
        for(int c=0;c<W_COLS;c++) {
            const std::string &tile=wg(s,r,c);
            if(tile=="EMPTY") continue;
            if(r==s->wy&&c==s->wx) continue; // Willy covers it
            int idx=ww_sprite_idx(tile); if(idx<0) continue;
            ww_draw_sprite(idx, (float)px+c*cell, (float)py+r*cell, cell,cell);
        }
    }

    // Balls — white
    for(auto &b:s->balls) {
        if(b.row<0||b.row>=W_ROWS||b.col<0||b.col>=W_COLS) continue;
        if(wg(s,b.row,b.col)=="BALLPIT") continue;
        if(b.row==s->wy&&b.col==s->wx) continue;
        ww_draw_sprite(7, (float)px+b.col*cell, (float)py+b.row*cell, cell,cell);
    }

    // Willy — white, blink during flash
    bool draw_willy = (s->sub != WSub::DEAD_WHITE);
    if(draw_willy && s->wy>=0&&s->wy<W_ROWS&&s->wx>=0&&s->wx<W_COLS) {
        int idx = (s->willy_direction=="RIGHT")?0:1;
        ww_draw_sprite(idx, (float)px+s->wx*cell, (float)py+s->wy*cell, cell,cell);
    }

    // Status bar — sits below the game grid, blue background, white text
    // Position it centered in the gap between grid bottom and window bottom
    float gap    = (float)(wh) - ((float)py + gh);
    float sy     = (float)py + gh + gap * 0.35f;
    // Blue strip filling the gap area
    gl_draw_rect(0.f, (float)py+gh, (float)ww, gap, bg.r,bg.g,bg.b,1.f);

    {
        char buf[160];
        snprintf(buf,sizeof(buf),
                 "SCORE: %6d    BONUS: %4d    LEVEL: %2d    WILLY THE WORMS LEFT: ",
                 s->score, s->bonus, s->level_num);
        gl_draw_text(buf,(float)px,sy, 1.f,1.f,1.f,1.f,1.f);

        // One small Willy sprite per remaining life, equally spaced, in place
        // of the old "%2d" life count.
        //
        // The vertical nudge below lines the icon row up with the text
        // baseline. This differs by renderer: the real WOPR gl_draw_text()
        // anchors text one cell lower than ww_draw_sprite()'s top-left
        // sprites (needs the nudge), while the standalone wopr_render.cpp
        // anchors both the same way (needs none). -DWOPR selects which.
        #ifdef WOPR
        static const float WW_LIFE_ICON_Y_NUDGE = 1.0f;
        #else
        static const float WW_LIFE_ICON_Y_NUDGE = 0.0f;
        #endif
        float icon_cs  = (float)cw;
        float icon_x   = (float)px + (float)strlen(buf) * icon_cs;
        float icon_gap = icon_cs * 1.4f;
        int   lives_shown = s->lives > 0 ? s->lives : 0;
        for(int i = 0; i < lives_shown; i++) {
            ww_draw_sprite(0 /* Willy, facing right */, icon_x + i*icon_gap,
                            sy - WW_LIFE_ICON_Y_NUDGE*icon_cs, icon_cs, icon_cs);
        }
    }

    gl_flush_verts();
}

// =============================================================================
// UPDATE
// =============================================================================

void wopr_willy_update(WoprState *w, double dt) {
    if(!w->sub_state) return;
    WillyWoprState *s=static_cast<WillyWoprState*>(w->sub_state);

    if(s->sub==WSub::INTRO) return;  // nothing ticks during intro

    if(s->sub==WSub::DEAD_WHITE) {
        s->sub_timer += dt;
        if(s->sub_timer >= 1.0) {  // hold white for 1 second
            if(s->lives<0) {
                s->hiscore_tier_msg = (s->score>0 && willy_highscores().isHighScore(s->score, WILLY_SCORE_CATEGORY))
                                        ? ww_hiscore_tier_msg(s->score) : "";
                s->sub=WSub::GAME_OVER;
            }
            else            { ww_load_level(s,s->level_num); }
        }
        return;
    }
    if(s->sub==WSub::WIN_PAUSE) {
        s->sub_timer += dt;
        if(s->sub_timer>0.8) ww_next_level(s);
        return;
    }
    if(s->sub!=WSub::PLAYING) return;

    s->ball_spawn_acc += dt;
    auto pit_pos = s->levels.ballpit(s->cur_level);
    auto &cur_lv = s->levels.levels[s->cur_level];
    if((int)s->balls.size()<s->max_balls && s->ball_spawn_acc>=s->ball_spawn_delay
       && cur_lv.has_ballpit) {
        s->ball_spawn_acc=0.0;
        std::uniform_real_distribution<double> dd(0.5,2.0);
        s->ball_spawn_delay=dd(s->rng);
        s->balls.push_back({pit_pos.first, pit_pos.second, ""});
    }

    s->tick_acc += dt;
    while(s->tick_acc>=s->tick_rate) {
        s->tick_acc -= s->tick_rate;
        ww_tick(s);
        if(s->sub!=WSub::PLAYING) break;
    }
}

// =============================================================================
// KEYBOARD  — mirrors willy_game_on_key_press exactly
// Space calls do_jump() which does NOT touch moving_continuously.
// Any other key (not arrow/space) clears moving_continuously.
// =============================================================================

bool wopr_willy_keydown(WoprState *w, SDL_Keycode sym) {
    if(!w->sub_state) return false;
    WillyWoprState *s=static_cast<WillyWoprState*>(w->sub_state);

    // Global: cycle the background color from anywhere in the sub-game.
    if(sym==SDLK_LEFTBRACKET)  { s->bg_color_idx = (s->bg_color_idx + WW_BG_COLOR_COUNT - 1) % WW_BG_COLOR_COUNT; return true; }
    if(sym==SDLK_RIGHTBRACKET) { s->bg_color_idx = (s->bg_color_idx + 1) % WW_BG_COLOR_COUNT; return true; }

    if(s->sub==WSub::PROMPT_LEVELS) {
        if(sym==SDLK_o || sym==SDLK_n) {
            bool want_original = (sym==SDLK_o);
            if(want_original != s->use_original_levels) {
                WLevels tmp;
                if(ww_load_levels(tmp, want_original) && tmp.exists("level1")) {
                    s->levels = tmp;
                    s->use_original_levels = want_original;
                    ww_load_level(s,1);
                }
                // else: pack failed to load (e.g. original_levels.json missing on
                // disk with no embedded copy) — silently keep whichever pack is
                // already active rather than breaking the game.
            }
            s->sub = WSub::PROMPT_COLOR;
        }
        return true;
    }

    if(s->sub==WSub::PROMPT_COLOR) {
        if(sym==SDLK_y) { s->color_monitor=true;  s->sub=WSub::PROMPT_SOUND; }
        else if(sym==SDLK_n) {
            s->color_monitor=false;
            s->bg_color_idx = WW_BG_MONO_IDX;   // drop to a single-hue "monochrome monitor" look
            s->sub=WSub::PROMPT_SOUND;
        }
        return true;
    }

    if(s->sub==WSub::PROMPT_SOUND) {
        if(sym==SDLK_y) { s->sound_enabled=true;  s_sound_on=true;  s->sub=WSub::INTRO; }
        else if(sym==SDLK_n) { s->sound_enabled=false; s_sound_on=false; s->sub=WSub::INTRO; }
        return true;
    }

    if(s->sub==WSub::INTRO) {
        if(sym==SDLK_RETURN||sym==SDLK_KP_ENTER||sym==SDLK_SPACE)
            s->sub=WSub::PLAYING;
        return true;
    }

    if(s->sub==WSub::GAME_OVER) {
        if(sym==SDLK_RETURN||sym==SDLK_KP_ENTER) {
            if(!s->hiscore_tier_msg.empty()) {
                s->name_entry.clear();
                s->just_scored = false;
                s->sub = WSub::NAME_ENTRY;
            } else {
                int saved_maxb=s->max_balls;
                ww_load_level(s,1);
                s->score=0; s->lives=5; s->life_adder=0;
                s->max_balls=saved_maxb;
                s->sub=WSub::INTRO;  // back to intro
            }
        }
        return true;
    }

    if(s->sub==WSub::NAME_ENTRY) {
        if(sym==SDLK_RETURN||sym==SDLK_KP_ENTER) {
            if(!s->just_scored) {
                std::string nm = s->name_entry.empty() ? "ANONYMOUS" : s->name_entry;
                willy_highscores().addScore({nm, s->score, WILLY_SCORE_CATEGORY});
                s->just_scored = true;
            }
            s->sub = WSub::HIGHSCORES;
        } else if(sym==SDLK_ESCAPE) {
            s->sub = WSub::GAME_OVER;   // bail out without recording a name
        } else if(sym==SDLK_BACKSPACE) {
            if(!s->name_entry.empty()) s->name_entry.pop_back();
        } else if(s->name_entry.size() < 16) {
            char c = 0;
            if(sym>=SDLK_a && sym<=SDLK_z)      c = 'A' + (char)(sym - SDLK_a);
            else if(sym>=SDLK_0 && sym<=SDLK_9) c = '0' + (char)(sym - SDLK_0);
            else if(sym==SDLK_SPACE)            c = ' ';
            else if(sym==SDLK_MINUS)            c = '-';
            if(c) s->name_entry += c;
        }
        return true;
    }

    if(s->sub==WSub::HIGHSCORES) {
        if(sym==SDLK_RETURN||sym==SDLK_KP_ENTER||sym==SDLK_SPACE) {
            int saved_maxb=s->max_balls;
            ww_load_level(s,1);
            s->score=0; s->lives=5; s->life_adder=0;
            s->max_balls=saved_maxb;
            s->sub=WSub::INTRO;  // back to intro
        }
        return true;
    }
    if(s->sub!=WSub::PLAYING) return true;

    // Any keypress while airborne arms the one-shot ladder-grab.
    if(s->willy_velocity_y != 0 || s->jumping)
        s->grab_ladder = true;

    switch (sym) {

        case SDLK_SPACE:
            do_jump(s);
            break;
        case SDLK_LEFT:
        case SDLK_a:
            s->continuous_direction = "LEFT";
            s->moving_continuously = true;
            s->willy_direction = "LEFT";
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            s->continuous_direction = "RIGHT";
            s->moving_continuously = true;
            s->willy_direction = "RIGHT";
            break;

        case SDLK_UP:
        case SDLK_w:
            s->up_pressed = true;
            break;
        case SDLK_DOWN:
        case SDLK_s:
            s->down_pressed = true;
            break;

        case SDLK_l:
            if(SDL_GetModState() & KMOD_CTRL) {
                s->lives++;
                ww_snd_life();
            }
            break;

        case SDLK_n:
            if(SDL_GetModState() & KMOD_CTRL) {
                ww_next_level(s);
            }
            break;

        default:
            // Any other key stops Willy
            if (sym>0) {
                s->moving_continuously = false;
                s->continuous_direction.clear();
                s->up_pressed = false;
                s->down_pressed = false;
                s->willy_velocity_y = 0;
                s->jumping = false;
                break;
            }
    }
    return true;
}

void wopr_willy_keyup(WoprState *w, SDL_Keycode sym) {
    if(!w->sub_state) return;
    WillyWoprState *s=static_cast<WillyWoprState*>(w->sub_state);
    switch(sym) {
        case SDLK_UP:   case SDLK_w: s->up_pressed=false;   break;
        case SDLK_DOWN: case SDLK_s: s->down_pressed=false;  break;
        case SDLK_LEFT: case SDLK_a:
            if(s->continuous_direction=="LEFT") {
                s->moving_continuously=false; s->continuous_direction.clear();
            } break;
        case SDLK_RIGHT: case SDLK_d:
            if(s->continuous_direction=="RIGHT") {
                s->moving_continuously=false; s->continuous_direction.clear();
            } break;
        default: break;
    }
}

// =============================================================================
// MOUSE
// =============================================================================

void wopr_willy_mousedown(WoprState *w, int mx, int my, int button) {
    if(!w->sub_state) return;
    WillyWoprState *s=static_cast<WillyWoprState*>(w->sub_state);

    // Intro screen: click on the GitHub URL to open it in a browser.
    // Intro screen: click on the GitHub URL to open it.
    // Mirrors chess mousedown pattern: SDL_BUTTON_LEFT, int coords vs float rects.
    if(s->sub == WSub::INTRO && button == SDL_BUTTON_LEFT && s->link_w > 0.f) {
        // Generous hit region: full width of URL, 3x line height vertically
        float pad_y = s->link_h;
        if(mx >= (int)s->link_x && mx <= (int)(s->link_x + s->link_w) &&
           my >= (int)(s->link_y - pad_y) && my <= (int)(s->link_y + s->link_h + pad_y)) {
            SDL_OpenURL("https://github.com/jasonbrianhall/willytheworm");
            return;
        }
    }

    if(s->sub!=WSub::PLAYING||s->rcw==0.f) return;
    int dc=(int)((mx-s->rx0)/s->rcw)-s->wx;
    int dr=(int)((my-s->ry0)/s->rch)-s->wy;
    if(button==1) {
        if(std::abs(dc)>=std::abs(dr)) {
            s->continuous_direction=(dc>0)?"RIGHT":"LEFT";
            s->moving_continuously=true;
            s->willy_direction=s->continuous_direction;
        } else {
            if(dr<0) s->up_pressed=true;
            else     s->down_pressed=true;
        }
    } else if(button==3) { do_jump(s); }
    else if(button==2)   { s->moving_continuously=false; s->continuous_direction.clear(); }
}

static void willy_set_cursor(SDL_SystemCursor id) {
    static SDL_SystemCursor last = SDL_SYSTEM_CURSOR_ARROW;
    if(id != last) { SDL_SetCursor(SDL_CreateSystemCursor(id)); last = id; }
}

void wopr_willy_mousemove(WoprState *w, int x, int y) {
    if(!w->sub_state) return;
    WillyWoprState *s = static_cast<WillyWoprState*>(w->sub_state);
    if(s->sub == WSub::INTRO && s->link_w > 0.f) {
        float pad_y = s->link_h;
        bool over = (x >= (int)s->link_x && x <= (int)(s->link_x + s->link_w) &&
                     y >= (int)(s->link_y - pad_y) && y <= (int)(s->link_y + s->link_h + pad_y));
        willy_set_cursor(over ? SDL_SYSTEM_CURSOR_HAND : SDL_SYSTEM_CURSOR_ARROW);
        return;
    }
    willy_set_cursor(SDL_SYSTEM_CURSOR_ARROW);
}

void wopr_willy_mouseup(WoprState *w, int,int,int button) {
    if(!w->sub_state) return;
    WillyWoprState *s=static_cast<WillyWoprState*>(w->sub_state);
    if(button==1) {
        s->moving_continuously=false; s->continuous_direction.clear();
        s->up_pressed=s->down_pressed=false;
    }
}

// =============================================================================
// ENTER / FREE
// =============================================================================

void wopr_willy_enter(WoprState *w) {
    if(w->sub_state) { delete static_cast<WillyWoprState*>(w->sub_state); w->sub_state=nullptr; }

    if(!s_chr_loaded && !ww_chr_load()) {
        w->lines.push_back("  ERROR: willy.chr NOT FOUND.");
        w->lines.push_back("  COPY willy.chr NEXT TO THE BINARY OR RUN gen_willy_assets.py.");
        return;
    }

    auto *s = new WillyWoprState();
    if(!ww_load_levels(s->levels, false)) {
        w->lines.push_back("  ERROR: levels.json NOT FOUND.");
        w->lines.push_back("  COPY levels.json NEXT TO THE BINARY OR RUN gen_willy_assets.py.");
        delete s; return;
    }
    if(!s->levels.exists("level1")) {
        w->lines.push_back("  ERROR: levels.json HAS NO 'level1'.");
        delete s; return;
    }

    ww_audio_init();
    s->max_balls=6; s->score=0; s->lives=5; s->life_adder=0;
    ww_load_level(s,1);
    s->sub = WSub::PROMPT_LEVELS;  // "Original 1980's levels, or the new levels?" first
    w->sub_state=s;
}

void wopr_willy_free(WoprState *w) {
    if(w->sub_state) {
        ww_audio_shutdown();
        delete static_cast<WillyWoprState*>(w->sub_state);
        w->sub_state=nullptr;
    }
}

void wopr_willy_textinput(WoprState *w, const char *t) { (void)w;(void)t; }

bool wopr_willy_escape_is_ingame(WoprState *w) {
    if(!w->sub_state) return false;
    WillyWoprState *s = static_cast<WillyWoprState*>(w->sub_state);
    return s->sub == WSub::NAME_ENTRY;  // Escape there skips submitting a name
}
