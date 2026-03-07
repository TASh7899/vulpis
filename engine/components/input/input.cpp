#include "input.h"
#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <SDL_scancode.h>
#include <SDL_stdinc.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <lauxlib.h>
#include <lua.h>
#include <pthread.h>
#include <vector>
#include <cstring>

#include "../../scripting/regsitry.h"

namespace Input {
  static Node* draggedScrollbarNode = nullptr;
  static float dragOffsetY = 0.0f;
  static Node* activeDragNode = nullptr;
  static int dragInitialMouseX = 0;
  static int dragInitialMouseY = 0;

  static int numKeys = 0;
  static const Uint8* currentKeyStates = nullptr;
  static Uint8* previousKeyStates = nullptr;

  void init() {
    currentKeyStates = SDL_GetKeyboardState(&numKeys);

    previousKeyStates = new Uint8[numKeys];
    std::memset(previousKeyStates, 0, numKeys);
    SDL_StartTextInput();
  }

  void updateState() {
    if (currentKeyStates && previousKeyStates) {
      std::memcpy(previousKeyStates, currentKeyStates, numKeys);
    }
  }

  static int getTextIndexAtCoords(Node* n, int mx, int my) {
    if (!n || n->type != "text" || !n->font) return -1;
  
    int startX = n->x + n->paddingLeft - n->scrollX;
    std::vector<uint32_t> codepoints = Font::DecodeUTF8(n->text);
    float currentX = startX;

    for (size_t i = 0; i < codepoints.size(); i++ ) {
      float adv = n->font->GetLogicalAdvance(codepoints[i]);
      if (mx < currentX + (adv / 2.0)) {
        return i;
      }
      currentX += adv;
    }
    return codepoints.size();
  }


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



  void fireDragEvent(lua_State* L, int ref, int dx, int dy, int mx, int my, int textIndex) {
    if (ref != -2) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, dx); lua_pushinteger(L, dy);
        lua_pushinteger(L, mx); lua_pushinteger(L, my);
        lua_pushinteger(L, textIndex);
        if (lua_pcall(L, 5, 0, 0) != LUA_OK) {
          std::cout << "Drag Event Error" << lua_tostring(L, -1) << std::endl;
          lua_pop(L, -1);
        }
      } else lua_pop(L, -1);
    }
  }

  void fireMouseEvent(lua_State* L, int ref, int mx, int my, int textIndex, int clicks) {
    if (ref != -2) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
      if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, mx); lua_pushinteger(L, my);
        lua_pushinteger(L, textIndex);
        lua_pushinteger(L, clicks);
        if (lua_pcall(L, 4, 0, 0) != LUA_OK) {
          std::cout << "Mouse Event Error: " << lua_tostring(L, -1) << std::endl;
          lua_pop(L, 1);
        }
      } else lua_pop(L, 1);
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

  Node* findFocusedNode(Node* root) {
    if (!root) return nullptr;
    if (root->isFocused) return root;
    for (Node* c : root->children) {
      Node* f = findFocusedNode(c);
      if (f) return f;
    }
    return nullptr;
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

        if (activeDragNode->isDraggable) {
          activeDragNode->dragOffsetX = (float)dx;
          activeDragNode->dragOffsetY = (float)dy;
        }

        int textIdx = getTextIndexAtCoords(activeDragNode, mx, my);
        fireDragEvent(L, activeDragNode->onDragRef, dx, dy, mx, my, textIdx);
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
        Node* focusCheck = target;
        bool focusHandle = false;

        while (focusCheck) {
          if (focusCheck->isFocusable) {
            Node* focusedNode = findFocusedNode(root);
            if (focusedNode != focusCheck) {
              if (focusedNode && focusedNode->onBlurRef != -2) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, focusedNode->onBlurRef);
                if (lua_isfunction(L, -1)) lua_pcall(L, 0, 0, 0);
                else lua_pop(L, 1);
              }
              focusedNode = focusCheck;
              if (focusedNode->onFocusRef != -2) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, focusedNode->onFocusRef);
                if (lua_isfunction(L, -1)) lua_pcall(L, 0, 0, 0);
                else lua_pop(L, 1);
              }
            }
            focusHandle = true;
            break;
          }
          focusCheck = focusCheck->parent;
        }

        if (!focusHandle) {
          Node* focusedNode = findFocusedNode(root);
          if (focusedNode && focusedNode->onBlurRef != -2) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, focusedNode->onBlurRef);
            if (lua_isfunction(L, -1)) lua_pcall(L, 0, 0, 0);
            else lua_pop(L, 1);
          }
        }

        Node* dragCheck = target;
        while (dragCheck) {
          if (dragCheck->isDraggable || dragCheck->onDragRef != -2 || dragCheck->onDragStartRef != -2) {
            activeDragNode = dragCheck;
            activeDragNode->isDragging = true;
            dragInitialMouseX = mx;
            dragInitialMouseY = my;

            int textIndex = getTextIndexAtCoords(activeDragNode, mx, my);
            fireMouseEvent(L, activeDragNode->onDragStartRef, mx, my, textIndex, event.button.clicks);
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

    else if (event.type == SDL_KEYDOWN) {  
      Node* focusedNode = findFocusedNode(root);
      if (focusedNode && focusedNode->onKeyDownRef != -2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, focusedNode->onKeyDownRef);
        if (lua_isfunction(L, -1)) {
          lua_pushstring(L, SDL_GetKeyName(event.key.keysym.sym));
          SDL_Keymod mod = SDL_GetModState();
          lua_newtable(L);
          lua_pushboolean(L, mod & KMOD_CTRL); lua_setfield(L, -2, "ctrl");
          lua_pushboolean(L, mod & KMOD_SHIFT); lua_setfield(L, -2, "shift");
          lua_pushboolean(L, mod & KMOD_ALT); lua_setfield(L, -2, "alt");
          if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            std::cerr << "Key Down Error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
          }
        } else {
          lua_pop(L, 1);
        }
      }
    }

    else if (event.type == SDL_TEXTINPUT) {
      Node* focusedNode = findFocusedNode(root);
      if (focusedNode && focusedNode->onTextInputRef != -2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, focusedNode->onTextInputRef);
        if (lua_isfunction(L, -1)) {
          lua_pushstring(L, event.text.text);
          if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            std::cerr << "Text Input Error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
          }
        } else {
          lua_pop(L, 1);
        }
      }
    }
  }


  int l_isKeyHeld(lua_State* L) {
    SDL_Scancode code = SDL_GetScancodeFromName(luaL_checkstring(L, 1));
    lua_pushboolean(L, currentKeyStates && currentKeyStates[code]);
    return 1;
  }

  int l_isKeyJustPressed(lua_State* L) {
    SDL_Scancode code = SDL_GetScancodeFromName(luaL_checkstring(L, 1));
    lua_pushboolean(L, currentKeyStates && currentKeyStates[code] && !previousKeyStates[code]);
    return 1;
  }

  int l_setClipboardText(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    SDL_SetClipboardText(text);
    return 0;
  }

  int l_getClipboardText(lua_State* L) {
    if (SDL_HasClipboardText()) {
      char* text = SDL_GetClipboardText();
      lua_pushstring(L, text);
      SDL_free(text);
      return 1;
    }
    lua_pushstring(L, "");
    return 1;
  }

  AutoRegisterLua regSetClip("setClipboardText", l_setClipboardText);
  AutoRegisterLua regGetClip("getClipboardText", l_getClipboardText);

  AutoRegisterLua regKeyHeld("isKeyHeld", l_isKeyHeld);
  AutoRegisterLua regKeyPressed("isKeyJustPressed", l_isKeyJustPressed);
}
