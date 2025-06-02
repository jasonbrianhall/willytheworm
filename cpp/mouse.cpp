#include "willy.h"

extern GameOptions game_options;

bool WillyGame::on_button_press(GdkEventButton *event) {
  if (!game_options.mouse_support || current_state != GameState::PLAYING) {
    return false;
  }

  // Debug: Print which button was pressed
  std::cout << "Mouse button " << event->button << " pressed" << std::endl;

  // Get menubar height to account for offset
  Gtk::Requisition menubar_min, menubar_nat;
  menubar.get_preferred_size(menubar_min, menubar_nat);
  int menubar_height = menubar_min.height;

  // Convert mouse coordinates to game coordinates
  double mouse_x = event->x;
  double mouse_y = event->y - menubar_height;

  // Account for scaling
  mouse_x /= current_scale_x;
  mouse_y /= current_scale_y;

  // Convert to grid coordinates
  int scaled_char_width = GAME_CHAR_WIDTH * scale_factor;
  int scaled_char_height = GAME_CHAR_HEIGHT * scale_factor;

  int click_col = (int)(mouse_x / scaled_char_width);
  int click_row = (int)(mouse_y / scaled_char_height);

  // Get Willy's current position
  int willy_row = willy_position.first;
  int willy_col = willy_position.second;

  std::cout << "Mouse click at grid (" << click_row << ", " << click_col
            << "), Willy at (" << willy_row << ", " << willy_col << ")"
            << std::endl;

  if (event->button == 1) { // Left mouse button
    mouse_button_held = true;
    held_button = 1;

    // Determine direction based on where user clicked relative to Willy
    int row_diff = click_row - willy_row;
    int col_diff = click_col - willy_col;

    // Use the larger difference to determine primary direction
    if (abs(col_diff) > abs(row_diff)) {
      // Horizontal movement
      if (col_diff > 0) {
        // Clicked to the right of Willy
        mouse_direction = "RIGHT";
        continuous_direction = "RIGHT";
        moving_continuously = true;
        willy_direction = "RIGHT";
        std::cout << "Holding RIGHT" << std::endl;
      } else if (col_diff < 0) {
        // Clicked to the left of Willy
        mouse_direction = "LEFT";
        continuous_direction = "LEFT";
        moving_continuously = true;
        willy_direction = "LEFT";
        std::cout << "Holding LEFT" << std::endl;
      }
    } else {
      // Vertical movement
      if (row_diff < 0) {
        // Clicked above Willy
        mouse_direction = "UP";
        mouse_up_held = true;
        up_pressed = true;
        std::cout << "Holding UP" << std::endl;

      } else if (row_diff > 0) {
        // Clicked below Willy
        mouse_direction = "DOWN";
        mouse_down_held = true;
        down_pressed = true;
        std::cout << "Holding DOWN" << std::endl;
      }
    }

  } else if (event->button == 2) { // Middle mouse button - stop
    // Stop all movement
    continuous_direction = "";
    moving_continuously = false;
    up_pressed = false;
    down_pressed = false;
    mouse_up_held = false;
    mouse_down_held = false;
    mouse_button_held = false;
    std::cout << "Middle click - stopping Willy" << std::endl;

  } else if (event->button == 3) { // Right mouse button - jump
    jump();
    std::cout << "Right click - jumping" << std::endl;

  } else {
    // Debug: Show any other button numbers
    std::cout << "Unknown mouse button: " << event->button << std::endl;
  }

  return true;
}

bool WillyGame::on_button_release(GdkEventButton *event) {
  if (!game_options.mouse_support || current_state != GameState::PLAYING) {
    return false;
  }

  std::cout << "Mouse button " << event->button << " released" << std::endl;

  if (event->button == 1 && mouse_button_held && held_button == 1) {
    // Stop the movement that was being held
    mouse_button_held = false;
    held_button = 0;

    if (mouse_direction == "LEFT" || mouse_direction == "RIGHT") {
      continuous_direction = "";
      moving_continuously = false;
      std::cout << "Released horizontal movement" << std::endl;
    } else if (mouse_direction == "UP") {
      mouse_up_held = false;
      up_pressed = false;
      std::cout << "Released UP movement" << std::endl;
    } else if (mouse_direction == "DOWN") {
      mouse_down_held = false;
      down_pressed = false;
      std::cout << "Released DOWN movement" << std::endl;
    }

    mouse_direction = "";
  }

  return true;
}

bool WillyGame::on_motion_notify(GdkEventMotion *event) {
  if (!game_options.mouse_support || current_state != GameState::PLAYING) {
    return false;
  }

  // Optional: Could implement mouse hover effects here
  // For now, just consume the event
  return true;
}

void WillyGame::setup_ui() {
  add(vbox);

  vbox.pack_start(menubar, Gtk::PACK_SHRINK);

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
