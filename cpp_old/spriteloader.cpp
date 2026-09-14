#include "willy.h"
#include <cstring>
#include <getopt.h>
#include <unistd.h>

extern double redbg;
extern double greenbg;
extern double bluebg;

SpriteLoader::SpriteLoader(int scale) : scale_factor(scale) {
  // Initialize sprite name mapping
  named_parts["0"] = "WILLY_RIGHT";
  named_parts["1"] = "WILLY_LEFT";
  named_parts["2"] = "PRESENT";
  named_parts["3"] = "LADDER";
  named_parts["4"] = "TACK";
  named_parts["5"] = "UPSPRING";
  named_parts["6"] = "SIDESPRING";
  named_parts["7"] = "BALL";
  named_parts["8"] = "BELL";

  // Add pipe parts 51-90
  for (int i = 51; i <= 90; i++) {
    named_parts[std::to_string(i)] = "PIPE" + std::to_string(i - 50);
  }
  named_parts["126"] = "BALLPIT";
  named_parts["127"] = "EMPTY";

  load_sprites();
}

std::string SpriteLoader::find_chr_file() {
  std::vector<std::string> possible_paths = {
      "willy.chr", "data/willy.chr", "/usr/games/willytheworm/data/willy.chr"};

  for (const auto &path : possible_paths) {
    std::ifstream file(path, std::ios::binary);
    if (file.good()) {
      return path;
    }
  }
  return "";
}

void SpriteLoader::load_sprites() {
  std::string chr_path = find_chr_file();

  if (!chr_path.empty()) {
    try {
      load_chr_file(chr_path);
      std::cout << "Loaded sprites from " << chr_path << std::endl;
      return;
    } catch (const std::exception &e) {
      std::cout << "Error loading .chr file: " << e.what() << std::endl;
    }
  }

  std::cout << "Creating fallback sprites..." << std::endl;
  create_fallback_sprites();
}

void SpriteLoader::load_chr_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Cannot open file");
  }

  // Read as 8x8 bitmap format
  file.seekg(0, std::ios::end);
  size_t file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(file_size);
  file.read(reinterpret_cast<char *>(data.data()), file_size);

  load_old_format(data);
}

void SpriteLoader::load_old_format(const std::vector<uint8_t> &data) {
  int num_chars = data.size() / 8;

  for (int i = 0; i < num_chars; i++) {
    auto it = named_parts.find(std::to_string(i));
    if (it != named_parts.end()) {
      try {
        auto surface = create_sprite_from_bitmap(data, i);
        sprites[it->second] = surface;
      } catch (const std::exception &e) {
        std::cout << "Error creating sprite " << i << ": " << e.what()
                  << std::endl;
      }
    }
  }
}

Cairo::RefPtr<Cairo::ImageSurface>
SpriteLoader::create_sprite_from_bitmap(const std::vector<uint8_t> &data,
                                        int char_index) {

  int size = GAME_CHAR_WIDTH * scale_factor;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
  auto ctx = Cairo::Context::create(surface);

  // Clear to transparent
  ctx->set_operator(Cairo::OPERATOR_CLEAR);
  ctx->paint();
  ctx->set_operator(Cairo::OPERATOR_OVER);

  // Extract bits and draw pixels
  for (int row = 0; row < 8; row++) {
    uint8_t byte = data[char_index * 8 + row];
    for (int col = 0; col < 8; col++) {
      int bit = (byte >> (7 - col)) & 1;
      if (bit) {
        ctx->set_source_rgba(1.0, 1.0, 1.0, 1.0); // White pixel
        ctx->rectangle(col * scale_factor, row * scale_factor, scale_factor,
                       scale_factor);
        ctx->fill();
      }
    }
  }

  return surface;
}

void SpriteLoader::create_fallback_sprites() {
  sprites["WILLY_RIGHT"] = create_willy_sprite(true);
  sprites["WILLY_LEFT"] = create_willy_sprite(false);
  sprites["PIPE1"] = create_colored_rect(0.5, 0.5, 0.5);
  sprites["LADDER"] = create_ladder_sprite();
  sprites["PRESENT"] = create_colored_rect(1.0, 0.0, 1.0);
  sprites["BALL"] = create_ball_sprite();
  sprites["BELL"] = create_bell_sprite();
  sprites["TACK"] = create_tack_sprite();
  sprites["UPSPRING"] = create_spring_sprite(true);
  sprites["SIDESPRING"] = create_spring_sprite(false);
  sprites["BALLPIT"] = create_colored_rect(0.3, 0.3, 0.3);
  sprites["EMPTY"] = create_empty_sprite();

  // Add all pipe variations
  for (int i = 1; i <= 40; i++) {
    sprites["PIPE" + std::to_string(i)] = create_colored_rect(0.5, 0.5, 0.5);
  }
}

