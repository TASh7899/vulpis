#include "input.h"
#include "../../lua.hpp"
#include <iostream>
#include <functional>

namespace Input {
  Node* hitTest(Node* root, int x, int y) {
    if (!root) return nullptr;
    if (x < root->x || x > root->x + root->w || y < root->y || y > root->y + root->h) {
      return nullptr;
    }

    for (int i = (int)root->children.size() - 1; i >= 0; --i) {
      Node* target = hitTest(root->children[i], x, y);
      if (target) {
        return target;
      }
    }

    return root;
  }

  InputEvent process(const SDL_Event& event) {
    return InputEvent::fromSDL(event);
  }

  std::vector<Node*> resolveTarget(const InputEvent& ev, Node* root) {
    std::vector<Node*> chain;
    if (!root) return chain;

    if (ev.type == InputEvent::Type::CLICK || ev.type == InputEvent::Type::MOVE) {
      Node* t = hitTest(root, ev.x, ev.y);
      while (t) {
        chain.push_back(t);
        t = t->parent;
      }
    }

    return chain;
  }

  bool dispatchAction(lua_State* L, const InputEvent& ev, const std::vector<Node*>& chain) {
    if (!L) return false;

    if (ev.type == InputEvent::Type::CLICK) {
      for (Node* target : chain) {
        if (!target) continue;
        if (target->onClickRef != -2) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, target->onClickRef);

          if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            continue;
          }

          if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cout << "Input Error:" << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
          }

          return true;
        }
      }
    }

    return false;
  }

  void handleEvent(lua_State *L, SDL_Event &event, Node *root) {
    InputEvent ev = process(event);
    std::vector<Node*> chain = resolveTarget(ev, root);
    dispatchAction(L, ev, chain);
  }

}
