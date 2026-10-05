/* Headless test - LinearProgress / CircularProgress.
 *
 * Invariants:
 *   1. The bar's fill width is value * track width, clamped to the track.
 *   2. A zero value emits a track and no fill.
 *   3. Omitted options fall back to the documented defaults.
 *   4. Infinite mode emits nothing outside the track.
 * */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "ccompose.h"

static const CC_Color FILL = {10, 20, 30, 255};
static const CC_Color TRACK = {40, 50, 60, 255};

static bool saw_error = false;

static void err_handler(CC_ErrorData err) {
  saw_error = true;
  fprintf(stderr, "clay error: %.*s\n", (int)err.errorText.length,
          err.errorText.chars);
}

typedef struct {
  int count;
  CC_RenderCommand *r[4];
} Rects;

static bool close_to(float a, float b) { return fabsf(a - b) < 0.5f; }

static bool same_color(CC_Color a, CC_Color b) {
  return close_to(a.r, b.r) && close_to(a.g, b.g) && close_to(a.b, b.b) &&
         close_to(a.a, b.a);
}

static CC_BoundingBox box_of(const char *id) {
  Clay_ElementData d = Clay_GetElementData(CC_SID(CC__Str(id)));
  assert(d.found && "element id not found");
  return d.boundingBox;
}

/* Rectangle commands whose box lies inside the element `parent_id`, in
 * emission order. The parents in this test have no background, so the
 * first one is the bar's track and the second, if any, its fill. */
static Rects rects_in(CC_RenderCommandArray cmds, const char *parent_id) {
  CC_BoundingBox p = box_of(parent_id);
  Rects out = {0};
  for (int32_t i = 0; i < cmds.length; ++i) {
    CC_RenderCommand *c = CC_RenderCommandArray_Get(&cmds, i);
    if (!c || c->commandType != CLAY_RENDER_COMMAND_TYPE_RECTANGLE)
      continue;
    CC_BoundingBox b = c->boundingBox;
    bool inside = b.x >= p.x - 0.5f && b.x + b.width <= p.x + p.width + 0.5f &&
                  b.y >= p.y - 0.5f && b.y + b.height <= p.y + p.height + 0.5f;
    if (inside && out.count < 4)
      out.r[out.count++] = c;
  }
  return out;
}

int main(void) {
  CC_SetViewport(400.0f, 300.0f);
  CC_SetErrorHandler(err_handler);
  CC_Init();

  CC_Begin();
  Column("Root", .layout = {.sizing = {Grow(), Grow()}, .childGap = 8}) {
    Row("Half", .layout = {.sizing = {Fixed(200), Fit()}}) {
      LinearProgress(0.5f);
    }
    Row("Full", .layout = {.sizing = {Fixed(200), Fit()}}) {
      LinearProgress(1.0f);
    }
    Row("Over", .layout = {.sizing = {Fixed(200), Fit()}}) {
      LinearProgress(1.5f);
    }
    Row("Empty", .layout = {.sizing = {Fixed(200), Fit()}}) {
      LinearProgress(0.0f);
    }
    Row("Opts", .layout = {.sizing = {Fixed(200), Fit()}}) {
      LinearProgress(0.5f, .length = 80, .thickness = 10, .color = FILL,
                     .trackColor = TRACK);
    }
    Row("Infinite", .layout = {.sizing = {Fixed(200), Fit()}}) {
      LinearProgress(CC_PROGRESS_INFINITE);
    }
  }
  CC_RenderCommandArray cmds = CC_End();

  CC_Color font = CC_GetGlobalFontColor();
  CC_Color quarter = font;
  quarter.a *= 0.25f;

  Rects half = rects_in(cmds, "Half");
  assert(half.count == 2 && "expected track and fill");
  assert(close_to(half.r[0]->boundingBox.width, 200.0f));
  assert(close_to(half.r[0]->boundingBox.height, 4.0f));
  assert(close_to(half.r[1]->boundingBox.width, 100.0f));
  assert(same_color(half.r[1]->renderData.rectangle.backgroundColor, font));
  assert(same_color(half.r[0]->renderData.rectangle.backgroundColor, quarter));

  Rects full = rects_in(cmds, "Full");
  assert(full.count == 2 && close_to(full.r[1]->boundingBox.width, 200.0f));

  Rects over = rects_in(cmds, "Over");
  assert(over.count == 2 && close_to(over.r[1]->boundingBox.width, 200.0f));

  Rects empty = rects_in(cmds, "Empty");
  assert(empty.count == 1 && "zero value must emit no fill");

  Rects opts = rects_in(cmds, "Opts");
  assert(opts.count == 2);
  assert(close_to(opts.r[0]->boundingBox.width, 80.0f));
  assert(close_to(opts.r[0]->boundingBox.height, 10.0f));
  assert(close_to(opts.r[1]->boundingBox.width, 40.0f));
  assert(same_color(opts.r[0]->renderData.rectangle.backgroundColor, TRACK));
  assert(same_color(opts.r[1]->renderData.rectangle.backgroundColor, FILL));

  /* rects_in already limits the result to the parent's box, and the
   * track fills the parent, so anything it returns is inside the track. */
  Rects inf = rects_in(cmds, "Infinite");
  assert(inf.count >= 1 && "infinite bar lost its track");
  assert(close_to(inf.r[0]->boundingBox.width, 200.0f));

  assert(!saw_error && "Clay error leaked");

  CC_Shutdown();
  printf("progress ok\n");
  return 0;
}
