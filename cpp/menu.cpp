#include "willy.h"

void WillyGame::create_menubar() {
  // Set menubar background to proper gray theme
  auto css_provider = Gtk::CssProvider::create();
  css_provider->load_from_data(R"(
        menubar {
            background-color: #f0f0f0;
            color: #000000;
            border-bottom: 1px solid #d0d0d0;
        }
        menubar > menuitem {
            background-color: #f0f0f0;
            color: #000000;
            padding: 4px 8px;
        }
        menubar > menuitem:hover {
            background-color: #e0e0e0;
            color: #000000;
        }
        menubar menu {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #d0d0d0;
        }
        menubar menu menuitem {
            background-color: #ffffff;
            color: #000000;
            padding: 4px 12px;
        }
        menubar menu menuitem:hover {
            background-color: #0078d4;
            color: #ffffff;
        }
        menubar menu separator {
            background-color: #d0d0d0;
            min-height: 1px;
        }
    )");

  auto style_context = menubar.get_style_context();
  style_context->add_provider(css_provider,
                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  // File menu
  auto file_menu = Gtk::manage(new Gtk::Menu());

  auto new_game_item = Gtk::manage(new Gtk::MenuItem("_New Game", true));
  new_game_item->signal_activate().connect(
      sigc::mem_fun(*this, &WillyGame::new_game));
  file_menu->append(*new_game_item);

  file_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));

  auto quit_item = Gtk::manage(new Gtk::MenuItem("_Quit", true));
  quit_item->signal_activate().connect(
      sigc::mem_fun(*this, &WillyGame::quit_game));
  file_menu->append(*quit_item);

  auto file_item = Gtk::manage(new Gtk::MenuItem("_File", true));
  file_item->set_submenu(*file_menu);
  menubar.append(*file_item);

  // Game menu
  auto game_menu = Gtk::manage(new Gtk::Menu());

  auto pause_item = Gtk::manage(new Gtk::MenuItem("_Pause", true));
  pause_item->signal_activate().connect([this]() {
    if (current_state == GameState::PLAYING) {
      current_state = GameState::PAUSED;
    } else if (current_state == GameState::PAUSED) {
      current_state = GameState::PLAYING;
    }
  });
  game_menu->append(*pause_item);

  auto reset_item = Gtk::manage(new Gtk::MenuItem("_Reset Level", true));
  reset_item->signal_activate().connect([this]() {
    if (current_state == GameState::PLAYING) {
      reset_level();
    }
  });
  game_menu->append(*reset_item);

  auto game_item = Gtk::manage(new Gtk::MenuItem("_Game", true));
  game_item->set_submenu(*game_menu);
  menubar.append(*game_item);

  // Help menu
  auto help_menu = Gtk::manage(new Gtk::Menu());

  auto about_item = Gtk::manage(new Gtk::MenuItem("_About", true));
  about_item->signal_activate().connect(
      [this]() { current_state = GameState::INTRO; });
  help_menu->append(*about_item);

  auto help_item = Gtk::manage(new Gtk::MenuItem("_Help", true));
  help_item->set_submenu(*help_menu);
  menubar.append(*help_item);
}

