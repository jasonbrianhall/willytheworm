#include "willy.h"
#include <getopt.h>

// Editor-specific global variables
double redbg = 0.0;
double greenbg = 0.0;
double bluebg = 1.0;

// Editor game options
struct EditorGameOptions {
    std::string levels_file = "levels.json";
    int scale_factor = 3;
    bool show_help = false;
};

EditorGameOptions editor_options;

// Current item iterator for sprite selection
class SpriteIterator {
private:
    std::vector<std::string> sprite_names;
    size_t current_index;
    
public:
    SpriteIterator() : current_index(0) {
        // Add all available sprites in a logical order
        sprite_names = {
            "WILLY_RIGHT", "WILLY_LEFT", "PRESENT", "LADDER", "TACK", 
            "UPSPRING", "SIDESPRING", "BALL", "BELL", "BALLPIT", "EMPTY"
        };
        
        // Add all pipe variations
        for(int i = 1; i <= 40; i++) {
            sprite_names.push_back("PIPE" + std::to_string(i));
        }
    }
    
    std::string current() const {
        return sprite_names[current_index];
    }
    
    void next() {
        current_index = (current_index + 1) % sprite_names.size();
    }
    
    void previous() {
        if(current_index == 0) {
            current_index = sprite_names.size() - 1;
        } else {
            current_index--;
        }
    }
    
    size_t size() const {
        return sprite_names.size();
    }
};

class WillyEditor : public Gtk::Window {
private:
    Gtk::DrawingArea drawing_area;
    Gtk::Box vbox;
    Gtk::MenuBar menubar;
    Gtk::Label status_bar;
    
    std::unique_ptr<SpriteLoader> sprite_loader;
    std::unique_ptr<LevelLoader> level_loader;
    
    // Editor state
    int scale_factor;
    int current_level_num;
    std::string current_level;
    int max_levels;
    
    SpriteIterator sprite_iterator;
    
    // Scaling factors for window resizing
    double current_scale_x = 1.0;
    double current_scale_y = 1.0;
    int base_game_width;
    int base_game_height;
    
public:
    WillyEditor();
    ~WillyEditor();
    
private:
    void setup_ui();
    void create_menubar();
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
    void draw_intro_screen(const Cairo::RefPtr<Cairo::Context>& cr);
    void draw_editor_screen(const Cairo::RefPtr<Cairo::Context>& cr);
    bool on_key_press(GdkEventKey* event);
    bool on_button_press(GdkEventButton* event);
    void load_level(int level_num);
    void save_level();
    void new_level();
    void next_level();
    void test_level();
    void change_background_color(int color_component);
    void update_status_bar();
    void calculate_scaling_factors();
    void on_window_resize();
    bool on_scroll_event(GdkEventScroll* event);
    std::pair<int, int> screen_to_grid(double screen_x, double screen_y);
    void place_sprite(int row, int col, const std::string& sprite_name);
    void remove_sprite(int row, int col);
    void ensure_level_exists();
    void quit_editor();
    
    // Editor states
    enum class EditorState {
        INTRO,
        EDITING
    } editor_state;
};

WillyEditor::WillyEditor() : 
    vbox(Gtk::ORIENTATION_VERTICAL),
    editor_state(EditorState::INTRO),
    scale_factor(editor_options.scale_factor),
    current_level_num(1),
    current_level("level1") {
    
    set_title("Willy the Worm Level Editor - C++ GTK Edition");
    set_decorated(true);
    set_skip_taskbar_hint(false);
    set_skip_pager_hint(false);
    property_window_position().set_value(Gtk::WIN_POS_CENTER);
    
    sprite_loader = std::make_unique<SpriteLoader>(scale_factor);
    level_loader = std::make_unique<LevelLoader>();
    
    // Load levels file
    if (!level_loader->load_levels(editor_options.levels_file)) {
        std::cout << "Warning: Failed to load " << editor_options.levels_file 
                  << ", creating new levels file" << std::endl;
        // Don't try to load default - just start with empty levels
        max_levels = 0;
    } else {
        max_levels = level_loader->get_max_levels();
    }
    
    setup_ui();
    
    // Store base game dimensions
    base_game_width = GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor;
    base_game_height = (GAME_SCREEN_HEIGHT + 2) * GAME_CHAR_HEIGHT * scale_factor;
    
    // Calculate proper window size
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    
    Gtk::Requisition statusbar_min, statusbar_nat;
    status_bar.get_preferred_size(statusbar_min, statusbar_nat);
    
    int total_height = base_game_height + menubar_min.height + statusbar_min.height + 10;
    
    set_default_size(base_game_width, total_height);
    resize(base_game_width, total_height);
    
    // Connect resize signal
    signal_size_allocate().connect(sigc::hide(sigc::mem_fun(*this, &WillyEditor::on_window_resize)));
    
    // Initial scaling calculation
    calculate_scaling_factors();
    
    drawing_area.grab_focus();
    
    show_all_children();
    
    std::cout << "Editor initialized with:" << std::endl;
    std::cout << "  Levels file: " << editor_options.levels_file << std::endl;
    std::cout << "  Scale factor: " << scale_factor << std::endl;
    std::cout << "  Max levels: " << max_levels << std::endl;
}

