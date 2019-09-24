#include "render.h"

#include <cstring>
#include <iostream>

#include "sprites.h"

typedef enum {
  EmptyCell,
  Brick,
  Cookie,
  Score,
  GameOver,
  Digits,
  Head = 16,
  Body = 32,
  Tail = 48
} FieldSprites;

void copy_image(const gif::Image &src, gif::Rect src_rect, gif::Image &dst,
                const gif::Point &dst_pos) {
  for (unsigned y = 0; y < src_rect.height(); ++y) {
    memcpy(dst.rbits(dst_pos.x(), dst_pos.y() + y),
           src.bits(src_rect.x(), src_rect.y() + y), src_rect.width());
  }
}

void fill_rect(gif::Rect rect, uint8_t color, gif::Image &dst) {
  for (unsigned y = 0; y < rect.height(); ++y) {
    memset(dst.rbits(rect.x(), rect.y() + y), color, rect.width());
  }
}

// MARK: Scheme
Scheme::Scheme(const Game &game)
    : cs(game.field().size().area(), EmptyCell), g(game) {}

void Scheme::update() {
  clear_field();
  put_cookie(g.cookie());
  put_snake(g.snake());
}

void Scheme::clear_field() {
  memset(&cs[0], EmptyCell, g.field().size().area());
}

void Scheme::put_cookie(int pos) { cs[pos] = Cookie; }

Orientation Scheme::orientation(int from, int to) {
  auto fld = g.field();
  if (fld.y(from) == fld.y(to)) {
    if (fld.x(from) < fld.x(to)) {
      return LeftRight;
    } else if (fld.x(from) > fld.x(to)) {
      return RightLeft;
    }
  } else if (fld.x(from) == fld.x(to)) {
    if (fld.y(from) < fld.y(to)) {
      return TopBottom;
    } else if (fld.y(from) > fld.y(to)) {
      return BottomTop;
    }
  }
  return OrientationError;
}

Orientation Scheme::orientation(int from, int via, int to) {
  return (Orientation)((orientation(from, via) & 12) |
                       (orientation(via, to) & 3));
}

void Scheme::put_head(int pos, int next_pos) {
  cs[pos] = Head + orientation(next_pos, pos);
}

void Scheme::put_body(int pos, int prev_pos, int next_pos) {
  cs[pos] = Body + orientation(next_pos, pos, prev_pos);
}

void Scheme::put_tail(int pos, int prev_pos) {
  cs[pos] = Tail + orientation(pos, prev_pos);
}

void Scheme::put_snake(const Snake &snake) {
  for (int i = 0; i < snake.size(); ++i) {
    if (i == 0) {
      put_head(snake.cell(i), snake.cell(i + 1));
    } else if (i == snake.size() - 1) {
      put_tail(snake.cell(i), snake.cell(i - 1));
    } else {
      put_body(snake.cell(i), snake.cell(i - 1), snake.cell(i + 1));
    }
  }
}

// MARK: Sprites

gif::Point pos16x16(unsigned x, unsigned y) {
  return gif::Point(x * 16, y * 16);
}

gif::Point pos8x8(unsigned x, unsigned y) { return gif::Point(x * 8, y * 8); }

Sprites::Sprites() {
  gif::Size s16x16(16, 16);
  gif::Size s8x8(8, 8);

  gif::MemDevice dev;
  dev.open_for_read(SpritesGif, sizeof(SpritesGif));
  gif.load(dev);

  cell_sz = s16x16;
  field_clr = 17;
  trans_clr = 125;

  rects[Brick] = gif::Rect(pos16x16(0, 0), s16x16);
  rects[Cookie] = gif::Rect(pos16x16(0, 1), s16x16);
  rects[Head + LeftRight] = gif::Rect(pos16x16(1, 0), s16x16);
  rects[Head + BottomTop] = gif::Rect(pos16x16(1, 1), s16x16);
  rects[Head + RightLeft] = gif::Rect(pos16x16(1, 2), s16x16);
  rects[Head + TopBottom] = gif::Rect(pos16x16(1, 3), s16x16);
  rects[Tail + LeftRight] = gif::Rect(pos16x16(2, 0), s16x16);
  rects[Tail + BottomTop] = gif::Rect(pos16x16(2, 1), s16x16);
  rects[Tail + RightLeft] = gif::Rect(pos16x16(2, 2), s16x16);
  rects[Tail + TopBottom] = gif::Rect(pos16x16(2, 3), s16x16);
  rects[Body + BottomTop] = gif::Rect(pos16x16(0, 2), s16x16);
  rects[Body + TopBottom] = rects[Body + BottomTop];
  rects[Body + LeftRight] = gif::Rect(pos16x16(0, 3), s16x16);
  rects[Body + RightLeft] = rects[Body + LeftRight];
  rects[Body + LeftBottom] = gif::Rect(pos16x16(3, 0), s16x16);
  rects[Body + BottomLeft] = rects[Body + LeftBottom];
  rects[Body + RightBottom] = gif::Rect(pos16x16(3, 1), s16x16);
  rects[Body + BottomRight] = rects[Body + RightBottom];
  rects[Body + TopRight] = gif::Rect(pos16x16(3, 2), s16x16);
  rects[Body + RightTop] = rects[Body + TopRight];
  rects[Body + LeftTop] = gif::Rect(pos16x16(3, 3), s16x16);
  rects[Body + TopLeft] = rects[Body + LeftTop];
  rects[Score] = gif::Rect(pos8x8(10, 8), gif::Size(40, 8));
  rects[Digits + 0] = gif::Rect(pos8x8(0, 8), s8x8);
  rects[Digits + 1] = gif::Rect(pos8x8(1, 8), s8x8);
  rects[Digits + 2] = gif::Rect(pos8x8(2, 8), s8x8);
  rects[Digits + 3] = gif::Rect(pos8x8(3, 8), s8x8);
  rects[Digits + 4] = gif::Rect(pos8x8(4, 8), s8x8);
  rects[Digits + 5] = gif::Rect(pos8x8(5, 8), s8x8);
  rects[Digits + 6] = gif::Rect(pos8x8(6, 8), s8x8);
  rects[Digits + 7] = gif::Rect(pos8x8(7, 8), s8x8);
  rects[Digits + 8] = gif::Rect(pos8x8(8, 8), s8x8);
  rects[Digits + 9] = gif::Rect(pos8x8(9, 8), s8x8);
  rects[GameOver] = gif::Rect(pos16x16(4, 0), gif::Size(12 * 8, 8 * 8));
}

