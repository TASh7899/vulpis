#include "input.h"
#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <SDL_scancode.h>
#include <SDL_stdinc.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <lauxlib.h>
#include <lua.h>
#include <algorithm>
#include <pthread.h>
#include <sys/types.h>
#include <vector>
#include <cstring>

#include "../../scripting/regsitry.h"

namespace Input {
  static Node* draggedScrollbarNode = nullptr;
  static float dragOffsetY = 0.0f;
  static Node* activeDragNode = nullptr;
  static int dragInitialMouseX = 0;
  static int dragInitialMouseY = 0;

  uint32_t lastInputTime = 0;

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
    if (!n || n->type != "text" || !n->font || n->computedLines.empty()) return -1;
  
    float contentWidth = n->w - (n->paddingLeft + n->paddingRight);
    const std::vector<uint32_t>& codepoints = n->codepoints;

    float startX = n->x + n->paddingLeft - n->scrollX + n->cachedOffsetX;
    float startY = n->y + n->paddingTop - n->scrollY + n->cachedOffsetY;

    float localY = my - startY;
    int lineIdx = std::floor(localY / n->computedLineHeight);

    if (lineIdx < 0) lineIdx = 0;
    if (lineIdx >= (int)n->computedLines.size()) lineIdx = n->computedLines.size() - 1;
    
    const TextLine& line = n->computedLines[lineIdx];

    float lineXOffset = 0;
    if (n->wordWrap) {
      if (n->textAlign == TextAlign::Center) lineXOffset = (contentWidth - line.width) / 2.0f;
      else if (n->textAlign == TextAlign::Right) lineXOffset = contentWidth - line.width;
    }

    // find the character in that line (X axis)
    float currentX = startX + lineXOffset;