Cairo::RefPtr<Cairo::ImageSurface>
SpriteLoader::create_willy_sprite(bool facing_right) {
  int size = GAME_CHAR_WIDTH * scale_factor;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
  auto ctx = Cairo::Context::create(surface);

  // Yellow body
  ctx->set_source_rgb(1.0, 1.0, 0.0);
  ctx->rectangle(0, 0, size, size);
  ctx->fill();

  // Eyes
  ctx->set_source_rgb(0.0, 0.0, 0.0);
  int eye_size = std::max(1, size / 8);
  if (facing_right) {
    ctx->rectangle(size * 0.25, size * 0.25, eye_size, eye_size);
    ctx->fill();
    ctx->rectangle(size * 0.625, size * 0.25, eye_size, eye_size);
    ctx->fill();
  } else {
    ctx->rectangle(size * 0.125, size * 0.25, eye_size, eye_size);
    ctx->fill();
    ctx->rectangle(size * 0.5, size * 0.25, eye_size, eye_size);
    ctx->fill();
  }

  return surface;
}

Cairo::RefPtr<Cairo::ImageSurface>
SpriteLoader::create_colored_rect(double r, double g, double b) {
  int size = GAME_CHAR_WIDTH * scale_factor;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
  auto ctx = Cairo::Context::create(surface);

  ctx->set_source_rgb(r, g, b);
  ctx->rectangle(0, 0, size, size);
  ctx->fill();

  return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_ladder_sprite() {
  int size = GAME_CHAR_WIDTH * scale_factor;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
  auto ctx = Cairo::Context::create(surface);

  // Brown color
  ctx->set_source_rgb(0.6, 0.3, 0.0);

  // Side rails
  int rail_width = std::max(1, size / 4);
  ctx->rectangle(0, 0, rail_width, size);
  ctx->fill();
  ctx->rectangle(size - rail_width, 0, rail_width, size);
  ctx->fill();

  // Rungs
  int rung_height = std::max(1, size / 8);
  for (int i = 0; i < size; i += size / 4) {
    ctx->rectangle(0, i, size, rung_height);
    ctx->fill();
  }

  return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_ball_sprite() {
  int size = GAME_CHAR_WIDTH * scale_factor;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
  auto ctx = Cairo::Context::create(surface);

  ctx->set_source_rgb(1.0, 0.0, 0.0); // Red
  ctx->arc(size / 2.0, size / 2.0, size / 2.0 - 1, 0, 2 * M_PI);
  ctx->fill();

  return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_bell_sprite() {
  int size = GAME_CHAR_WIDTH * scale_factor;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
  auto ctx = Cairo::Context::create(surface);

  ctx->set_source_rgb(1.0, 1.0, 0.0); // Gold
  ctx->arc(size / 2.0, size / 2.0, size / 2.0 - 1, 0, 2 * M_PI);
  ctx->fill();

  // Bell details
  ctx->set_source_rgb(0.8, 0.8, 0.0);
  ctx->arc(size / 2.0, size / 3.0, size / 4.0, 0, 2 * M_PI);
  ctx->fill();

  return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_tack_sprite() {
  int size = GAME_CHAR_WIDTH * scale_factor;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
  auto ctx = Cairo::Context::create(surface);

  ctx->set_source_rgb(0.5, 0.5, 0.5); // Gray
  // Triangle pointing up
  ctx->move_to(size / 2.0, 0);
  ctx->line_to(0, size);
  ctx->line_to(size, size);
  ctx->close_path();
  ctx->fill();

  return surface;
}

Cairo::RefPtr<Cairo::ImageSurface>
SpriteLoader::create_spring_sprite(bool upward) {
  int size = GAME_CHAR_WIDTH * scale_factor;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
  auto ctx = Cairo::Context::create(surface);

  if (upward) {
    ctx->set_source_rgb(0.0, 1.0, 0.0); // Green
    // Horizontal lines (coil effect)
    int line_height = std::max(1, size / 8);
    for (int i = 0; i < 4; i++) {
      int y = size - (i + 1) * (size / 4);
      ctx->rectangle(size * 0.25, y, size * 0.5, line_height);
      ctx->fill();
    }
  } else {
    ctx->set_source_rgb(redbg, greenbg, bluebg); // Blue
    // Vertical lines (coil effect)
    int line_width = std::max(1, size / 8);
    for (int i = 0; i < 4; i++) {
      int x = i * (size / 4);
      ctx->rectangle(x, size * 0.25, line_width, size * 0.5);
      ctx->fill();
    }
  }

  return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> SpriteLoader::create_empty_sprite() {
  int size = GAME_CHAR_WIDTH * scale_factor;
  return Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, size, size);
}

Cairo::RefPtr<Cairo::ImageSurface>
SpriteLoader::get_sprite(const std::string &name) {
  auto it = sprites.find(name);
  if (it != sprites.end()) {
    return it->second;
  }
  return sprites["EMPTY"];
}
