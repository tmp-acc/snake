#ifndef RENDER_H
#define RENDER_H

#include "game.h"
#include "gif.h"

typedef enum {
  OrientationError = 0,
  TopBottom,
  TopLeft,
  TopRight,
  BottomTop,
  BottomLeft = 6,
  BottomRight,
  LeftTop,
  LeftBottom,
  LeftRight = 11,
  RightTop,
  RightBottom,
  RightLeft
} Orientation;

class Scheme {
 public:
  Scheme(const Game &game);
  const std::vector<uint8_t> &cells() const { return cs; }
  void update();

 private:
  void clear_field();
  void put_cookie(int pos);
  Orientation orientation(int from, int to);
  Orientation orientation(int from, int via, int to);
  void put_head(int pos, int next_pos);
  void put_body(int pos, int prev_pos, int next_pos);
  void put_tail(int pos, int prev_pos);
  void put_snake(const Snake &snake);

  std::vector<uint8_t> cs;
  const Game &g;
};

class Sprites {
 public:
  Sprites();
  const gif::Image &image() const { return *gif.images()[0].get(); }
  gif::ColorMap color_map() const { return *gif.color_map(); }
  const gif::Rect &rect(uint8_t sprite) const { return rects[sprite]; }
  gif::Size cell_size() const { return cell_sz; }
  uint8_t field_color() const { return field_clr; }
  uint8_t transparent_color() const { return trans_clr; }

 private:
  uint8_t trans_clr;
  uint8_t field_clr;
  gif::Size cell_sz;
  gif::Rect rects[64];
  gif::Gif gif;
};

class GameRender {

 public:
  GameRender(const Game &game);
  void draw_frame(int delay);
  void draw_game_over(int delay);
  void save(gif::FileDevice &file);

 private:
  gif::Size display_size() const;
  void draw_sprite(uint8_t sprite, const gif::Point &pos,
                   gif::Image &dst) const;
  void draw_transparent_sprite(uint8_t sprite, const gif::Point &pos,
                               gif::Image &dst) const;
  void draw_num(unsigned num, const gif::Point &pos, gif::Image &dst) const;
  void draw_field_border(gif::Image &dst) const;
  gif::Point cell_gif_pos(size_t cell_num) const ;
  void draw_field(gif::Image &dst) const;
  void draw_score(unsigned score, gif::Image &dst) const;
  void draw_game_over_msg(gif::Image &dst) const;
  std::unique_ptr<gif::Image> create_game_frame() const;
  void set_image_show_time(gif::Image& img, int delay);
  void create_separate_image();

 private:
  size_t frame_count;
  Scheme scheme;
  const Game &g;
  Sprites sprites;
  gif::Gif gif;
};

class AsciiRender {
 public:
  AsciiRender(const Game &game);
  void draw_frame();

 private:
  const Game &g;
  Scheme scheme;
  std::vector<std::string> glyphs;
};

#endif  // RENDER_H