#include "willy.h"

extern GameOptions game_options;

// Control panel window class
class ControlPanelWindow : public Gtk::Window {
private:
    WillyGame* game_window;
    
    Gtk::Box main_box;
    
    Gtk::Button new_game_btn;
    Gtk::Button quit_btn;
    
    Gtk::Label lives_label;
    Gtk::Scale lives_scale;
    Gtk::Label balls_label;
    Gtk::Scale balls_scale;
    Gtk::Label fps_label;
    Gtk::Scale fps_scale;
    Gtk::Label level_label;
    Gtk::Scale level_scale;
    
public:
    ControlPanelWindow(WillyGame* game) 
        : game_window(game),
          main_box(Gtk::ORIENTATION_VERTICAL, 5),
          new_game_btn("New Game"),
          quit_btn("Quit"),
          lives_label("Lives:"),
          balls_label("Balls:"),
          fps_label("Game Speed:"),
          level_label("Starting Level:") {
        
        set_title("Willy the Worm - Control Panel");
        set_default_size(180, 300);
        set_resizable(false);
        set_type_hint(Gdk::WINDOW_TYPE_HINT_UTILITY);
        
        setup_ui();
        apply_styling();
        show_all_children();
    }
    
    void setup_ui() {
        add(main_box);
        main_box.set_margin_left(10);
        main_box.set_margin_right(10);
        main_box.set_margin_top(10);
        main_box.set_margin_bottom(10);
        
        // Lives setting
        lives_label.set_halign(Gtk::ALIGN_START);
        main_box.pack_start(lives_label, Gtk::PACK_SHRINK);
        
        lives_scale.set_range(1, 10);
        lives_scale.set_value(game_options.starting_lives);
        lives_scale.set_digits(0);
        lives_scale.set_hexpand(true);
        main_box.pack_start(lives_scale, Gtk::PACK_SHRINK);
        
        // Number of balls setting
        balls_label.set_halign(Gtk::ALIGN_START);
        main_box.pack_start(balls_label, Gtk::PACK_SHRINK);
        
        balls_scale.set_range(0, 100);
        balls_scale.set_value(game_options.number_of_balls);
        balls_scale.set_digits(0);
        balls_scale.set_hexpand(true);
        main_box.pack_start(balls_scale, Gtk::PACK_SHRINK);
        
        // FPS setting
        fps_label.set_halign(Gtk::ALIGN_START);
        main_box.pack_start(fps_label, Gtk::PACK_SHRINK);
        
        fps_scale.set_range(5, 30);
        fps_scale.set_value(game_options.fps);
        fps_scale.set_digits(0);
        fps_scale.set_hexpand(true);
        main_box.pack_start(fps_scale, Gtk::PACK_SHRINK);
        
        // Starting level setting
        level_label.set_halign(Gtk::ALIGN_START);
        main_box.pack_start(level_label, Gtk::PACK_SHRINK);
        
        level_scale.set_range(1, 100);
        level_scale.set_value(game_options.starting_level);
        level_scale.set_digits(0);
        level_scale.set_hexpand(true);
        main_box.pack_start(level_scale, Gtk::PACK_SHRINK);
        
        // Buttons
        new_game_btn.set_size_request(-1, 35);
        quit_btn.set_size_request(-1, 35);
        
        main_box.pack_start(new_game_btn, Gtk::PACK_SHRINK);
        main_box.pack_start(quit_btn, Gtk::PACK_SHRINK);
        
        // Connect signals
        lives_scale.signal_value_changed().connect([this]() {
            game_options.starting_lives = (int)lives_scale.get_value();
        });
        
        balls_scale.signal_value_changed().connect([this]() {
            game_options.number_of_balls = (int)balls_scale.get_value();
        });
        
        fps_scale.signal_value_changed().connect([this]() {
            game_options.fps = (int)fps_scale.get_value();
        });
        
        level_scale.signal_value_changed().connect([this]() {
            game_options.starting_level = (int)level_scale.get_value();
        });
        
        new_game_btn.signal_clicked().connect(
            sigc::mem_fun(*game_window, &WillyGame::new_game));
        
        quit_btn.signal_clicked().connect(
            sigc::mem_fun(*game_window, &WillyGame::quit_game));
    }
    
