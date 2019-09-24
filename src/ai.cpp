#include <algorithm>
#include <vector>

#include "ai.h"

class Wave {
  enum { target, undefined = 888888, obstacle = 999999 };

 public:
  Wave(const Field &field)
      : data(field.size().area()),
        dirs({Dir::Left, Dir::Right, Dir::Up, Dir::Down}),
        f(field) {}

  void dump() const {
    int sz = f.size().area();
    for (int i = 0; i < sz; ++i) {
      if (data[i] == obstacle) {
        printf(" # ");
      } else if (data[i] == target) {
        printf(" @ ");
      } else if (data[i] == undefined) {
        printf(" . ");
      } else {
        printf("%2d ", data[i]);
      }
      if (i % f.size().width() == f.size().width() - 1) {
        printf("\n");
      }
    }
    printf("\n");
  }

  void reset(const Snake &snake, int dst) {
    size_t sz = f.size().area();
    for (size_t i = 0; i < sz; ++i) {
      data[i] = snake.contains(i, false) ? obstacle : undefined;
    }
    data[dst] = target;
  }

  template <typename Comparator>
  Dir find_move(int src, int val, Comparator comp) const {
    Dir res = Dir::Err;
    for (auto &dir : dirs) {
      int dst = src + f.move_value(dir);
      if (f.can_move(src, dir) && comp(data[dst], val) &&
          data[dst] < undefined) {
        val = data[dst];
        res = dir;
      }
    }
    return res;
  }

  Dir shortest_move(int src) const {
    return find_move(src, obstacle, std::less<int>());
  }

  Dir longest_move(int src) const {
    return find_move(src, -1, std::greater<int>());
  }
  void build_wave(int src, int dst, bool *dst_reached) {
    *dst_reached = false;
    std::vector<int> array;
    size_t i = 0;
    array.push_back(dst);
    while (i < array.size()) {
      auto cell = array[i++];
      for (auto &d : dirs) {
        auto m = f.move_value(d);
        auto next = cell + m;
        if (f.can_move(cell, d)) {
          if (next == src) {
            *dst_reached = true;
          }
          if (data[next] < obstacle) {
            if (data[next] > data[cell] + 1) {
              data[next] = data[cell] + 1;
            }
            if (std::find(array.begin(), array.end(), next) == array.end()) {
              array.push_back(next);
            }
          }
        }
      }
    }
  }

  bool is_tail_in_sight(const Snake &snake) {
    reset(snake, snake.tail());
    bool res;
    build_wave(snake.head(), snake.tail(), &res);
    return res;
  }

  void set_target(int pos) { data[pos] = target; }
  void set_obstacle(int pos) { data[pos] = obstacle; }
  void set_undefined(int pos) { data[pos] = undefined; }

 private:
  std::vector<int> data;
  const std::vector<Dir> dirs;
  const Field &f;
};

GameAI::GameAI(Game &game) : g(game) {}

bool GameAI::next_move() {
  Dir dir = find_move_dir();
  if (dir == Dir::Err) {
    return false;
  }
  bool cookie_eaten;
  g.move(dir, &cookie_eaten);
  return true;
}

Dir GameAI::find_move_dir() const {
  bool has_way_to_cookie = is_reachable(g.snake().head(), g.cookie());
  return has_way_to_cookie ? find_safe_way() : follow_tail();
}

bool GameAI::is_reachable(int src, int dst) const {
  bool res;
  Wave wave(g.field());
  wave.reset(g.snake(), dst);
  wave.build_wave(src, dst, &res);
  return res;
}

Dir GameAI::follow_tail() const {
  Wave wave(g.field());
  auto &s = g.snake();
  wave.reset(s, g.cookie());
  wave.set_target(s.tail());
  wave.set_obstacle(g.cookie());
  bool dummy;
  wave.build_wave(s.head(), s.tail(), &dummy);
  return wave.longest_move(s.head());
}

Dir GameAI::find_safe_way() const {
  Wave wave(g.field());
  Snake tmp_snake = g.snake();
  wave.reset(tmp_snake, g.cookie());
  Dir res = Dir::Err;
  while (true) {
    bool dummy;
    wave.build_wave(tmp_snake.head(), g.cookie(), &dummy);
    Dir move = wave.shortest_move(tmp_snake.head());
    if (res == Dir::Err) {
      res = move;
    }
    tmp_snake.move(move);
    if (tmp_snake.head() == g.cookie()) {
      tmp_snake.grow();
      break;
    } else {
      wave.set_obstacle(tmp_snake.head());
      wave.set_undefined(tmp_snake.cell(tmp_snake.size() - 1));
    }
  }
  // We always have to keep the tail reachable from the head
  if (wave.is_tail_in_sight(tmp_snake)) {  // TODO: check on every step of
                                           // the find_safe_way() algirithm
    return res;
  }
  return follow_tail();
}