WillyEditor::~WillyEditor() {
}

void WillyEditor::setup_ui() {
    add(vbox);
    
    create_menubar();
    vbox.pack_start(menubar, Gtk::PACK_SHRINK);
    
    // Allow the drawing area to expand and fill available space
    drawing_area.set_hexpand(true);
    drawing_area.set_vexpand(true);
    
    drawing_area.signal_draw().connect(
        sigc::mem_fun(*this, &WillyEditor::on_draw));
    
    drawing_area.set_can_focus(true);
    
    // Enable keyboard events
    drawing_area.add_events(Gdk::KEY_PRESS_MASK);
    drawing_area.signal_key_press_event().connect(
        sigc::mem_fun(*this, &WillyEditor::on_key_press));
    
    // Enable mouse events including scroll wheel
    drawing_area.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK);
    drawing_area.signal_button_press_event().connect(
        sigc::mem_fun(*this, &WillyEditor::on_button_press));
    drawing_area.signal_scroll_event().connect(
        sigc::mem_fun(*this, &WillyEditor::on_scroll_event));
    
    vbox.pack_start(drawing_area, Gtk::PACK_EXPAND_WIDGET);
    
    update_status_bar();
    vbox.pack_start(status_bar, Gtk::PACK_SHRINK);
}

void WillyEditor::create_menubar() {
    // File menu
    auto file_menu = Gtk::manage(new Gtk::Menu());
    
    auto new_level_item = Gtk::manage(new Gtk::MenuItem("_New Level", true));
    new_level_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyEditor::new_level));
    file_menu->append(*new_level_item);
    
    auto save_item = Gtk::manage(new Gtk::MenuItem("_Save Level", true));
    save_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyEditor::save_level));
    file_menu->append(*save_item);
    
    file_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
    
    auto quit_item = Gtk::manage(new Gtk::MenuItem("_Quit", true));
    quit_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyEditor::quit_editor));
    file_menu->append(*quit_item);
    
    auto file_item = Gtk::manage(new Gtk::MenuItem("_File", true));
    file_item->set_submenu(*file_menu);
    menubar.append(*file_item);
    
    // Level menu
    auto level_menu = Gtk::manage(new Gtk::Menu());
    
    auto next_level_item = Gtk::manage(new Gtk::MenuItem("_Next Level", true));
    next_level_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyEditor::next_level));
    level_menu->append(*next_level_item);
    
    auto test_level_item = Gtk::manage(new Gtk::MenuItem("_Test Level", true));
    test_level_item->signal_activate().connect(
        sigc::mem_fun(*this, &WillyEditor::test_level));
    level_menu->append(*test_level_item);
    
    auto level_item = Gtk::manage(new Gtk::MenuItem("_Level", true));
    level_item->set_submenu(*level_menu);
    menubar.append(*level_item);
    
    // Help menu
    auto help_menu = Gtk::manage(new Gtk::Menu());
    
    auto about_item = Gtk::manage(new Gtk::MenuItem("_About", true));
    about_item->signal_activate().connect([this]() {
        editor_state = EditorState::INTRO;
        drawing_area.queue_draw();
    });
    help_menu->append(*about_item);
    
    auto help_item = Gtk::manage(new Gtk::MenuItem("_Help", true));
    help_item->set_submenu(*help_menu);
    menubar.append(*help_item);
}

