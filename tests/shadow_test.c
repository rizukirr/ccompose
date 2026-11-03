/* Headless test - Shadow.
 *
 * Invariants:
 *   1. An element with .custom = Shadow(...) emits one CUSTOM command
 *      whose data carries CC_SHADOW_TAG and the passed values, before
 *      the element's background RECTANGLE.
 *   2. Alpha 0 falls back to {0, 0, 0, 64}, negative blur becomes 0.
 *   3. The pool holds CC_SHADOW_POOL_SIZE shadows per frame, the next
 *      call returns NULL, and CC_Begin() empties the pool.
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

static bool same_color(CC_Color a, CC_Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int main(void) {
  CC_SetViewport(400.0f, 400.0f);
  CC_SetErrorHandler(err_handler);
  CC_Init();

  /* 1 and 2 */
  CC_Begin();
  CC_CustomElementConfig full =
      Shadow(.offset = {3, 4}, .blur = 12, .spread = 2,
             .color = Color(1, 2, 3, 90));
  CC_CustomElementConfig dflt = Shadow(.blur = -5);
  Column("Root", .layout = {.sizing = {Grow(), Grow()}}) {
    Column("Card", .layout = {.sizing = {Fixed(100), Fixed(50)}},
           .backgroundColor = Color(200, 200, 200, 255),
           .cornerRadius = RadiusAll(8), .custom = full) {}
  }
  CC_RenderCommandArray cmds = CC_End();
  assert(!saw_error);

  CC_ShadowSlot *s = (CC_ShadowSlot *)full.customData;
  assert(s && s->_tag == CC_SHADOW_TAG);
  assert(s->opts.offset.x == 3 && s->opts.offset.y == 4);
  assert(s->opts.blur == 12 && s->opts.spread == 2);
  assert(same_color(s->opts.color, Color(1, 2, 3, 90)));

  CC_ShadowSlot *d = (CC_ShadowSlot *)dflt.customData;
  assert(d && d->_tag == CC_SHADOW_TAG);
  assert(d->opts.blur == 0);
  assert(same_color(d->opts.color, Color(0, 0, 0, 64)));

  uint32_t card = CC_SID(CC__Str("Card")).id;
  int custom_at = -1, rect_at = -1, customs = 0;
  for (int32_t i = 0; i < cmds.length; ++i) {
    CC_RenderCommand *c = CC_RenderCommandArray_Get(&cmds, i);
    if (c->id != card)
      continue;
    if (c->commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM) {
      customs++;
      custom_at = i;
      assert(c->renderData.custom.customData == s);
      assert(c->renderData.custom.cornerRadius.topLeft == 8);
    }
    if (c->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE)
      rect_at = i;
  }
  assert(customs == 1);
  assert(rect_at >= 0 && custom_at < rect_at);

  /* 3 */
  CC_Begin();
  for (int i = 0; i < CC_SHADOW_POOL_SIZE; ++i)
    assert(Shadow(.blur = 1).customData != NULL);
  assert(Shadow(.blur = 1).customData == NULL);
  CC_End();
  CC_Begin();
  assert(Shadow(.blur = 1).customData != NULL);
  CC_End();

  assert(!saw_error);
  CC_Shutdown();
  printf("shadow_test: ok\n");
  return 0;
}
