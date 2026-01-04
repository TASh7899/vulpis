#include "layout.h"
#include <algorithm>

namespace Layout {

void DefaultLayoutSolver::measure(Node* n) {

  for (Node* c : n->children) {
    DefaultLayoutSolver::measure(c);
  }

  int contentH = 0;
  int contentW = 0;

  if (n->type == "vbox") {
    for (Node* c : n->children) {
      int childH = c->h + c->marginBottom + c->marginTop;
      int childW = c->w + c->marginLeft + c->marginRight;

      contentH += childH;
      contentW = std::max(contentW, childW);
    }

    if (!n->children.empty()) {
      contentH += n->spacing * (n->children.size() - 1);
    }
  }
  else if (n->type == "hbox") {
    for (Node* c : n->children) {
      int childH = c->h + c->marginTop + c->marginBottom;
      int childW = c->w + c->marginLeft + c->marginRight;

      contentW += childW;
      contentH = std::max(contentH, childH);
    }
    if (!n->children.empty()) contentW += n->spacing * (n->children.size() - 1);
  }

  contentW += n->paddingLeft + n->paddingRight;
  contentH += n->paddingTop + n->paddingBottom;

  if (n->w == 0) {
    n->w = contentW;
  }
  if (n->h == 0) {
    n->h = contentH;
  }

  n->w = std::max(n->minWidth, std::min(n->w, n->maxWidth));
  n->h = std::max(n->minHeight, std::min(n->h, n->maxHeight));
}


void DefaultLayoutSolver::compute(Node* n, int x, int y) {
  n->x = x;
  n->y = y;

  int innerX = x + n->paddingLeft;
  int innerY = y + n->paddingTop;
  int innerW = n->w - n->paddingLeft - n->paddingRight;
  int innerH = n->h - n->paddingTop- n->paddingBottom;

  int usedSize = 0;
  float totalFlex = 0.0f;
  int childCount = n->children.size();

  bool isRow = (n->type == "hbox");

  for (Node* c : n->children) {
    totalFlex += c->flexGrow;
    if (isRow) {
      usedSize += c->w + c->marginLeft + c->marginRight;
    } else {
      usedSize += c->h + c->marginTop + c->marginBottom;
    }
  }
  if (childCount > 0) usedSize += n->spacing * (childCount - 1);

  int freeSpace = (isRow ? innerW : innerH) - usedSize;

  if (totalFlex > 0 && freeSpace > 0) {
    for (Node* c : n->children) {
      if (c->flexGrow > 0) {
        int add = (int)((c->flexGrow / totalFlex) * freeSpace);
        if (isRow) c->w += add;
        else c->h += add;
      }
    }

    usedSize = (isRow ? innerW : innerH);
    freeSpace = 0;
  }

  int startOffset = 0;
  int gap = n->spacing;

  if (totalFlex == 0) {
    if (n->justifyContent == Justify::Center) startOffset = ((isRow ? innerW : innerH) - usedSize) / 2;
    else if (n->justifyContent == Justify::End) startOffset = ((isRow ? innerW : innerH) - usedSize);
    else if (n->justifyContent == Justify::SpaceBetween && childCount > 1) {
      gap = ((isRow ? innerW : innerH) - (usedSize - (n->spacing*(childCount-1)))) / (childCount - 1);
    }
    else if (n->justifyContent == Justify::SpaceAround && childCount > 0) {
      int totalFree = (isRow ? innerW : innerH) - (usedSize - (n->spacing * (childCount - 1)));
      gap = totalFree / childCount;
      startOffset = gap / 2;
    }
    else if (n->justifyContent == Justify::SpaceEvenly && childCount > 0) {
      int childrenWidth = usedSize - (n->spacing * (childCount - 1));
      int totalFree = (isRow ? innerW : innerH) - childrenWidth;
      gap = totalFree / (childCount + 1);
      startOffset = gap;
    }
  }

  int cx = innerX + (isRow ? startOffset : 0);
  int cy = innerY + (isRow ? 0 : startOffset);

  for (Node* c : n->children) {
    int childX = cx + c->marginLeft;
    int childY = cy + c->marginTop;

    if (isRow) {
      if (n->alignItems == Align::Center) childY = innerY + (innerH - c->h)/2;
      if (n->alignItems == Align::End) childY = innerY + innerH - c->h - c->marginBottom;
      if (n->alignItems == Align::Stretch) c->h = innerH - c->marginTop - c->marginBottom;
    } else {
      if (n->alignItems == Align::Center) childX = innerX + (innerW - c->w)/2;
      if (n->alignItems == Align::End) childX = innerX + innerW - c->w - c->marginRight;
      if (n->alignItems == Align::Stretch) c->w = innerW - c->marginLeft - c->marginRight;
    }
    compute(c, childX, childY);

    if (isRow) {
      cx += c->w + c->marginLeft + c->marginRight + gap;
    } else {
      cy += c->h + c->marginTop + c->marginBottom + gap;
    }
  }
}

void DefaultLayoutSolver::solve(Node* root, Size viewport) {
  resolveStyles(root, viewport.w, viewport.h);
  measure(root);
  compute(root, 0, 0);
}

}
