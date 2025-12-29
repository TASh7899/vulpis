#pragma once
#include "../ui/ui.h"

namespace Layout {
  void measure(Node* n, bool isRoot = false);
  void compute(Node* n, int x, int y);
}
