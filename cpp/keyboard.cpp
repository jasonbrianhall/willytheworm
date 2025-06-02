#include "willy.h"

extern GameOptions game_options;
extern double redbg;
extern double greenbg;
extern double bluebg;


bool WillyGame::on_key_press(GdkEventKey *event) {
  std::string keyname = gdk_keyval_name(event->keyval);
  // std::cout << "Key pressed: " << keyname << std::endl;
  keys_pressed.insert(keyname);

  // Check for modifier keys
  bool ctrl_pressed = (event->state & GDK_CONTROL_MASK);
  bool shift_pressed = (event->state & GDK_SHIFT_MASK);
  bool alt_pressed = (event->state & GDK_MOD1_MASK);

  if (keyname == "Escape") {
    quit_game();
  } else if (keyname == "F11") {
    if (get_window()->get_state() & GDK_WINDOW_STATE_FULLSCREEN) {
      unfullscreen();
    } else {
      fullscreen();
    }
  } else if (current_state == GameState::INTRO) {
    if (keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
      std::cout << "Starting game..." << std::endl;
      start_game();
    }
  } else if (current_state == GameState::PLAYING) {
    if (keyname == "space") {
      jump();
    } else if (keyname == "Left" || (game_options.use_wasd && keyname == "a")) {
      continuous_direction = "LEFT";
      moving_continuously = true;
      willy_direction = "LEFT";
    } else if (keyname == "Right" ||
               (game_options.use_wasd && keyname == "d")) {
      continuous_direction = "RIGHT";
      moving_continuously = true;
      willy_direction = "RIGHT";
    } else if (keyname == "Up" || (game_options.use_wasd && keyname == "w")) {
      up_pressed = true;
    } else if (keyname == "Down" || (game_options.use_wasd && keyname == "s")) {
      down_pressed = true;
    } else if ((keyname == "L" || keyname == "l") && ctrl_pressed) {
      // Level skip with Ctrl+L (matching Python version)
      complete_level_nobonus();
    } else if ((keyname == "S" || keyname == "s") && ctrl_pressed) {
      // Sound toggle with Ctrl+S
      bool current_sound_state = sound_manager->is_sound_enabled();
      sound_manager->set_sound_enabled(!current_sound_state);

      // Visual feedback for sound toggle
      std::string sound_status = current_sound_state ? "OFF" : "ON";
      std::cout << "Sound toggled " << sound_status << std::endl;

      // Optional: Play a test sound when enabling
      if (!current_sound_state) {
        sound_manager->play_sound("bell.mp3");
      }
    } else if (keyname == "F1") {
       // Show/hide the control panel
       show_control_panel();
    } else if (keyname == "F5") {
      redbg += 0.25;
      if (redbg > 1.0) {
        redbg = 0.0;
      }
    } else if (keyname == "F6") {
      greenbg += 0.25;
      if (greenbg > 1.0) {
        greenbg = 0.0;
      }
    } else if (keyname == "F7") {
      bluebg += 0.25;
      if (bluebg > 1.0) {
        bluebg = 0.0;
      }
    } else {
      moving_continuously = false;
      continuous_direction = "";
    }
  } else if (current_state == GameState::GAME_OVER) {
    if (keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
      current_state = GameState::INTRO;
    }
  } else if (current_state == GameState::HIGH_SCORE_ENTRY) {
    if (keyname == "Return" || keyname == "Enter" || keyname == "KP_Enter") {
      if (!name_input.empty()) {
        score_manager->add_score(name_input, score);
      }
      current_state = GameState::HIGH_SCORE_DISPLAY;
    } else if (keyname == "BackSpace") {
      if (!name_input.empty()) {
        name_input.pop_back();
      }
    } else if (keyname.length() == 1) {
      // Single character key
      if (name_input.length() < 20) { // Limit name length
        name_input += keyname;
      }
    }
  } else if (current_state == GameState::HIGH_SCORE_DISPLAY) {
    if (keyname == "Escape") {
      quit_game();
    } else {
      current_state = GameState::INTRO;
    }
  }

  return true;
}

bool WillyGame::on_key_release(GdkEventKey *event) {
  std::string keyname = gdk_keyval_name(event->keyval);
  keys_pressed.erase(keyname);

  if (keyname == "Up") {
    up_pressed = false;
  } else if (keyname == "Down") {
    down_pressed = false;
  }

  return true;
}