bool WillyEditor::on_key_press(GdkEventKey* event) {
    std::string keyname = gdk_keyval_name(event->keyval);
    
    if(keyname == "Escape") {
        quit_editor();
    } else if(keyname == "F11") {
        if(get_window()->get_state() & GDK_WINDOW_STATE_FULLSCREEN) {
            unfullscreen();
        } else {
            fullscreen();
        }
    } else if(editor_state == EditorState::INTRO) {
        if(keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
            std::cout << "Starting editor..." << std::endl;
            editor_state = EditorState::EDITING;
            load_level(current_level_num);
            drawing_area.queue_draw();
        }
    } else if(editor_state == EditorState::EDITING) {
        if(keyname == "s" || keyname == "S") {
            save_level();
        } else if(keyname == "l" || keyname == "L") {
            next_level();
        } else if(keyname == "n" || keyname == "N") {
            new_level();
        } else if(keyname == "p" || keyname == "P") {
            test_level();
        } else if(keyname == "q" || keyname == "Q") {
            quit_editor();
        } else if(keyname == "F5") {
            change_background_color(0); // Red
        } else if(keyname == "F6") {
            change_background_color(1); // Green
        } else if(keyname == "F7") {
            change_background_color(2); // Blue
        } else if(keyname == "F8") {
            // Reset to blue background
            redbg = 0.0;
            greenbg = 0.0;
            bluebg = 1.0;
            sprite_loader = std::make_unique<SpriteLoader>(scale_factor);
        }
    }
    
    drawing_area.queue_draw();
    return true;
}

bool WillyEditor::on_scroll_event(GdkEventScroll* event) {
    if(editor_state != EditorState::EDITING) {
        return false;
    }
    
    if(event->direction == GDK_SCROLL_UP) {
        sprite_iterator.previous();
        std::cout << "Scroll up - previous sprite: " << sprite_iterator.current() << std::endl;
    } else if(event->direction == GDK_SCROLL_DOWN) {
        sprite_iterator.next();
        std::cout << "Scroll down - next sprite: " << sprite_iterator.current() << std::endl;
    }
    
    // Update the preview sprite in top-right corner (column 40)
    place_sprite(0, 40, sprite_iterator.current());
    update_status_bar();
    drawing_area.queue_draw();
    
    return true;
}

bool WillyEditor::on_button_press(GdkEventButton* event) {
    if(editor_state != EditorState::EDITING) {
        return false;
    }
    
    // Get mouse coordinates and convert to grid
    auto grid_pos = screen_to_grid(event->x, event->y);
    int row = grid_pos.first;
    int col = grid_pos.second;
    
    if(row < 0 || row >= GAME_MAX_HEIGHT || col < 0 || col >= GAME_MAX_WIDTH) {
        return false;
    }
    
    if(event->button == 1) { // Left click - place sprite
        std::string current_sprite = sprite_iterator.current();
        place_sprite(row, col, current_sprite);
        
        // Special handling for Willy sprites - remove other Willies
        if(current_sprite == "WILLY_RIGHT" || current_sprite == "WILLY_LEFT") {
            for(int r = 0; r < GAME_MAX_HEIGHT; r++) {
                for(int c = 0; c < GAME_MAX_WIDTH; c++) {
                    if((r != row || c != col)) {
                        std::string tile = level_loader->get_tile(current_level, r, c);
                        if(tile == "WILLY_RIGHT" || tile == "WILLY_LEFT") {
                            level_loader->set_tile(current_level, r, c, "EMPTY");
                        }
                    }
                }
            }
        }
        
        // Special handling for BALLPIT - set as primary ball pit
        if(current_sprite == "BALLPIT") {
            // This would need to be implemented in LevelLoader
            std::cout << "Set primary ball pit at (" << row << ", " << col << ")" << std::endl;
        }
        
    } else if(event->button == 3) { // Right click - remove sprite
        remove_sprite(row, col);
    } else if(event->button == 4) { // Scroll up - previous sprite
        sprite_iterator.previous();
        // Update the preview sprite in top-right corner
        place_sprite(0, GAME_MAX_WIDTH - 1, sprite_iterator.current());
    } else if(event->button == 5) { // Scroll down - next sprite
        sprite_iterator.next();
        // Update the preview sprite in top-right corner
        place_sprite(0, GAME_MAX_WIDTH - 1, sprite_iterator.current());
    }
    
    drawing_area.queue_draw();
    return true;
}

