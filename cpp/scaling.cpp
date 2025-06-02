#include "willy.h"

void WillyGame::calculate_scaling_factors() {
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
  int available_height =
      window_height - menubar_min.height - statusbar_min.height;

  // Calculate scale based on height, unless width is smaller than height
  double scale;
  if (available_width < available_height) {
    // Width is the limiting factor
    scale = (double)available_width / base_game_width;
  } else {
    // Height is the limiting factor (normal case)
    scale = (double)available_height / base_game_height;
  }

  // Round to nearest 0.1
  double rounded_scale = std::round(scale * 10.0) / 10.0;

  // Apply the same scale to both dimensions to maintain aspect ratio
  current_scale_x = rounded_scale;
  current_scale_y = rounded_scale;

  // Don't scale below 0.1 or above 10.0 for sanity
  current_scale_x = std::max(0.1, std::min(10.0, current_scale_x));
  current_scale_y = std::max(0.1, std::min(10.0, current_scale_y));
}

void WillyGame::on_window_resize() {
  calculate_scaling_factors();

  // Force a redraw
  drawing_area.queue_draw();
}
