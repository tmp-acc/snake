#include <algorithm>
#include <vector>

#include "game.h"

// MARK: Field

bool Field::can_move(int cell, Dir dir) const {
  if (dir == Dir::Left) {
    return cell % sz.width() > 0;
  }
  if (dir == Dir::Right) {
    return cell % sz.width() < sz.width() - 1;
  }
  if (dir == Dir::Up) {
    return cell > sz.width() - 1;
  }
  if (dir == Dir::Down) {
    return cell < (sz.area() - sz.width());
  }
  return false;
}

int Field::move_value(Dir dir) const {
  switch (dir) {
    case Dir::Left:
      return -1;
    case Dir::Right:
      return 1;
    case Dir::Up:
      return -sz.width();
    case Dir::Down:
      return sz.width();
    default:
      return 0;
  }
}

// MARK: Snake

Snake::Snake(const Field &field)
    : cs(field.size().area() + 1), sz(0), f(field) {
  int len = 3;
  int y = f.size().height() / 2;
  for (int i = 0; i < len; ++i) {
    cs[i] = f.cell(len - i, y);
    sz++;
  }
}

// TODO:  (sort?), algorithm
bool Snake::contains(int cell, bool test_tail) const {
  int end = test_tail ? sz : sz - 1;
  for (int i = 0; i < end; ++i) {
    if (cs[i] == cell) {
      return true;
    }
  }
  return false;
}

// TODO:  (sort?), algorithm
bool Snake::eats_itself() const {
  for (int i = 1; i < sz; ++i) {  // from 1
    if (cs[i] == head()) {
      return true;
    }
  }
  return false;
}

void Snake::move(Dir dir) {
  memmove(&cs[1], &cs[0], sz * sizeof(int));
  cs[0] += f.move_value(dir);
}

void Snake::grow() {
  if (sz < cs.size()) {
    sz++;
  }
}

// MARK: Game

Game::Game(Size field_size)
    : f(field_size),
      s(f),
      cooky(0),
      scr(0),
      over(false),
      mt(2)
{
  new_cookie();
}

void Game::move(Dir dir, bool *cookie_eaten) {
  if (over) {
    return;
  }
  if (!f.can_move(s.head(), dir) ||
      s.contains(s.head() + f.move_value(dir), false)) {
    over = true;
    return;
  }

  s.move(dir);
  *cookie_eaten = s.head() == cooky;

  if (*cookie_eaten) {
    scr += 1;
    s.grow();
    if (s.size() < f.size().area()) {
      new_cookie();
    } else {
      over = true;
    }
  }
}

void Game::new_cookie() { cooky = random_free_cell(); }

int Game::random_free_cell() const {
  std::uniform_int_distribution<int> dist(0, f.size().area() - s.size() - 1);
  int res = 0;
  int rnd = dist(mt);
  bool used = s.contains(res, true);
  while (rnd > 0 || used) {
    if (!used) --rnd;
    ++res;
    used = s.contains(res, true);
  }
  return res;
}
