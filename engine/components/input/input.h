#pragma once
#include <SDL2/SDL.h>
#include "../ui/ui.h"

namespace Input {
  // determine which node is under the mouse
  Node* hitTest(Node* root, int x, int y);
  // handle all type of events like mouse clicks
  void handleEvent(lua_State* L, SDL_Event& event, Node* root);

}
