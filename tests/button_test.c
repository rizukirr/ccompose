/* Headless test - Button default hover/press feedback.
 *
 * Invariants:
 *   1. A Button the pointer is not over keeps its declared color,
 *      pointer up or down.
 *   2. Hover scales the background alpha by CC_BUTTON_HOVER_ALPHA.
 *   3. Hover with the pointer down scales it by CC_BUTTON_PRESS_ALPHA.
 *   4. A Row under the pointer keeps its declared color.
 *   5. A Button with alpha 0 emits no rectangle even when hovered.
 * */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "ccompose.h"

static const CC_Color BASE = {40, 100, 200, 255};

static bool saw_error = false;

static void err_handler(CC_ErrorData err) {
  saw_error = true;
  fprintf(stderr, "clay error: %.*s\n", (int)err.errorText.length,
          err.errorText.chars);
}

/* Three 100x40 cells side by side: Btn at x 0..100, Plain at 100..200,
 * Ghost at 200..300. */
static CC_RenderCommandArray frame(void) {
  CC_Begin();
  Row("Root", .layout = {.sizing = {Grow(), Grow()}}) {
    Button("Btn", .layout = {.sizing = {Fixed(100), Fixed(40)}},
           .backgroundColor = BASE) {}
    Row("Plain", .layout = {.sizing = {Fixed(100), Fixed(40)}},
        .backgroundColor = BASE) {}
    Button("Ghost", .layout = {.sizing = {Fixed(100), Fixed(40)}},
           .backgroundColor = Color(0, 0, 0, 0)) {}
  }
  return CC_End();
}

/* Returns the alpha of the rectangle emitted for `id`, or -1 if the
 * element emitted no rectangle. */
static float rect_alpha(CC_RenderCommandArray cmds, const char *id) {
  uint32_t want = CC_SID(CC__Str(id)).id;
  for (int32_t i = 0; i < cmds.length; ++i) {
    CC_RenderCommand *c = CC_RenderCommandArray_Get(&cmds, i);
    if (c && c->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE &&
        c->id == want)
      return c->renderData.rectangle.backgroundColor.a;
  }
  return -1.0f;
}

static bool close_to(float a, float b) { return fabsf(a - b) < 0.01f; }

int main(void) {
  CC_SetViewport(400.0f, 300.0f);
  CC_SetErrorHandler(err_handler);
  CC_Init();

  /* First frame gives Clay the geometry that hover checks run against. */
  frame();

  CC_RenderCommandArray cmds;

  Clay_SetPointerState((CC_Vector2){350, 200}, false);
  cmds = frame();
  assert(close_to(rect_alpha(cmds, "Btn"), BASE.a) && "idle button changed");

  Clay_SetPointerState((CC_Vector2){50, 20}, false);
  cmds = frame();
  assert(close_to(rect_alpha(cmds, "Btn"), BASE.a * CC_BUTTON_HOVER_ALPHA) &&
         "hover alpha wrong");
  assert(close_to(rect_alpha(cmds, "Plain"), BASE.a) && "row changed");

  Clay_SetPointerState((CC_Vector2){50, 20}, true);
  cmds = frame();
  assert(close_to(rect_alpha(cmds, "Btn"), BASE.a * CC_BUTTON_PRESS_ALPHA) &&
         "press alpha wrong");

  Clay_SetPointerState((CC_Vector2){350, 200}, true);
  cmds = frame();
  assert(close_to(rect_alpha(cmds, "Btn"), BASE.a) &&
         "pointer down elsewhere changed button");

  Clay_SetPointerState((CC_Vector2){150, 20}, true);
  cmds = frame();
  assert(close_to(rect_alpha(cmds, "Plain"), BASE.a) && "hovered row changed");

  Clay_SetPointerState((CC_Vector2){250, 20}, false);
  cmds = frame();
  assert(rect_alpha(cmds, "Ghost") < 0.0f && "ghost button drew a rectangle");

  assert(!saw_error && "Clay error leaked");

  CC_Shutdown();
  printf("button ok\n");
  return 0;
}
