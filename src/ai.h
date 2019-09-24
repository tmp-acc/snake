#ifndef AI_H
#define AI_H

#include <vector>

#include "game.h"

class GameAI {
 public:
  GameAI(Game &game);
  bool next_move();

 private:
  bool is_reachable(int src, int dst) const;
  Dir find_move_dir() const;
  Dir follow_tail() const;
  Dir find_safe_way() const;

  Game &g;
};

#endif  // AI_H