std::pair<int, int> WillyEditor::screen_to_grid(double screen_x, double screen_y) {
    // Get menubar height to account for offset
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    int menubar_height = menubar_min.height;
    
    // Adjust for menubar offset
    screen_y -= menubar_height;
    
    // Account for scaling
    screen_x /= current_scale_x;
    screen_y /= current_scale_y;
    
    // Convert to grid coordinates
    int scaled_char_width = GAME_CHAR_WIDTH * scale_factor;
    int scaled_char_height = GAME_CHAR_HEIGHT * scale_factor;
    
    int col = (int)(screen_x / scaled_char_width);
    int row = (int)(screen_y / scaled_char_height);
    
    return {row, col};
}

void WillyEditor::place_sprite(int row, int col, const std::string& sprite_name) {
    level_loader->set_tile(current_level, row, col, sprite_name);
}

void WillyEditor::remove_sprite(int row, int col) {
    level_loader->set_tile(current_level, row, col, "EMPTY");
}

void WillyEditor::load_level(int level_num) {
    current_level_num = level_num;
    current_level = "level" + std::to_string(level_num);
    
    std::cout << "Loading level: " << current_level << std::endl;
    
    if(!level_loader->level_exists(current_level)) {
        // Create new level
        std::cout << "Level doesn't exist, creating new one..." << std::endl;
        ensure_level_exists();
    }
    
    // Set up the preview sprite in top-right corner (column 40, outside the game area)
    place_sprite(0, 40, sprite_iterator.current());
    
    update_status_bar();
    std::cout << "Loaded level: " << current_level << std::endl;
}

void WillyEditor::ensure_level_exists() {
    // Initialize empty level if it doesn't exist
    if (!level_loader->level_exists(current_level)) {
        std::cout << "Creating new level: " << current_level << std::endl;
        // Create level by setting at least one tile - we'll set all to EMPTY initially
        for(int row = 0; row < GAME_MAX_HEIGHT; row++) {
            for(int col = 0; col < GAME_MAX_WIDTH; col++) {
                level_loader->set_tile(current_level, row, col, "EMPTY");
            }
        }
        std::cout << "Created new empty level: " << current_level << std::endl;
    } else {
        std::cout << "Level " << current_level << " already exists" << std::endl;
    }
}

void WillyEditor::save_level() {
    // Remove the preview sprite before saving (column 40)
    if (level_loader->get_tile(current_level, 0, 40) != "EMPTY") {
        level_loader->set_tile(current_level, 0, 40, "EMPTY");
    }
    
    if(level_loader->save_levels(editor_options.levels_file)) {
        std::cout << "Level saved successfully to " << editor_options.levels_file << std::endl;
    } else {
        std::cout << "Failed to save level!" << std::endl;
    }
    
    // Restore the preview sprite (column 40)
    place_sprite(0, 40, sprite_iterator.current());
    
    update_status_bar();
}

void WillyEditor::new_level() {
    max_levels++;
    current_level_num = max_levels;
    current_level = "level" + std::to_string(current_level_num);
    
    // Initialize new empty level
    ensure_level_exists();
    
    // Set up the preview sprite in top-right corner (column 40)
    place_sprite(0, 40, sprite_iterator.current());
    
    update_status_bar();
    drawing_area.queue_draw();
    
    std::cout << "Created new level: " << current_level << std::endl;
}

void WillyEditor::next_level() {
    current_level_num++;
    if(current_level_num > max_levels) {
        current_level_num = 1;
    }
    
    // Handle case where no levels exist yet
    if(max_levels == 0) {
        max_levels = 1;
        current_level_num = 1;
    }
    
    load_level(current_level_num);
    drawing_area.queue_draw();
}