    void apply_styling() {
        auto css_provider = Gtk::CssProvider::create();
        css_provider->load_from_data(R"(
            window {
                background-color: #f0f0f0;
            }
            
            frame {
                border: 1px solid #d0d0d0;
                border-radius: 5px;
                margin: 3px;
            }
            
            frame > border {
                background-color: #ffffff;
                border-radius: 5px;
            }
            
            frame > label {
                font-weight: bold;
                color: #000000;
                padding: 0 8px;
                background-color: #f0f0f0;
            }
            
            button {
                border-radius: 3px;
                padding: 8px;
                background-color: #ffffff;
                color: #000000;
                border: 1px solid #d0d0d0;
            }
            
            button:hover {
                background-color: #0078d4;
                color: #ffffff;
            }
        )");
        
        auto style_context = get_style_context();
        style_context->add_provider_for_screen(
            Gdk::Screen::get_default(),
            css_provider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }
    
    bool on_delete_event(GdkEventAny* /*event*/) override {
        hide();
        return true;
    }
};

// Declare control_panel at file scope after the class definition
static ControlPanelWindow* control_panel = nullptr;

void WillyGame::create_menubar() {
    // Instead of creating a menubar, create a control panel window
    if (!control_panel) {
        control_panel = new ControlPanelWindow(this);
        
        // Position it to the left of the main window
        int main_x, main_y;
        get_position(main_x, main_y);
        control_panel->move(0, 0);
        
        // Don't show it initially - let user open it with F1
        // control_panel->show();
    }
}

void WillyGame::show_control_panel() {
    if (!control_panel) {
        // Create it if it doesn't exist
        create_menubar();
    }
    
    if (control_panel->is_visible()) {
        // If visible, hide it
        control_panel->hide();
        std::cout << "Control panel hidden (press F1 to show)" << std::endl;
    } else {
        // If hidden, show it
        control_panel->show();
        control_panel->present(); // Bring to front
        std::cout << "Control panel shown (press F1 to hide)" << std::endl;
    }
}

void WillyGame::setup_ui() {
    add(vbox);
    create_menubar(); // This now creates the control panel instead
    
    // Don't pack the menubar since we're using a separate window
    // vbox.pack_start(menubar, Gtk::PACK_SHRINK);

    // Allow the drawing area to expand and fill available space
    drawing_area.set_hexpand(true);
    drawing_area.set_vexpand(true);

    drawing_area.signal_draw().connect(sigc::mem_fun(*this, &WillyGame::on_draw));

    drawing_area.set_can_focus(true);

    // Existing keyboard events
    drawing_area.add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    drawing_area.signal_key_press_event().connect(
        sigc::mem_fun(*this, &WillyGame::on_key_press));
    drawing_area.signal_key_release_event().connect(
        sigc::mem_fun(*this, &WillyGame::on_key_release));

    if (game_options.mouse_support) {
        // Enable all mouse button events including press and release
        drawing_area.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK |
                                Gdk::BUTTON1_MOTION_MASK |
                                Gdk::BUTTON2_MOTION_MASK |
                                Gdk::BUTTON3_MOTION_MASK);
        drawing_area.signal_button_press_event().connect(
            sigc::mem_fun(*this, &WillyGame::on_button_press));
        drawing_area.signal_button_release_event().connect(
            sigc::mem_fun(*this, &WillyGame::on_button_release));
        std::cout << "Mouse support enabled - hold mouse button to keep moving, "
                     "middle-click to stop"
                  << std::endl;
    }

    // Pack with expansion so it fills available space
    vbox.pack_start(drawing_area, Gtk::PACK_EXPAND_WIDGET);

    update_status_bar();
    vbox.pack_start(status_bar, Gtk::PACK_SHRINK);
}
