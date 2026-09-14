#include "willy.h"

extern double redbg;
extern double greenbg;
extern double bluebg;

void WillyGame::draw_intro_screen(const Cairo::RefPtr<Cairo::Context> &cr) {
  // Get menubar height to use as offset
  Gtk::Requisition menubar_min, menubar_nat;
  menubar.get_preferred_size(menubar_min, menubar_nat);
  int menubar_height = menubar_min.height;

  // Get current drawing area size
  Gtk::Allocation allocation = drawing_area.get_allocation();
  int area_width = allocation.get_width();
  int area_height = allocation.get_height() - menubar_height;

  // Apply offset for menubar and clear background
  cr->save();
  cr->translate(0, menubar_height);

  // Clear the intro area with blue background
  cr->set_source_rgb(redbg, greenbg, bluebg);
  cr->rectangle(0, 0, area_width, area_height);
  cr->fill();

  // Text data
  std::vector<std::vector<std::string>> textdata = {
      {""},
      {""},
      {"Willy the Worm"},
      {""},
      {"By Jason Hall"},
      {"(original version by Alan Farmer 1985)"},
      {""},
      {"This code is Free Open Source Software (FOSS)"},
      {"Please feel free to do with it whatever you wish."},
      {""},
      {"If you do make changes though such as new levels,"},
      {"please share them with the world."},
      {""},
      {""},
      {"Meet Willy the Worm ", "WILLY_RIGHT", ". Willy is a fun-"},
      {"loving invertebrate who likes to climb"},
      {"ladders ", "LADDER", " bounce on springs ", "UPSPRING", " ",
       "SIDESPRING"},
      {"and find his presents ", "PRESENT", ".  But more"},
      {"than anything, Willy loves to ring,"},
      {"bells! ", "BELL"},
      {""},
      {"You can press the arrow keys ← ↑ → ↓"},
      {"to make Willy run and climb, or the"},
      {"space bar to make him jump. Anything"},
      {"else will make Willy stop and wait"},
      {""},
      {"Good luck, and don't let Willy step on"},
      {"a tack ", "TACK", " or get ran over by a ball! ", "BALL"},
      {""},
      {"Press Enter to Continue"}};

  // Calculate appropriate sizing
  int num_lines = textdata.size();
  int available_height = area_height - 40; // Leave some margin
  int line_height = available_height / num_lines;
  int base_font_size = std::max(10, std::min(20, line_height - 2));

  // Set up font
  Pango::FontDescription font_desc;
  font_desc.set_family("Courier");
  font_desc.set_size(base_font_size * PANGO_SCALE);

  auto layout = Pango::Layout::create(cr);
  layout->set_font_description(font_desc);

  // Sprite size should match text height
  int sprite_size = base_font_size;

  for (size_t line_idx = 0; line_idx < textdata.size(); line_idx++) {
    const auto &line = textdata[line_idx];

    if (line.empty() || (line.size() == 1 && line[0].empty())) {
      continue;
    }

    int y_pos = 20 + line_idx * line_height; // 20px top margin

    // Calculate line width for centering
    int line_width = 0;
    for (const auto &element : line) {
      if (element.empty())
        continue;

      if (element == "WILLY_RIGHT" || element == "WILLY_LEFT" ||
          element == "LADDER" || element == "UPSPRING" ||
          element == "SIDESPRING" || element == "PRESENT" ||
          element == "BELL" || element == "TACK" || element == "BALL") {
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
    for (const auto &element : line) {
      if (element.empty())
        continue;

      if (element == "WILLY_RIGHT" || element == "WILLY_LEFT" ||
          element == "LADDER" || element == "UPSPRING" ||
          element == "SIDESPRING" || element == "PRESENT" ||
          element == "BELL" || element == "TACK" || element == "BALL") {

        auto sprite = sprite_loader->get_sprite(element);
        if (sprite) {
          cr->save();
          cr->translate(current_x, y_pos);

          // Scale sprite to match text size
          double sprite_scale =
              (double)sprite_size / (GAME_CHAR_WIDTH * scale_factor);
          cr->scale(sprite_scale, sprite_scale);

          cr->set_source(sprite, 0, 0);
          cr->paint();
          cr->restore();
        }
        current_x += sprite_size;
      } else {
        // Draw text
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

void WillyGame::draw_game_over_screen(const Cairo::RefPtr<Cairo::Context> &cr) {
  // Create font
  Pango::FontDescription font_desc;
  font_desc.set_family("Monospace");
  font_desc.set_size(24 * PANGO_SCALE);

  auto layout = create_pango_layout("");
  layout->set_font_description(font_desc);

  cr->set_source_rgb(1.0, 1.0, 1.0); // White text

  // Game Over text
  layout->set_text("GAME OVER");
  int text_width, text_height;
  layout->get_pixel_size(text_width, text_height);

  int x_offset =
      (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
  int y_offset =
      (GAME_SCREEN_HEIGHT * GAME_CHAR_HEIGHT * scale_factor - text_height) / 2 -
      50;

  cr->move_to(x_offset, y_offset);
  layout->show_in_cairo_context(cr);

  // Final Score
  std::string score_text = "Final Score: " + std::to_string(score);
  layout->set_text(score_text);
  layout->get_pixel_size(text_width, text_height);

  x_offset =
      (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
  y_offset += 60;

  cr->move_to(x_offset, y_offset);
  layout->show_in_cairo_context(cr);

  // Continue text
  layout->set_text("Press Enter to return to intro");
  layout->get_pixel_size(text_width, text_height);

  x_offset =
      (GAME_SCREEN_WIDTH * GAME_CHAR_WIDTH * scale_factor - text_width) / 2;
  y_offset += 80;

  cr->move_to(x_offset, y_offset);
  layout->show_in_cairo_context(cr);
}

// WillyApplication implementation
WillyApplication::WillyApplication()
    : Gtk::Application("org.example.willytheworm") {}

Glib::RefPtr<WillyApplication> WillyApplication::create() {
  return Glib::RefPtr<WillyApplication>(new WillyApplication());
}

void WillyGame::flash_death_screen() {
  // Get the window and context for drawing
  auto window = drawing_area.get_window();
  if (!window)
    return;

  auto cr = window->create_cairo_context();

  // Flash white for 0.25 seconds
  cr->set_source_rgb(1.0, 1.0, 1.0); // White
  cr->paint();

  // Force immediate display update
  window->invalidate(false);

  // Process pending GTK events to ensure the white screen shows
  while (Gtk::Main::events_pending()) {
    Gtk::Main::iteration();
  }

  // Hold the white screen for 0.25 seconds
  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // Return to normal - this will be handled by the next game tick/draw cycle
}

void WillyGame::flash_death_screen_seizure() {
  // Get the current drawing area allocation
  Gtk::Allocation allocation = drawing_area.get_allocation();

  // Calculate the duration of the flashing in seconds (0.25 seconds like
  // Python)
  double duration = 0.25;

  // Calculate the number of times to switch between colors (assuming ~60 FPS)
  int num_flashes = static_cast<int>(duration * 60);

  // Colors to alternate between
  Cairo::RefPtr<Cairo::ImageSurface> surface1 = Cairo::ImageSurface::create(
      Cairo::FORMAT_RGB24, allocation.get_width(), allocation.get_height());
  Cairo::RefPtr<Cairo::ImageSurface> surface2 = Cairo::ImageSurface::create(
      Cairo::FORMAT_RGB24, allocation.get_width(), allocation.get_height());

  // Fill surfaces with different colors
  auto ctx1 = Cairo::Context::create(surface1);
  ctx1->set_source_rgb(1.0, 1.0, 1.0); // White
  ctx1->paint();

  auto ctx2 = Cairo::Context::create(surface2);
  ctx2->set_source_rgb(redbg, greenbg,
                       bluebg); // Blue (or current background color)
  ctx2->paint();

  // Get the window and context for drawing
  auto window = drawing_area.get_window();
  if (!window)
    return;

  auto cr = window->create_cairo_context();

  // Start the flashing
  for (int i = 0; i < num_flashes; i++) {
    // Alternate between the two colors
    if (i % 2 == 0) {
      cr->set_source(surface1, 0, 0);
    } else {
      cr->set_source(surface2, 0, 0);
    }

    cr->paint();

    // Force immediate display update
    window->invalidate(false);

    // Process pending GTK events
    while (Gtk::Main::events_pending()) {
      Gtk::Main::iteration();
    }

    // Wait for a short amount of time before the next frame
    std::this_thread::sleep_for(
        std::chrono::microseconds(static_cast<int>(1000000.0 / 60.0)));
  }

  // Clear back to normal background
  cr->set_source_rgb(redbg, greenbg, bluebg); // Blue
  cr->paint();
  window->invalidate(false);
}

bool WillyGame::on_draw(const Cairo::RefPtr<Cairo::Context> &cr) {
  // Only paint blue background for intro screen
  if (current_state == GameState::INTRO) {
    cr->set_source_rgb(redbg, greenbg,
                       bluebg); // Blue background for intro only
    cr->paint();
    draw_intro_screen(cr);
  } else if (current_state == GameState::PLAYING) {
    draw_game_screen(cr);
  } else if (current_state == GameState::GAME_OVER) {
    draw_game_over_screen(cr);
  } else if (current_state == GameState::HIGH_SCORE_ENTRY) {
    draw_high_score_entry_screen(cr);
  } else if (current_state == GameState::HIGH_SCORE_DISPLAY) {
    draw_high_score_display_screen(cr);
  }

  return true;
}

std::pair<int, int> WillyGame::find_ballpit_position() {
  for (int row = 0; row < GAME_MAX_HEIGHT; row++) {
    for (int col = 0; col < GAME_MAX_WIDTH; col++) {
      std::string tile = get_tile(row, col);
      if (tile == "BALLPIT") {
        return {row, col}; // Return the first position found
      }
    }
  }
  return {-1, -1}; // Indicate that no BALLPIT was found
}

void WillyGame::draw_game_screen(const Cairo::RefPtr<Cairo::Context> &cr) {
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

  // Draw ALL sprite positions with blue background, even empty ones
  for (int row = 0; row < GAME_MAX_HEIGHT; row++) {
    for (int col = 0; col < GAME_MAX_WIDTH; col++) {
      int x = col * scaled_char_width;
      int y = row * scaled_char_height;

      // Paint blue background for EVERY sprite position
      cr->set_source_rgb(redbg, greenbg, bluebg);
      cr->rectangle(x, y, scaled_char_width, scaled_char_height);
      cr->fill();

      // Get the tile at this position
      std::string tile = get_tile(row, col);

      // Draw sprite if not empty or Willy start position, but not at Willy's
      // current position
      if (tile != "EMPTY" && tile.find("WILLY") == std::string::npos &&
          !(row == willy_position.first && col == willy_position.second)) {
        auto sprite = sprite_loader->get_sprite(tile);
        if (sprite) {
          cr->set_source(sprite, x, y);
          cr->paint();
        }
      }
    }
  }

  // Draw balls (but not the ones in ball pits or at Willy's position)
  for (const auto &ball : balls) {
    if (get_tile(ball.row, ball.col) != "BALLPIT" &&
        !(ball.row == willy_position.first &&
          ball.col == willy_position.second)) {

      // Make sure ball is in visible area
      if (ball.row >= 0 && ball.row < GAME_MAX_HEIGHT && ball.col >= 0 &&
          ball.col < GAME_MAX_WIDTH) {

        int x = ball.col * scaled_char_width;
        int y = ball.row * scaled_char_height;

        auto sprite = sprite_loader->get_sprite("BALL");
        if (sprite) {
          cr->set_source(sprite, x, y);
          cr->paint();
        }
      }
    }
  }

  // Draw Willy - make sure he's in visible area
  if (willy_position.first >= 0 && willy_position.first < GAME_MAX_HEIGHT &&
      willy_position.second >= 0 && willy_position.second < GAME_MAX_WIDTH) {

    int x = willy_position.second * scaled_char_width;
    int y = willy_position.first * scaled_char_height;

    std::string sprite_name =
        (willy_direction == "LEFT") ? "WILLY_LEFT" : "WILLY_RIGHT";
    auto sprite = sprite_loader->get_sprite(sprite_name);
    if (sprite) {
      cr->set_source(sprite, x, y);
      cr->paint();
    }
  }

  // Draw status information below the game area
  if (current_state == GameState::PLAYING) {
    // Calculate status bar area (below the main game area)
    int status_y = (GAME_SCREEN_HEIGHT + 1) *
                   scaled_char_height;          // +1 to go BELOW the last line
    int status_height = 2 * scaled_char_height; // Give it some height

    // Draw blue background for status area (same as game background)
    cr->set_source_rgb(redbg, greenbg, bluebg);
    cr->rectangle(0, status_y, GAME_SCREEN_WIDTH * scaled_char_width,
                  status_height);
    cr->fill();

    // Create font for status text
    Pango::FontDescription font_desc;
    font_desc.set_family("Courier");
    // Scale font size based on the current scale - make it bigger and scale
    // properly
    int font_size =
        std::max(12, (int)(16 * std::min(current_scale_x, current_scale_y)));
    if (font_size > 16) {
      font_size = 16;
    }
    font_desc.set_size(font_size * PANGO_SCALE);

    auto layout = Pango::Layout::create(cr);
    layout->set_font_description(font_desc);
    // Create status text with fixed-width formatting
    char status_buffer[200];
    snprintf(
        status_buffer, sizeof(status_buffer),
        "SCORE: %5d    BONUS: %4d    LEVEL: %2d    WILLY THE WORMS LEFT: %3d",
        score, bonus, level, lives);
    std::string status_text = status_buffer;

    layout->set_text(status_text);

    // Draw white text
    cr->set_source_rgb(1.0, 1.0, 1.0);

    // Position text in the status area
    int text_width, text_height;
    layout->get_pixel_size(text_width, text_height);

    // Center the text in the status area
    int text_x = (GAME_SCREEN_WIDTH * scaled_char_width - text_width) / 2;
    int text_y = status_y + (status_height - text_height) / 2;

    cr->move_to(text_x, text_y);
    layout->show_in_cairo_context(cr);
  }

  cr->restore();
}

void WillyGame::update_status_bar() {
  if (current_state == GameState::PLAYING) {
    std::string status_text =
        "SCORE: " + std::to_string(score) +
        "    BONUS: " + std::to_string(bonus) +
        "    Level: " + std::to_string(level) +
        "    Willy the Worms Left: " + std::to_string(lives);
    status_bar.set_text(status_text);
  } else {
    status_bar.set_text("Willy the Worm - C++ GTK Edition");
  }
}