    for (uint32_t i = 0; i < line.count; i++ ) {
      uint32_t charIdx = line.startIndex + i;
      float adv = n->font->GetLogicalAdvance(codepoints[charIdx]);
      if (mx < currentX + (adv / 2.0f)) {
        return charIdx;
      }
      currentX += adv;
    }
    return line.startIndex + line.count;
  }


  Node* hitTest(Node* root, int mx, int my, Node* ignore, float parentOffsetX, float parentOffsetY) {
    if (!root || root == ignore) return nullptr;

    float totalOffsetX = parentOffsetX;
    float totalOffsetY = parentOffsetY;

    totalOffsetX += root->dragOffsetX;
    totalOffsetY += root->dragOffsetY;

    float screenX = root->x + totalOffsetX;
    float screenY = root->y + totalOffsetY;

    bool inBounds = (mx >= screenX && mx <= screenX + root->w &&
                    my >= screenY && my <= screenY + root->h);

    if (root->overflowHidden && !inBounds) {
      return nullptr;
    }

    for (int i = root->children.size() - 1; i >= 0; --i) {
      Node* target = hitTest(root->children[i], mx, my, ignore, totalOffsetX - root->scrollX, totalOffsetY - root->scrollY);
      if (target) {
        return target;
      }
    }

    if (inBounds) {
      return root;
    }

    return nullptr;
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

  static std::vector<Node*> lastHoveredPath;

  void processHover(lua_State* L, const std::vector<Node*>& activePath) {
    // 1. Fire onMouseLeave for nodes that are no longer hovered
    for (Node* oldNode : lastHoveredPath) {
      if (std::find(activePath.begin(), activePath.end(), oldNode) == activePath.end()) {
        oldNode->isHovered = false;
        oldNode->makePaintDirty();
        if (oldNode->onMouseLeaveRef != -2) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, oldNode->onMouseLeaveRef);
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
    }

    // 2. Fire onMouseEnter for nodes that are newly hovered
    for (Node* newNode : activePath) {
      if (std::find(lastHoveredPath.begin(), lastHoveredPath.end(), newNode) == lastHoveredPath.end()) {
        newNode->isHovered = true;
        newNode->makePaintDirty();
        if (newNode->onMouseEnterRef != -2) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, newNode->onMouseEnterRef);
          if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
              std::cout << "Input Error (onMouseEnter): " << lua_tostring(L, -1) << std::endl;
              lua_pop(L, 1);
            }
          } else {
            lua_pop(L, 1);
          }
        }
      }
    }

    // 3. Save the current path for the next frame
    lastHoveredPath = activePath;
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

    if (event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT || event.type == SDL_MOUSEBUTTONDOWN) {
      lastInputTime = SDL_GetTicks();
    }

    if (event.type == SDL_MOUSEMOTION) {
      int mx = event.motion.x;
      int my = event.motion.y;

      if (draggedScrollbarNode) {
        draggedScrollbarNode->scrollbarTimer = 1.5f;
        ScrollbarMetrics sb = draggedScrollbarNode->getScrollbarMetrics();

        if (sb.isVisible) {
          float screenTrackY = sb.trackY + draggedScrollbarNode->cachedOffsetY;

          float localY = (my - dragOffsetY) - screenTrackY;
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
          activeDragNode->makePaintDirty();
          activeDragNode->dragOffsetX = (float)dx;
          activeDragNode->dragOffsetY = (float)dy;
          activeDragNode->makePaintDirty();
        }

        int textIdx = getTextIndexAtCoords(activeDragNode, mx, my);

        // selection support when mouse is in motion and dragging
        if (activeDragNode->allowSelection && activeDragNode->type == "text") {
          activeDragNode->selectionEnd = textIdx;
          activeDragNode->cursorPosition = textIdx;
          activeDragNode->makePaintDirty();
        }

        fireDragEvent(L, activeDragNode->onDragRef, dx, dy, mx, my, textIdx);
      }

      Node* target = hitTest(root, mx, my);
      std::vector<Node*> activePath;
      Node* curr = target;
      while (curr) {
        activePath.push_back(curr);
        curr = curr->parent;
      }

      processHover(L, activePath);
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

          if (sb.isVisible && curr->scrollbarOpacity > 0.0f) {
            float screenTrackX = sb.trackX + curr->cachedOffsetX;
            float screenTrackY = sb.trackY + curr->cachedOffsetY;

            if (mx >= screenTrackX && mx <= screenTrackX + sb.trackW &&
                my >= screenTrackY && my <= screenTrackY + sb.trackH) {

              draggedScrollbarNode = curr;

              float localY = my - sb.trackY;
              float availableTrackSpace = sb.trackH - sb.thumbH;
              float screenThumbY = sb.thumbY + curr->cachedOffsetY;

              if (my >= screenThumbY && my <= screenThumbY + sb.thumbH) {
                dragOffsetY = my - (screenThumbY + (sb.thumbH / 2.0f));
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
              if (focusedNode) {
                focusedNode->isFocused = false;
                if (focusedNode && focusedNode->onBlurRef != -2) {
                  lua_rawgeti(L, LUA_REGISTRYINDEX, focusedNode->onBlurRef);
                  if (lua_isfunction(L, -1)) lua_pcall(L, 0, 0, 0);
                  else lua_pop(L, 1);
                }
              }
              focusCheck->isFocused = true;
              if (focusCheck->onFocusRef != -2) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, focusCheck->onFocusRef);
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
          if (focusedNode) {
            focusedNode->isFocused = false; 

            if (focusedNode->onBlurRef != -2) {
              lua_rawgeti(L, LUA_REGISTRYINDEX, focusedNode->onBlurRef);
              if (lua_isfunction(L, -1)) {
                lua_pcall(L, 0, 0, 0);
              } else {
                lua_pop(L, 1);
              }
            }
          }
        }
        Node* dragCheck = target;
        while (dragCheck) {
          // Only trigger selection drag logic if it is explicitly a text node!
          if (dragCheck->isDraggable || dragCheck->onDragRef != -2 || dragCheck->onDragStartRef != -2 || (dragCheck->allowSelection && dragCheck->type == "text")) {
            activeDragNode = dragCheck;
            activeDragNode->isDragging = true;
            activeDragNode->lastCursorPosition = -1;
            dragInitialMouseX = mx;
            dragInitialMouseY = my;

            int textIndex = getTextIndexAtCoords(activeDragNode, mx, my);

            if (activeDragNode->allowSelection && activeDragNode->type == "text") {
              SDL_Keymod mod = SDL_GetModState();
              bool isShift = mod & KMOD_SHIFT;

              if (event.button.clicks >= 3) {
                // Triple click: Select All
                activeDragNode->selectionStart = 0;
                activeDragNode->selectionEnd = activeDragNode->codepoints.size();
                activeDragNode->cursorPosition = activeDragNode->codepoints.size();
              } else if (event.button.clicks == 2) {
                // Double click: Select Word
                int start = textIndex;
                int end = textIndex;
                const auto& cps = activeDragNode->codepoints;
                int maxLen = (int)cps.size();

                while (start > 0 && cps[start - 1] != ' ' && cps[start - 1] != '\n') start--;
                while (end < maxLen && cps[end] != ' ' && cps[end] != '\n') end++;

                activeDragNode->selectionStart = start;
                activeDragNode->selectionEnd = end;
                activeDragNode->cursorPosition = end;
              } else if (isShift) {
                // Shift + Click expands the current selection
                if (activeDragNode->selectionStart == -1) {
                  activeDragNode->selectionStart = (activeDragNode->cursorPosition != -1) ? activeDragNode->cursorPosition : 0;
                }
                activeDragNode->selectionEnd = textIndex;
                activeDragNode->cursorPosition = textIndex;
              } else {
                // Standard single click
                activeDragNode->selectionStart = textIndex;
                activeDragNode->selectionEnd = textIndex;
                activeDragNode->cursorPosition = textIndex;
              }
              activeDragNode->makePaintDirty();
            }

            fireMouseEvent(L, activeDragNode->onDragStartRef, mx, my, textIndex, event.button.clicks);

            if (activeDragNode->isDraggable || activeDragNode->onDragRef != -2 || activeDragNode->onDragStartRef != -2) {
              eventConsumed = true;
            }

            break;
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
      if (focusedNode) {
        if (focusedNode->allowSelection && focusedNode->type == "text") {
          SDL_Keymod mod = SDL_GetModState();
          bool isCtrl = (mod & KMOD_CTRL) || (mod & KMOD_GUI);

          if (isCtrl && event.key.keysym.sym == SDLK_c) {
            int selMin = std::min(focusedNode->selectionStart, focusedNode->selectionEnd);
            int selMax = std::max(focusedNode->selectionStart, focusedNode->selectionEnd);

            if (selMin >= 0 && selMax >= 0 && selMin != selMax) {
              std::string clipboardData = "";
              for (int i = selMin; i < selMax && i < (int)focusedNode->codepoints.size(); i++) {
                uint32_t cp = focusedNode->codepoints[i];
                if (cp <= 0x7F) { clipboardData += (char)cp; } 
                else if (cp <= 0x7FF) {
                  clipboardData += (char)(0xC0 | ((cp >> 6) & 0x1F));
                  clipboardData += (char)(0x80 | (cp & 0x3F));
                } else if (cp <= 0xFFFF) {
                  clipboardData += (char)(0xE0 | ((cp >> 12) & 0x0F));
                  clipboardData += (char)(0x80 | ((cp >> 6) & 0x3F));
                  clipboardData += (char)(0x80 | (cp & 0x3F));
                } else {
                  clipboardData += (char)(0xF0 | ((cp >> 18) & 0x07));
                  clipboardData += (char)(0x80 | ((cp >> 12) & 0x3F));
                  clipboardData += (char)(0x80 | ((cp >> 6) & 0x3F));
                  clipboardData += (char)(0x80 | (cp & 0x3F));
                }
              }
              SDL_SetClipboardText(clipboardData.c_str());
            }
          }
          else if (isCtrl && event.key.keysym.sym == SDLK_a) {
            focusedNode->selectionStart = 0;
            focusedNode->selectionEnd = focusedNode->codepoints.size();
            focusedNode->cursorPosition = focusedNode->codepoints.size();
            focusedNode->makePaintDirty();
          }

        }
      }

      if (focusedNode && focusedNode->onKeyDownRef != -2) {
        focusedNode->lastCursorPosition = -1;
        lua_rawgeti(L, LUA_REGISTRYINDEX, focusedNode->onKeyDownRef);
        if (lua_isfunction(L, -1)) {
          lua_pushstring(L, SDL_GetKeyName(event.key.keysym.sym));
          SDL_Keymod mod = SDL_GetModState();
          lua_newtable(L);
          lua_pushboolean(L, mod & KMOD_CTRL); lua_setfield(L, -2, "ctrl");
          lua_pushboolean(L, mod & KMOD_SHIFT); lua_setfield(L, -2, "shift");
          lua_pushboolean(L, mod & KMOD_ALT); lua_setfield(L, -2, "alt");
          lua_pushboolean(L, mod & KMOD_GUI); lua_setfield(L, -2, "gui");
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
        focusedNode->lastCursorPosition = -1;
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




  void clearNodeState(Node *n) {
    if (activeDragNode == n) activeDragNode = nullptr;
    if (draggedScrollbarNode == n) draggedScrollbarNode = nullptr;

    lastHoveredPath.erase(std::remove(lastHoveredPath.begin(), lastHoveredPath.end(), n), lastHoveredPath.end());
  }


}
