#include "layout.h"
#include "yoga/YGNode.h"
#include "yoga/YGNodeLayout.h"
#include "yoga/YGNodeStyle.h"
#include <yoga/Yoga.h>
#include <vector>

namespace Layout {

  class YogaSolver : public LayoutSolver {
    public:
      void solve(Node* root, Size viewport) override {
        if (!root) return;

        YGNodeRef yogaRoot = buildTree(root);
        YGNodeStyleSetWidth(yogaRoot, (float)viewport.w);
        YGNodeStyleSetHeight(yogaRoot, (float)viewport.h);

        YGNodeCalculateLayout(yogaRoot, (float)viewport.w, (float)viewport.h, YGDirectionLTR);
        applyLayout(root, yogaRoot, 0, 0);
        YGNodeFreeRecursive(yogaRoot);
      }
    private:
      YGNodeRef buildTree(Node* n) {
        YGNodeRef yogaNode = YGNodeNew();
        if (n->type == "vstack") {
          YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionColumn);
        } else if (n->type == "hstack") {
          YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionRow);
        }

        if (n->flexGrow > 0) {
          YGNodeStyleSetFlexGrow(yogaNode, n->flexGrow);
        }

        if (n->widthStyle.type == PERCENT) {
          YGNodeStyleSetWidthPercent(yogaNode, n->widthStyle.value);
        } else if (n->widthStyle.value > 0) {
          YGNodeStyleSetWidth(yogaNode, n->widthStyle.value);
        }

        if (n->heightStyle.type == PERCENT) {
          YGNodeStyleSetHeightPercent(yogaNode, n->heightStyle.value);
        } else if (n->heightStyle.value > 0) {
          YGNodeStyleSetHeight(yogaNode, n->heightStyle.value);
        }

        if (n->minWidth > 0) YGNodeStyleSetMinWidth(yogaNode, n->minWidth);
        if (n->maxWidth < 99999) YGNodeStyleSetMaxWidth(yogaNode, n->maxWidth);
        if (n->minHeight > 0) YGNodeStyleSetMinHeight(yogaNode, n->minHeight);
        if (n->maxHeight < 99999) YGNodeStyleSetMaxHeight(yogaNode, n->maxHeight);

        YGNodeStyleSetAlignItems(yogaNode, mapAlign(n->alignItems));
        YGNodeStyleSetJustifyContent(yogaNode, mapJustify(n->justifyContent));

        YGNodeStyleSetPadding(yogaNode, YGEdgeTop, (float)n->paddingTop);
        YGNodeStyleSetPadding(yogaNode, YGEdgeBottom, (float)n->paddingBottom);
        YGNodeStyleSetPadding(yogaNode, YGEdgeLeft, (float)n->paddingLeft);
        YGNodeStyleSetPadding(yogaNode, YGEdgeRight, (float)n->paddingRight);

        YGNodeStyleSetMargin(yogaNode, YGEdgeTop, (float)n->marginTop);
        YGNodeStyleSetMargin(yogaNode, YGEdgeBottom, (float)n->marginBottom);
        YGNodeStyleSetMargin(yogaNode, YGEdgeLeft, (float)n->marginLeft);
        YGNodeStyleSetMargin(yogaNode, YGEdgeRight, (float)n->marginRight);

        if (n->spacing > 0) {
          YGNodeStyleSetGap(yogaNode, YGGutterAll, (float)n->spacing);
        }

        for (size_t i = 0; i < n->children.size(); i++) {
          YGNodeRef childYoga = buildTree(n->children[i]);
          YGNodeInsertChild(yogaNode, childYoga, i);
        }

        return yogaNode;
      }

      void applyLayout(Node* n, YGNodeRef yogaNode, float parentX, float parentY) {
        float relX = YGNodeLayoutGetLeft(yogaNode);
        float relY = YGNodeLayoutGetTop(yogaNode);

        n->x = parentX + relX;
        n->y = parentY + relY;
        n->w = YGNodeLayoutGetWidth(yogaNode);
        n->h = YGNodeLayoutGetHeight(yogaNode);

        for (size_t i = 0; i < n->children.size(); i++) {
          YGNodeRef childNode = YGNodeGetChild(yogaNode, i);
          applyLayout(n->children[i], childNode, n->x, n->y);
        }
      }

      YGAlign mapAlign(Align a) {
        switch (a) {
          case Align::Center: return YGAlignCenter;
          case Align::End: return YGAlignFlexEnd;
          case Align::Stretch: return YGAlignStretch;
          default: return YGAlignFlexStart;
        }
      }

      YGJustify mapJustify(Justify j) {
        switch (j) {
          case Justify::Center: return YGJustifyCenter;
          case Justify::End: return YGJustifyFlexEnd;
          case Justify::SpaceBetween: return YGJustifySpaceBetween;
          case Justify::SpaceAround: return YGJustifySpaceAround;
          case Justify::SpaceEvenly: return YGJustifySpaceEvenly;
          default: return YGJustifyFlexStart;
        }
      }

  };

  LayoutSolver* createYogaSolver() {
    return new YogaSolver();
  }

}