void WillyEditor::test_level() {
    std::cout << "Test level functionality would launch the game here" << std::endl;
    std::cout << "Command would be: ./willy -l " << current_level_num 
              << " -L " << editor_options.levels_file << std::endl;
    
    // In a full implementation, you could fork and exec the game process
    // or create a separate test window
}

void WillyEditor::change_background_color(int color_component) {
    double* color_ptr = nullptr;
    std::string color_name;
    
    switch(color_component) {
        case 0: color_ptr = &redbg; color_name = "Red"; break;
        case 1: color_ptr = &greenbg; color_name = "Green"; break;
        case 2: color_ptr = &bluebg; color_name = "Blue"; break;
        default: return;
    }
    
    if(*color_ptr >= 1.0) {
        *color_ptr = 0.0;
    } else if(*color_ptr >= 0.75) {
        *color_ptr = 1.0;
    } else if(*color_ptr >= 0.5) {
        *color_ptr = 0.75;
    } else if(*color_ptr >= 0.25) {
        *color_ptr = 0.5;
    } else {
        *color_ptr = 0.25;
    }
    
    // Recreate sprite loader with new background colors
    sprite_loader = std::make_unique<SpriteLoader>(scale_factor);
    
    std::cout << color_name << " component set to: " << *color_ptr << std::endl;
}

void WillyEditor::update_status_bar() {
    std::string status_text = "Level: " + std::to_string(current_level_num) + 
                            "    Current item: " + sprite_iterator.current() +
                            "    File: " + editor_options.levels_file;
    status_bar.set_text(status_text);
}

void WillyEditor::calculate_scaling_factors() {
    // Get current window size
    int window_width, window_height;
    get_size(window_width, window_height);
    
    // Get menubar and status bar heights
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    
    Gtk::Requisition statusbar_min, statusbar_nat;
    status_bar.get_preferred_size(statusbar_min, statusbar_nat);
    
    // Calculate available space for the game area
    int available_width = window_width;
    int available_height = window_height - menubar_min.height - statusbar_min.height;
    
    // Calculate scale
    double scale_x = (double)available_width / base_game_width;
    double scale_y = (double)available_height / base_game_height;
    
    // Use the smaller scale to maintain aspect ratio
    double scale = std::min(scale_x, scale_y);
    
    // Round to nearest 0.1
    current_scale_x = std::round(scale * 10.0) / 10.0;
    current_scale_y = current_scale_x; // Maintain aspect ratio
    
    // Don't scale below 0.1 or above 10.0
    current_scale_x = std::max(0.1, std::min(10.0, current_scale_x));
    current_scale_y = std::max(0.1, std::min(10.0, current_scale_y));
}

void WillyEditor::on_window_resize() {
    calculate_scaling_factors();
    drawing_area.queue_draw();
}

bool WillyEditor::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    if(editor_state == EditorState::INTRO) {
        // Paint blue background for entire area first
        cr->set_source_rgb(redbg, greenbg, bluebg);
        cr->paint();
        draw_intro_screen(cr);
    } else if(editor_state == EditorState::EDITING) {
        draw_editor_screen(cr);
    }
    
    return true;
}

