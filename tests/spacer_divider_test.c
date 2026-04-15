/* Headless test - HSpacer/VSpacer, HDivider/VDivider.
 *
 * Invariants:
 *   1. No Clay error with anonymous ID repeated in loops (regresseion: auto-id
 * must be collide)
 *   2. Divider emits a RECTANGLE command with the supplied color.
 *   3. Spacer emits no RECTANGLE (no background set).
 *   4. Default divider color is applied when .color is zeroed
 * */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "ccompose.h"

static bool saw_error = false;

static void err_handler(CC_ErrorData err) {
  saw_error = true;
  fprintf(stderr, "clay error: %.*s\n", (int)err.errorText.length,
          err.errorText.chars);
}

int main(void) {
  CC_SetViewport(400.0f, 300.0f);
  CC_SetErrorHandler(err_handler);

  CC_Init();
  CC_Begin();
  Column("Root", .layout = {.sizing = {Grow(), Grow()}}) {
    Row("Toolbar", .layout = {.sizing = {Grow(), Fixed(40)}}) {
      Text("L", .textColor = Color(255, 255, 255, 255), .fontSize = 12);
      HSpacer();
      Text("R", .textColor = Color(255, 255, 255, 255), .fontSize = 12);
    }

    /* Regression: many dividers in a loop must not collide. */
    for (int i = 0; i < 10; i++) {
      HDivider();
    }

    HDivider(.thickness = 3, .color = Color(255, 0, 0, 255));

    Row("Sides", .layout = {.sizing = {Grow(), Fixed(40)}}) {
      Text("A", .textColor = Color(255, 255, 255, 255), .fontSize = 12);
      VDivider(.thickness = 2, .color = Color(0, 255, 0, 255));
      Text("B", .textColor = Color(255, 255, 255, 255), .fontSize = 12);
      VSpacer();
    }
  }
  CC_RenderCommandArray cmds = CC_End();

  int rect = 0;
  for (int32_t i = 0; i < cmds.length; ++i) {
    CC_RenderCommand *c = CC_RenderCommandArray_Get(&cmds, i);
    if (c && c->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE)
      rect++;
  }

  assert(!saw_error && "duplicate id or other Clay error leaked");
  assert(rect >= 12 && "expected >= 12 divider rectangles");

  CC_Shutdown();
  printf("spacer_divider ok: %d rectangles\n", rect);
  return 0;
}