// MARK: GameRender

gif::Size GameRender::display_size() const {
  auto field_sz = g.field().size();
  auto cell_sz = sprites.cell_size();
  return gif::Size((field_sz.width() + 2) * cell_sz.width(),
                   (field_sz.height() + 2) * cell_sz.height());
}

GameRender::GameRender(const Game &game)
    : frame_count(0), scheme(game), g(game), gif(display_size(), 0) {
  gif.set_color_map(sprites.color_map());
}

std::unique_ptr<gif::Image> GameRender::create_game_frame() const {
  auto res = std::make_unique<gif::Image>(display_size());
  draw_field(*res);
  draw_score(g.score(), *res);
  return res;
}

void GameRender::set_image_show_time(gif::Image &img, int delay) {
  std::vector<gif::Extension> ext;
  if (frame_count == 0) {
    int replays = 0;
    ext.push_back(gif::create_animation_mark(replays));
  }
  ext.push_back(gif::create_delay_mark(delay));
  img.set_extensions(ext);
}

void GameRender::create_separate_image() {
  char filename[100];
  auto img = create_game_frame();
  gif::Gif gif(display_size(), 0);
  gif.set_color_map(sprites.color_map());
  gif.append(std::move(img));

  sprintf(filename, "out%04lu.gif", frame_count);
  gif::FileDevice dev(filename);
  if (dev.open(gif::FileDevice::write_only)) {
    gif.save(dev);
  }
}

void GameRender::save(gif::FileDevice &file) { gif.save(file); }

void GameRender::draw_frame(int delay) {
  scheme.update();
  auto img = create_game_frame();
  set_image_show_time(*img, delay);
  gif.append(std::move(img));
  frame_count++;
}

void GameRender::draw_game_over(int delay) {
  scheme.update();
  auto img = create_game_frame();
  draw_game_over_msg(*img);
  set_image_show_time(*img, delay);
  gif.append(std::move(img));
  frame_count++;
}

void GameRender::draw_sprite(uint8_t sprite, const gif::Point &pos,
                             gif::Image &dst) const {
  if (sprite == EmptyCell) {
    fill_rect(gif::Rect(pos, sprites.cell_size()), sprites.field_color(), dst);
  } else {
    copy_image(sprites.image(), sprites.rect(sprite), dst, pos);
  }
}

void GameRender::draw_transparent_sprite(uint8_t sprite, const gif::Point &pos,
                                         gif::Image &dst) const {
  auto src = sprites.image();
  auto src_rect = sprites.rect(sprite);
  auto trn = sprites.transparent_color();
  for (unsigned y = 0; y < src_rect.height(); ++y) {
    auto s = src.bits(src_rect.x(), src_rect.y() + y);
    auto d = dst.rbits(pos.x(), pos.y() + y);
    for (unsigned x = 0; x < src_rect.width(); ++x, ++d, ++s) {
      if (*s != trn) *d = *s;
    }
  }
}

void GameRender::draw_field_border(gif::Image &dst) const {
  auto sz = g.field().size();
  auto img = sprites.image();
  auto rect = sprites.rect(Brick);
  auto bw = rect.width();
  auto bh = rect.height();
  for (unsigned i = 0; i < sz.width() + 2; ++i) {
    unsigned x = i * bw;
    copy_image(img, rect, dst, gif::Point(x, 0));
    copy_image(img, rect, dst, gif::Point(x, (sz.height() + 1) * bh));
  }

  for (unsigned i = 0; i < sz.height(); ++i) {
    unsigned y = (i + 1) * bh;
    copy_image(img, rect, dst, gif::Point(0, y));
    copy_image(img, rect, dst, gif::Point((sz.width() + 1) * bw, y));
  }
}

