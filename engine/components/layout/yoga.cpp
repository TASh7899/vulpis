#include "layout.h"
#include "yoga/YGConfig.h"
#include "yoga/YGNode.h"
#include "yoga/YGNodeLayout.h"
#include "yoga/YGNodeStyle.h"
#include <algorithm>
#include <cmath>
#include <yoga/Yoga.h>
#include <vector>
#include "../ui/ui.h"

namespace Layout {

  YGSize textMeasure(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
    Node* n = (Node*)YGNodeGetContext(node);
    YGSize size = {0, 0};
    if (!n || !n->font || n->text.empty()) {
      return size;
    }

    float maxWidth = 999999.0f;
    if (widthMode == YGMeasureModeExactly) {
      maxWidth = width;
    } else if (widthMode == YGMeasureModeAtMost) {
      maxWidth = width;
    }

    TextLayoutResult res = calculateTextLayout(n->text, n->font, maxWidth);

    size.width = std::ceil(res.width);
    size.height = std::ceil(res.height);

    if (heightMode == YGMeasureModeExactly) {
      size.height = height;
    } else if (heightMode == YGMeasureModeAtMost) {
      size.height = std::min(height, size.height);
    }

    return size;
  }

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
        YGNodeSetContext(yogaNode, n);

        if (n->position == PositionType::Absolute) {
          YGNodeStyleSetPositionType(yogaNode, YGPositionTypeAbsolute);
        } else {
          YGNodeStyleSetPositionType(yogaNode, YGPositionTypeRelative);
        }

        if (n->type == "text") {
          YGNodeSetMeasureFunc(yogaNode, textMeasure);
          YGNodeStyleSetFlexShrink(yogaNode, 0.0f);
          YGNodeStyleSetFlexGrow(yogaNode, 0.0f);
        }

        if (n->type == "vbox") {
          YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionColumn);
        } else if (n->type == "hbox") {
          YGNodeStyleSetFlexDirection(yogaNode, YGFlexDirectionRow);
        }

        if (n->flexGrow > 0) {
          YGNodeStyleSetFlexGrow(yogaNode, n->flexGrow);
        }

        if (n->flexShrink >= 0) {
          YGNodeStyleSetFlexShrink(yogaNode, n->flexShrink);
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

        if (n->type == "text" && !n->children.empty()) {
          std::cerr << "ERROR: Text Node (text='" 
            << n->text.substr(0, 20) << (n->text.length() > 20 ? "..." : "") 
            << "') cannot have children.\n";
            exit(1);
        } else {
          for (size_t i = 0; i < n->children.size(); i++) {
            YGNodeRef childYoga = buildTree(n->children[i]);
            YGNodeInsertChild(yogaNode, childYoga, i);
          }
        }

        return yogaNode;
      }

      void applyLayout(Node* n, YGNodeRef yogaNode, float parentX, float parentY) {

        if (!n || !yogaNode) return;

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
