#include "input.h"
#include <SDL_events.h>
#include <SDL_mouse.h>
#include <SDL_stdinc.h>
#include <algorithm>
#include <iostream>
#include <lua.h>
#include <vector>

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

  void processHover(lua_State* L, Node* node, const std::vector<Node*>& activePath) {
    if (!node) return;

    bool shouldBeHovered = std::find(activePath.begin(), activePath.end(), node) != activePath.end();

    if (shouldBeHovered && !node->isHovered) {
      node->isHovered = true;
      if (node->onMouseEnterRef != -2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, node->onMouseEnterRef);
        if (lua_isfunction(L, -1)) {
          if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cout << "Input Error (onMouseEnter): " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
          }
        } else {
          lua_pop(L, 1);
        }
      }
    } else if (!shouldBeHovered && node->isHovered) {
      node->isHovered = false;
      if (node->onMouseLeaveRef != -2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, node->onMouseLeaveRef);
        if (lua_isfunction(L, -1)) {
          if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cout << "Input Error (onMouseLeave): " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
          }
        } else {
          lua_pop(L, 1);
        }
      }
    }

    for (Node* child : node->children) {
      processHover(L, child, activePath);
    }
  }

  void handleEvent(lua_State *L, SDL_Event &event, Node *root) {
    if (event.type == SDL_MOUSEMOTION) {
      int mx = event.motion.x;
      int my = event.motion.y;

      Node* target = hitTest(root, mx, my);

      std::vector<Node*> activePath;
      Node* curr = target;
      while (curr) {
        activePath.push_back(curr);
        curr = curr->parent;
      }

      processHover(L, root, activePath);
    }

    else if (event.type == SDL_MOUSEBUTTONDOWN) {
      int mx = event.button.x;
      int my = event.button.y;
      Uint8 button = event.button.button;

      Node* target = hitTest(root, mx, my);

      while (target) {
        int refToCall = -2;

        if (button == SDL_BUTTON_LEFT) {
          refToCall = target->onClickRef;
        } else if (button == SDL_BUTTON_RIGHT) {
          refToCall = target->onRightClickRef;
        }

        if (refToCall != -2) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, refToCall);

          if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            target = target->parent;
            continue;
          }

          lua_pushnumber(L, mx);
          lua_pushnumber(L, my);

          if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            std::cout << "Input Error (Mouse Up): " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
          }
          break;
        }
        target = target->parent;
      }
    }
  }
}