gif::Point GameRender::cell_gif_pos(size_t cell_num) const {
  auto border = sprites.cell_size();
  auto fld = g.field();
  return gif::Point(border.width() + border.width() * fld.x(cell_num),
                    border.height() + border.height() * fld.y(cell_num));
}

void GameRender::draw_field(gif::Image &dst) const {
  auto cells = scheme.cells();
  size_t cells_num = g.field().size().area();
  draw_field_border(dst);  // TODO: cache
  for (size_t i = 0; i < cells_num; ++i) {
    draw_sprite(cells[i], cell_gif_pos(i), dst);
  }
}

void GameRender::draw_num(unsigned num, const gif::Point &pos,
                          gif::Image &dst) const {
  gif::Point p = pos;
  char buf[32];
  char *numStr = buf;
  sprintf(buf, "%0d", num);
  while (*numStr) {
    uint8_t sprite = Digits + *numStr++ - '0';
    draw_transparent_sprite(sprite, p, dst);
    p.set_x(p.x() + sprites.rect(sprite).width());
  }
}

int num_digits(unsigned num) {
  int res = 0;
  while (num) {
    num /= 10;
    res++;
  }
  return res ? res : 1;
}

void GameRender::draw_score(unsigned score, gif::Image &dst) const {
  auto msg_width = sprites.rect(Score).width() +
                   sprites.rect(Digits).width() * (num_digits(score) + 1);

  auto msg_x = (display_size().width() - msg_width) / 2;
  auto msg_y = 4;
  gif::Point score_pos(msg_x, msg_y);
  gif::Size score_size(msg_width, sprites.rect(Digits).height());
  int indent = 1;
  gif::Rect back_rect(
      gif::Point(score_pos.x() - indent, score_pos.y() - indent),
      gif::Size(score_size.width() + 2 * indent,
                score_size.height() + 2 * indent));
  fill_rect(back_rect, 116, dst);

  draw_transparent_sprite(Score, score_pos, dst);

  auto num_x =
      msg_x + sprites.rect(Score).width() + sprites.rect(Digits).width();
  draw_num(score, gif::Point(num_x, msg_y), dst);
}

void GameRender::draw_game_over_msg(gif::Image &dst) const {
  uint8_t sprite = GameOver;
  gif::Size ds = dst.size();
  gif::Size ss = sprites.rect(sprite).size();
  gif::Point pos((ds.width() - ss.width()) / 2,
                 (ds.height() - ss.height()) / 2);
  draw_sprite(sprite, pos, dst);
}

AsciiRender::AsciiRender(const Game &game)
    : g(game), scheme(game), glyphs(64, "   ") {
  glyphs[EmptyCell] = "· ";
  glyphs[Cookie] = "@ ";
  glyphs[Head + LeftRight] = "╼╸";
  glyphs[Head + BottomTop] = "╿ ";
  glyphs[Head + RightLeft] = "╺╾";
  glyphs[Head + TopBottom] = "╽ ";
  glyphs[Tail + LeftRight] = " ─";
  glyphs[Tail + BottomTop] = "╵ ";
  glyphs[Tail + RightLeft] = "─ ";
  glyphs[Tail + TopBottom] = "╷ ";
  glyphs[Body + BottomTop] = "│ ";
  glyphs[Body + TopBottom] = "│ ";
  glyphs[Body + LeftRight] = "──";
  glyphs[Body + RightLeft] = "──";
  glyphs[Body + LeftBottom] = "╮ ";
  glyphs[Body + BottomLeft] = "╮ ";
  glyphs[Body + RightBottom] = "╭─";
  glyphs[Body + BottomRight] = "╭─";
  glyphs[Body + TopRight] = "╰─";
  glyphs[Body + RightTop] = "╰─";
  glyphs[Body + LeftTop] = "╯ ";
  glyphs[Body + TopLeft] = "╯ ";
}

void AsciiRender::draw_frame() {
  scheme.update();

  std::cout << "Score: " << g.score() << "\n";

  Size sz = g.field().size();
  int n = sz.area();
  auto s = scheme.cells();

  std::cout << "╔";
  for (int i = 0; i < sz.width(); ++i) {
    std::cout << "══";
  }
  std::cout << "╗\n║";

  for (int i = 0; i < n; ++i) {
    std::cout << glyphs[s[i]];
    if (i % sz.width() == sz.width() - 1) {
      std::cout << "║\n";
      int h = i / sz.width();
      if (h < sz.height() - 1) {
        std::cout << "║";
      }
    }
  }

  std::cout << "╚";
  for (int i = 0; i < sz.width(); ++i) {
    std::cout << "══";
  }
  std::cout << "╝\n";

  if (g.is_over()) {
    std::cout << "Game Over";
  }

  std::cout << "\n";
}
