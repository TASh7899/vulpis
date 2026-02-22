#include "input.h"
#include <SDL_events.h>
#include <SDL_mouse.h>
#include <SDL_stdinc.h>
#include <algorithm>
#include <iostream>
#include <lua.h>
#include <vector>

namespace Input {
  static Node* draggedScrollbarNode = nullptr;
  static float dragOffsetY = 0.0f;
  static Node* activeDragNode = nullptr;
  static int dragInitialMouseX = 0;
  static int dragInitialMouseY = 0;


  Node* hitTest(Node* root, int x, int y, Node* ignore) {
    if (!root || root == ignore) return nullptr;

    float currentX = root->x + root->dragOffsetX;
    float currentY = root->y + root->dragOffsetY;

    if (x < currentX || x > currentX + root->w || y < currentY || y > currentY + root->h) {
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

  void fireLuaEvent(lua_State* L, int ref, int dx = 0, int dy = 0) {
    if (ref != -2) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, dx);
        lua_pushinteger(L, dy);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
          std::cout << "Drag Event Error: " << lua_tostring(L, -1) << std::endl;
          lua_pop(L, 1);
        }
      } else {
        lua_pop(L, 1);
      }
    }
  }


  void fireDragEndEvent(lua_State* L, int ref, const std::string& dropId, int dx, int dy) {
    if (ref != -2) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      if (lua_isfunction(L, -1)) {
        if (dropId.empty()) lua_pushnil(L); 
        else lua_pushstring(L, dropId.c_str());

        lua_pushinteger(L, dx);
        lua_pushinteger(L, dy);

        if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
          std::cout << "Drag End Error: " << lua_tostring(L, -1) << std::endl;
          lua_pop(L, 1);
        }
      } else {
        lua_pop(L, 1);
      }
    }
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

      if (draggedScrollbarNode) {
        draggedScrollbarNode->scrollbarTimer = 1.5f;
        ScrollbarMetrics sb = draggedScrollbarNode->getScrollbarMetrics();
        if (sb.isVisible) {
          float localY = (my - dragOffsetY) - sb.trackY;
          float availableTrackSpace = sb.trackH - sb.thumbH;

          if (availableTrackSpace > 0) {
            float scrollPct = (localY - (sb.thumbH / 2.0f)) / availableTrackSpace;
            draggedScrollbarNode->targetScrollY = std::clamp(scrollPct * sb.maxScrollY, 0.0f, sb.maxScrollY);
          }
        }
        return;
      }

      if (activeDragNode && activeDragNode->isDragging) {
        int dx = mx - dragInitialMouseX;
        int dy = my - dragInitialMouseY;

        activeDragNode->dragOffsetX = (float)dx;
        activeDragNode->dragOffsetY = (float)dy;

        fireLuaEvent(L, activeDragNode->onDragRef, dx, dy);
      }

      Node* target = hitTest(root, mx, my);
      std::vector<Node*> activePath;
      Node* curr = target;
      while (curr) {
        activePath.push_back(curr);
        curr = curr->parent;
      }

      processHover(L, root, activePath);
    }

    else if (event.type == SDL_MOUSEBUTTONUP) {
      if (event.button.button == SDL_BUTTON_LEFT) {
        draggedScrollbarNode = nullptr;

        if (activeDragNode) {
          int finalDx = event.button.x - dragInitialMouseX;
          int finalDy = event.button.y - dragInitialMouseY;

          Node* dropTarget = hitTest(root, event.button.x, event.button.y, activeDragNode);
          std::string dropId = "";

          while (dropTarget) {
            if (!dropTarget->id.empty()) {
              dropId = dropTarget->id;
              break;
            }
            dropTarget = dropTarget->parent;
          }

          fireDragEndEvent(L, activeDragNode->onDragEndRef, dropId, finalDx, finalDy);

          activeDragNode->isDragging = false;
          activeDragNode->dragOffsetX = 0.0f;
          activeDragNode->dragOffsetY = 0.0f;
          activeDragNode = nullptr;
        }
      }

    }

    else if (event.type == SDL_MOUSEWHEEL) {
      int mx, my;
      SDL_GetMouseState(&mx, &my);

      Node* target = hitTest(root, mx, my);

      while (target) {
        if (target->overflowScroll) {
          float maxScrollY = std::max(0.0f, target->contentH - target->h);
          float maxScrollX = std::max(0.0f, target->contentW - target->w);

          if (maxScrollY > 0 || maxScrollX > 0) {
            float scrollSpeed = 40.0f;
            target->scrollbarTimer = 1.5f;

            target->targetScrollY -= event.wheel.y * scrollSpeed;
            target->targetScrollX -= event.wheel.x * scrollSpeed;

            target->targetScrollX = std::clamp(target->targetScrollX, 0.0f, maxScrollX);
            target->targetScrollY = std::clamp(target->targetScrollY, 0.0f, maxScrollY);

            break;
          }
        }
        target = target->parent;
      }
    }

    else if (event.type == SDL_MOUSEBUTTONDOWN) {
      int mx = event.button.x;
      int my = event.button.y;
      Uint8 button = event.button.button;

      Node* target = hitTest(root, mx, my);
      bool eventConsumed = false;

      if (button == SDL_BUTTON_LEFT) {
        Node* curr = target;
        while (curr) {
          ScrollbarMetrics sb = curr->getScrollbarMetrics();

          if (sb.isVisible) {
            if (mx >= sb.trackX && mx <= sb.trackX + sb.trackW &&
                my >= sb.trackY && my <= sb.trackY + sb.trackH) {

              draggedScrollbarNode = curr;

              float localY = my - sb.trackY;
              float availableTrackSpace = sb.trackH - sb.thumbH;

              if (my >= sb.thumbY && my <= sb.thumbY + sb.thumbH) {
                dragOffsetY = my - (sb.thumbY + (sb.thumbH / 2.0f));
              } else {
                dragOffsetY = 0.0f; // Clicked empty track, snap center
              }

              if (availableTrackSpace > 0) {
                float scrollPct = (localY - (sb.thumbH / 2.0f)) / availableTrackSpace;
                curr->targetScrollY = std::clamp(scrollPct * sb.maxScrollY, 0.0f, sb.maxScrollY);
              }

              eventConsumed = true;
              break;
            }
          }
          curr = curr->parent;
        }
      }

      if (!eventConsumed && button == SDL_BUTTON_LEFT) {
        Node* dragCheck = target;
        while (dragCheck) {
          if (dragCheck->isDraggable) {
            activeDragNode = dragCheck;
            activeDragNode->isDragging = true;
            dragInitialMouseX = mx;
            dragInitialMouseY = my;

            fireLuaEvent(L, activeDragNode->onDragStartRef);
            eventConsumed = true;
            break; // Stop bubbling up
          }
          dragCheck = dragCheck->parent;
        }
      }

      if (!eventConsumed) {

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
}
