#pragma once
#include "../ui/ui.h"

namespace Layout {

  struct Size {
    int w;
    int h;
  };

  class LayoutSolver {
    public:
      virtual ~LayoutSolver() = default;
      virtual void solve(Node* root, Size viewport) = 0;
  };

  class DefaultLayoutSolver: public LayoutSolver {
    public:
      void solve(Node* root, Size viewport) override;
    private:
      void measure(Node* n);
      void compute(Node* n, int x, int y);
  };
  
  LayoutSolver* createYogaSolver();

}
