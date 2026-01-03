#include "input.h"
#include <iostream>
#include <lua.h>

namespace Input {
  Node* hitTest(Node* root, int x, int y) {
    if (!root) return nullptr;
    if (x < root->x || x > root->x + root->w || y < root->y || y > root->y + root->h) {
      return nullptr;
    }

    for (int i = root->children.size() - 1; i >= 0; --i) {
      Node* target = hitTest(root->children[i], x, y);
      if (target) {
        return target;
      }
    }

    return root;
  }

  void handleEvent(lua_State *L, SDL_Event &event, Node *root) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
      int mx = event.button.x;
      int my = event.button.y;

      Node* target = hitTest(root, mx, my);

      while (target) {
        if (target->onClickRef != -2) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, target->onClickRef);

          if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            target = target->parent;
            continue;

          }
          if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cout << "Input Error:" << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
          }

          break;
        }
        target = target->parent;
      }
    }
  }
}
