#include <chrono>
#include <iostream>
#include <regex>
#include <sstream>

#include "ai.h"
#include "game.h"
#include "gif.h"
#include "render.h"

void generate_gif(const Size& sz, size_t max_frames) {
  std::ostringstream ss;
  ss << "snake" << sz.width() << "x" << sz.height() << ".gif";
  gif::FileDevice dev(ss.str());
  dev.open(gif::FileDevice::write_only);

  Game game(sz);
  GameAI ai(game);
  GameRender r(game);

  int max_score = sz.area() - 3;
  int max_delay = 15;
  int c = 0;
  do {
    if (c++ > max_frames) {
      break;
    }
    int delay = double(max_score -  game.score()) / max_score * max_delay;
    r.draw_frame(delay);
    ai.next_move();
  } while (!game.is_over());

  r.draw_frame(100);
  r.draw_game_over(100);
  r.save(dev);
}

struct Params {
  Size field_size = {13, 8};
  int max_frames = 3000;

  void parse(int argc, char** argv) {
    std::string args;
    for (int i = 1; i < argc; i++) {
      args += argv[i];
    }
    std::smatch sz_match;
    std::regex_search(args, sz_match, std::regex("size\\s*=\\s*(\\d+)x(\\d+)"));
    if (sz_match.size() == 3) {
      field_size = Size(std::max(std::stoi(sz_match[1]), 4),
                        std::max(std::stoi(sz_match[2]), 3));
    }
    std::smatch mf_match;
    std::regex_search(args, mf_match, std::regex("maxframes\\s*=\\s*(\\d+)"));
    if (mf_match.size() == 2) {
      max_frames = std::stoi(mf_match[1]);
    }
  }
};

int main(int argc, char** argv) {
  Params params;
  params.parse(argc, argv);
  generate_gif(params.field_size, params.max_frames);
  return 0;
}
