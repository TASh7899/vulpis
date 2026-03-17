#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include "../ui/ui.h"

namespace Input {
  extern uint32_t lastInputTime;
  // determine which node is under the mouse
  Node* hitTest(Node* root, int x, int y, Node* ignore = nullptr, float parentOffsetX = 0.0f, float parentOffsetY = 0.0f);
  // handle all type of events like mouse clicks
  void handleEvent(lua_State* L, SDL_Event& event, Node* root);

  void init();
  void updateState();
  Node* findFocusedNode(Node* root);

  void clearNodeState(Node* n);

}
