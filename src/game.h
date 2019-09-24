#ifndef GAME_H
#define GAME_H

#include <random>
#include <vector>

class Size {
 public:
  Size(int width, int height) : wd(width), ht(height) {}
  int width() const { return wd; }
  int height() const { return ht; }
  int area() const { return wd * ht; }

 private:
  int wd;
  int ht;
};

enum class Dir { Left, Right, Up, Down, Err };

class Field {
 public:
  Field(const Size &size) : sz(size) {}
  Size size() const { return sz; }
  bool can_move(int cell, Dir dir) const;
  int x(int cell) const { return cell % sz.width(); }
  int y(int cell) const { return cell / sz.width(); }
  int cell(int x, int y) const { return y * sz.width() + x; }
  int move_value(Dir dir) const;

 private:
  Size sz;
};

class Snake {
 public:
  Snake(const Field &field);
  bool contains(int cell, bool test_tail) const;
  bool eats_itself() const;
  int tail() const { return cs[sz - 1]; }
  int head() const { return cs[0]; }
  int cell(int i) const { return cs[i]; }
  int size() const { return sz; }
  void move(Dir dir);
  void grow();

 private:
  std::vector<int> cs;
  int sz;
  const Field &f;
};

class Game {
 public:
  Game(Size field_size);
  void move(Dir dir, bool *cookie_eaten);
  bool is_over() const { return over; }
  const Field &field() const { return f; }
  const Snake &snake() const { return s; }
  int cookie() const { return cooky; }
  int score() const { return scr; }

 private:
  void new_cookie();
  int random_free_cell() const;

  Field f;
  Snake s;
  int cooky;
  int scr;
  bool over;
  mutable std::mt19937 mt;
};

#endif  // GAME_H