void WillyEditor::draw_intro_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Get menubar height to use as offset
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    int menubar_height = menubar_min.height;
    
    // Get current drawing area size
    Gtk::Allocation allocation = drawing_area.get_allocation();
    int area_width = allocation.get_width();
    int area_height = allocation.get_height() - menubar_height;
    
    cr->save();
    cr->translate(0, menubar_height);
    
    // Text data for intro screen
    std::vector<std::vector<std::string>> textdata = {
        {""},
        {""},
        {"WILLY_LEFT", " Willy the Worm ", "WILLY_RIGHT", " Level Editor"},
        {""},
        {"By Jason Hall"},
        {"(Based on original version by Alan Farmer 1985)"},
        {""},
        {"This code is Free Open Source Software (FOSS)"},
        {"Please feel free to do with it whatever you wish."},
        {""},
        {"If you do make changes though such as new levels,"},
        {"please share them with the world."},
        {""},
        {""},
        {"Welcome to the Willy the Worm ", "WILLY_RIGHT", " Editor."},
        {""},
        {"Left click places items, ", "PRESENT", " right click removes items"},
        {"Mouse wheel scrolls between items."},
        {"F11 toggles full screen ", "BELL"},
        {"The 'S' Key saves the Level"},
        {"The 'L' Key Changes Level; The 'N' Key creates a new empty level"},
        {"The 'Q' Key or ESC exits the editor (without saving)"},
        {"The 'P' Key Tests the Level"},
        {""},
        {"You can also specify the level file at the command line"},
        {""},
        {"The LAST BallPit ", "BALLPIT", " placed is where the balls come out."},
        {""},
        {"Good luck and have fun building levels!!!"},
        {""},
        {"Press Enter to Continue"}
    };
    
    // Calculate appropriate sizing
    int num_lines = textdata.size();
    int available_height = area_height - 40;
    int line_height = available_height / num_lines;
    int base_font_size = std::max(10, std::min(20, line_height - 2));
    
    // Set up font
    Pango::FontDescription font_desc;
    font_desc.set_family("Courier");
    font_desc.set_size(base_font_size * PANGO_SCALE);
    
    auto layout = Pango::Layout::create(cr);
    layout->set_font_description(font_desc);
    
    int sprite_size = base_font_size;
    
    for(size_t line_idx = 0; line_idx < textdata.size(); line_idx++) {
        const auto& line = textdata[line_idx];
        
        if(line.empty() || (line.size() == 1 && line[0].empty())) {
            continue;
        }
        
        int y_pos = 20 + line_idx * line_height;
        
        // Calculate line width for centering
        int line_width = 0;
        for(const auto& element : line) {
            if(element.empty()) continue;
            
            if(sprite_loader->get_sprite(element)) {
                line_width += sprite_size;
            } else {
                layout->set_text(element);
                int text_w, text_h;
                layout->get_pixel_size(text_w, text_h);
                line_width += text_w;
            }
        }
        
        // Center the line
        int start_x = (area_width - line_width) / 2;
        int current_x = start_x;
        
        // Draw elements
        for(const auto& element : line) {
            if(element.empty()) continue;
            
            auto sprite = sprite_loader->get_sprite(element);
            if(sprite) {
                cr->save();
                cr->translate(current_x, y_pos);
                
                double sprite_scale = (double)sprite_size / (GAME_CHAR_WIDTH * scale_factor);
                cr->scale(sprite_scale, sprite_scale);
                
                cr->set_source(sprite, 0, 0);
                cr->paint();
                cr->restore();
                
                current_x += sprite_size;
            } else {
                layout->set_text(element);
                int text_w, text_h;
                layout->get_pixel_size(text_w, text_h);
                
                cr->set_source_rgb(1.0, 1.0, 1.0);
                cr->move_to(current_x, y_pos);
                layout->show_in_cairo_context(cr);
                
                current_x += text_w;
            }
        }
    }
    
    cr->restore();
}

void WillyEditor::draw_editor_screen(const Cairo::RefPtr<Cairo::Context>& cr) {
    // Get menubar height to use as offset
    Gtk::Requisition menubar_min, menubar_nat;
    menubar.get_preferred_size(menubar_min, menubar_nat);
    int menubar_height = menubar_min.height;
    
    // Apply scaling transformation
    cr->save();
    cr->translate(0, menubar_height);
    cr->scale(current_scale_x, current_scale_y);
    
    // Calculate the scaled character dimensions
    int scaled_char_width = GAME_CHAR_WIDTH * scale_factor;
    int scaled_char_height = GAME_CHAR_HEIGHT * scale_factor;
    
    // Draw background for game area (40 columns) plus preview column (column 40)
    for(int row = 0; row < GAME_MAX_HEIGHT; row++) {
        for(int col = 0; col <= 40; col++) { // Include column 40 for preview
            int x = col * scaled_char_width;
            int y = row * scaled_char_height;
            
            // Paint background
            cr->set_source_rgb(redbg, greenbg, bluebg);
            cr->rectangle(x, y, scaled_char_width, scaled_char_height);
            cr->fill();
            
            // Get the tile at this position
            std::string tile;
            if(col < GAME_MAX_WIDTH) {
                tile = level_loader->get_tile(current_level, row, col);
            } else if(col == 40 && row == 0) {
                // Preview sprite in column 40, row 0
                tile = sprite_iterator.current();
            } else {
                tile = "EMPTY";
            }
            
            // Draw sprite if not empty
            if(tile != "EMPTY") {
                auto sprite = sprite_loader->get_sprite(tile);
                if(sprite) {
                    cr->set_source(sprite, x, y);
                    cr->paint();
                }
            }
        }
    }
    
    cr->restore();
}

