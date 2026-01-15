#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include "../../lua.hpp"
#include "../ui/ui.h"

namespace Input {
  struct InputEvent {
    enum class Type { UNKNOWN, CLICK, MOVE, KEY };
    Type type = Type::UNKNOWN;
    int x = 0;
    int y = 0;
    int button = 0;
    int key = 0;
    SDL_Event raw;

    static InputEvent fromSDL(const SDL_Event& e) {
      InputEvent ev;
      ev.raw = e;
      switch (e.type) {
        case SDL_MOUSEBUTTONDOWN:
          ev.type = Type::CLICK;
          ev.x = e.button.x;
          ev.y = e.button.y;
          ev.button = e.button.button;
          break;
        case SDL_MOUSEMOTION:
          ev.type = Type::MOVE;
          ev.x = e.motion.x;
          ev.y = e.motion.y;
          break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          ev.type = Type::KEY;
          ev.key = e.key.keysym.sym;
          break;
        default:
          ev.type = Type::UNKNOWN;
      }
      return ev;
    }
  };

  // determine which node is under the mouse
  Node* hitTest(Node* root, int x, int y);

  // Convert raw SDL event into InputEvent
  InputEvent process(const SDL_Event& event);

  // Resolve the target node chain (target -> parents) for an InputEvent
  std::vector<Node*> resolveTarget(const InputEvent& ev, Node* root);

  // Dispatch the action by iterating the chain and invoking Lua handlers
  // Returns true if the event was handled
  bool dispatchAction(lua_State* L, const InputEvent& ev, const std::vector<Node*>& chain);

  // Backwards-compatible convenience that does full convert+resolve+dispatch
  void handleEvent(lua_State* L, SDL_Event& event, Node* root);

}
