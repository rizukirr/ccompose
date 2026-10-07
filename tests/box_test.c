/* Headless test - Box z-stack.
 *
 * Invariants:
 *   1. Every child of a Box is aligned inside the box on its own, later
 *      children do not flow below earlier ones.
 *   2. A Grow() child after the first fills the box inside its padding.
 *   3. A Fit() box takes its size from its first child.
 *   4. Leaf children (Text, CircularProgress) are layered too.
 *   5. Column still flows its children top to bottom.
 * */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "ccompose.h"

static bool saw_error = false;

static void err_handler(CC_ErrorData err) {
  saw_error = true;
  fprintf(stderr, "clay error: %.*s\n", (int)err.errorText.length,
          err.errorText.chars);
}

static bool close_to(float a, float b) { return fabsf(a - b) < 0.5f; }

static CC_BoundingBox box_of(const char *id) {
  Clay_ElementData d = Clay_GetElementData(CC_SID(CC__Str(id)));
  assert(d.found && "element id not found");
  return d.boundingBox;
}

static bool is_at(const char *id, float x, float y, float w, float h) {
  CC_BoundingBox b = box_of(id);
  bool ok = close_to(b.x, x) && close_to(b.y, y) && close_to(b.width, w) &&
            close_to(b.height, h);
  if (!ok)
    fprintf(stderr, "%s: got %.1f,%.1f %.1fx%.1f\n", id, b.x, b.y, b.width,
            b.height);
  return ok;
}

int main(void) {
  CC_SetViewport(400.0f, 300.0f);
  CC_SetErrorHandler(err_handler);
  CC_Init();

  CC_Begin();
  Column("Root", .layout = {.sizing = {Grow(), Grow()}}) {
    Box("Stack",
        .layout = {.sizing = {Fixed(200), Fixed(100)},
                   .padding = PadAll(10),
                   .childAlignment = {.x = CC_ALIGN_X_CENTER,
                                      .y = CC_ALIGN_Y_CENTER}}) {
      Column("A", .layout = {.sizing = {Fixed(50), Fixed(50)}}) {}
      Column("B", .layout = {.sizing = {Fixed(20), Fixed(20)}}) {}
      Row("C", .layout = {.sizing = {Grow(), Grow()}}) {}
      CircularProgress(0.5f);
      Text("24:59", .fontSize = 14);
    }
    Box("Fit", .layout = {.padding = PadAll(5)}) {
      Column("FitA", .layout = {.sizing = {Fixed(30), Fixed(40)}}) {}
      Column("FitB", .layout = {.sizing = {Fixed(90), Fixed(90)}}) {}
    }
    Column("Flow") {
      Column("FlowA", .layout = {.sizing = {Fixed(10), Fixed(10)}}) {}
      Column("FlowB", .layout = {.sizing = {Fixed(10), Fixed(10)}}) {}
    }
  }
  CC_RenderCommandArray cmds = CC_End();

  assert(!saw_error && "clay reported an error during layout");

  assert(is_at("Stack", 0, 0, 200, 100));
  assert(is_at("A", 75, 25, 50, 50));
  assert(is_at("B", 90, 40, 20, 20));
  assert(is_at("C", 10, 10, 180, 80));

  assert(is_at("Fit", 0, 100, 40, 50));
  assert(is_at("FitA", 5, 105, 30, 40));
  assert(is_at("FitB", 5, 105, 90, 90));

  assert(is_at("FlowA", 0, 150, 10, 10));
  assert(is_at("FlowB", 0, 160, 10, 10));

  /* The text is the last layer: centred in the box, not below it. */
  bool saw_text = false;
  for (int32_t i = 0; i < cmds.length; ++i) {
    CC_RenderCommand *c = CC_RenderCommandArray_Get(&cmds, i);
    if (!c || c->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT)
      continue;
    CC_BoundingBox b = c->boundingBox;
    saw_text = true;
    assert(close_to(b.x + b.width / 2, 100) && "text not centred on x");
    assert(close_to(b.y + b.height / 2, 50) && "text not centred on y");
  }
  assert(saw_text && "expected a TEXT render command");

  CC_Shutdown();
  printf("box_test ok\n");
  return 0;
}