void WillyEditor::quit_editor() {
    hide();
}

// Application class for the editor
class WillyEditorApplication : public Gtk::Application {
protected:
    WillyEditorApplication();
    
public:
    static Glib::RefPtr<WillyEditorApplication> create();

protected:
    void on_activate() override;
};

WillyEditorApplication::WillyEditorApplication() : Gtk::Application("org.example.willyeditor") {}

Glib::RefPtr<WillyEditorApplication> WillyEditorApplication::create() {
    return Glib::RefPtr<WillyEditorApplication>(new WillyEditorApplication());
}

void WillyEditorApplication::on_activate() {
    auto window = new WillyEditor();
    add_window(*window);
    window->present();
}

void print_editor_help(const char* program_name) {
    std::cout << "Willy the Worm Level Editor - C++ GTK Edition\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -L LEVELFILE      Use custom levels file (default: levels.json)\n";
    std::cout << "  -S SCALE          Set scale factor (default: 3)\n";
    std::cout << "  -h, --help        Show this help message\n\n";
    std::cout << "Editor Controls:\n";
    std::cout << "  Left Click        Place current item\n";
    std::cout << "  Right Click       Remove item\n";
    std::cout << "  Mouse Wheel       Cycle through available items\n";
    std::cout << "  S                 Save level\n";
    std::cout << "  L                 Next level\n";
    std::cout << "  N                 Create new level\n";
    std::cout << "  P                 Test level (shows command to run)\n";
    std::cout << "  Q or Escape       Quit editor\n";
    std::cout << "  F5/F6/F7          Change background colors\n";
    std::cout << "  F8                Reset to blue background\n";
    std::cout << "  F11               Toggle fullscreen\n\n";
    std::cout << "Notes:\n";
    std::cout << "  - Only one Willy can exist per level (placing a new one removes others)\n";
    std::cout << "  - The last BALLPIT placed becomes the primary ball spawn point\n";
    std::cout << "  - Current item is shown in the top-right corner\n";
    std::cout << "  - Levels are auto-created when you place items\n\n";
}

bool parse_editor_command_line(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"levels-file", required_argument, nullptr, 'L'},
        {"scale", required_argument, nullptr, 'S'},
        {nullptr, 0, nullptr, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "hL:S:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                editor_options.show_help = true;
                return true;

            case 'L':
                editor_options.levels_file = optarg;
                break;

            case 'S': {
                try {
                    int value = std::stoi(optarg);
                    if (value < 1 || value > 10) {
                        std::cerr << "Error: Scale factor must be between 1 and 10\n";
                        return false;
                    }
                    editor_options.scale_factor = value;
                } catch (const std::exception&) {
                    std::cerr << "Error: Invalid value for scale factor: " << optarg << "\n";
                    return false;
                }
                break;
            }

            case '?':
                return false;

            default:
                std::cerr << "Error: Unknown option encountered.\n";
                return false;
        }
    }

    // Check for unexpected arguments
    if (optind < argc) {
        std::cerr << "Error: Unexpected argument: " << argv[optind] << "\n";
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments BEFORE creating GTK app
    if (!parse_editor_command_line(argc, argv)) {
        print_editor_help(argv[0]);
        return 1;
    }
    
    if (editor_options.show_help) {
        print_editor_help(argv[0]);
        return 0;
    }
    
    // Print startup information
    std::cout << "Willy the Worm Level Editor - C++ GTK Edition\n";
    std::cout << "Levels file: " << editor_options.levels_file << "\n";
    std::cout << "Scale factor: " << editor_options.scale_factor << "\n";
    std::cout << "\n";

    // Create a new argc/argv with only the program name for GTK
    int gtk_argc = 1;
    char* gtk_argv[] = {argv[0], nullptr};

    auto app = WillyEditorApplication::create();
    return app->run(gtk_argc, gtk_argv);
}
