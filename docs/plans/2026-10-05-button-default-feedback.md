# button default feedback: Implementation Plan

**Spec:** docs/specs/2026-10-05-button-default-feedback-design.md
**Goal:** `Button` lowers its background alpha on hover and on a held press with no caller code.
**Architecture:** `Button` stops expanding to the generic `Element` and expands to a new `CC_OpenButton` in `src/ccompose.c`. That function opens the element, scales `decl.backgroundColor.a` from Clay's pointer state, then configures the element. Two `#ifndef`-guarded macros in `include/ccompose.h` hold the factors.

## Global constraints

- No fields are added to `Clay_ElementDeclaration` and it is not wrapped.
- `include/clay.h` and `external/clay/` stay unedited.
- Tests run headless under `CCOMPOSE_NO_BACKEND` and drive the pointer with `Clay_SetPointerState`.
- Public symbols are `CC_*`, file-local helpers are `cc__*`. Indentation matches the file being edited.
- C11.
- Defaults: `CC_BUTTON_HOVER_ALPHA` is `0.85f`, `CC_BUTTON_PRESS_ALPHA` is `0.70f`.
- The scaled alpha is stored as a float with no rounding.
- Commits follow the Lore protocol in `AGENTS.md`: the subject explains why, with trailers where relevant.

### Task 1: Button scales its background alpha from pointer state → verify: `cmake -S . -B build -DCCOMPOSE_BACKEND_RAYLIB=OFF && cmake --build build && ctest --test-dir build --output-on-failure` exits 0 with `ccompose_button_test` among the tests run, and the same three commands against `build-override` configured with `-DCMAKE_C_FLAGS=-DCC_BUTTON_HOVER_ALPHA=0.5f` exit 0 with no redefinition warning in the build output

**Files:**
- Create: `tests/button_test.c`
- Modify: `CMakeLists.txt:187`
- Modify: `include/ccompose.h:1507-1577`
- Modify: `src/ccompose.c:589`

- [x] Step 1: Create `tests/button_test.c` with this content.

```c
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
```

- [x] Step 2: In `CMakeLists.txt`, add this line directly after the `ccompose_add_test(compose_spacer_divider_test tests/spacer_divider_test.c)` line.

```cmake
    ccompose_add_test(ccompose_button_test        tests/button_test.c)
```

- [x] Step 3: Run `cmake -S . -B build -DCCOMPOSE_BACKEND_RAYLIB=OFF && cmake --build build`. Confirm the build fails on `CC_BUTTON_HOVER_ALPHA` being undeclared in `tests/button_test.c`, which shows the test exercises the new surface.

- [x] Step 4: In `src/ccompose.c`, add this function directly after the closing brace of `CC_OpenElement` and before `CC_CloseScope`.

```c
CC_Scope CC_OpenButton(CC_String id, Clay_ElementDeclaration decl) {
  decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
  if (id.length == 0) {
    Clay__OpenElement();
  } else {
    Clay__OpenElementWithId(Clay__HashString(id, 0));
  }
  /* vibekit: a press that started outside the button also counts, track
   * the press origin id if that ever matters. */
  if (Clay_Hovered()) {
    Clay_PointerData pointer = Clay_GetPointerState();
    bool down = pointer.state == CLAY_POINTER_DATA_PRESSED ||
                pointer.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME;
    decl.backgroundColor.a *=
        down ? CC_BUTTON_PRESS_ALPHA : CC_BUTTON_HOVER_ALPHA;
  }
  Clay__ConfigureOpenElement(decl);
  return (CC_Scope){.active = 1};
}
```

- [x] Step 5: In `include/ccompose.h`, replace the `Button("id", ...)` bullet in the "Button + pointer interaction" comment block (the five lines starting ` *   Button("id", ...)  — scoped block` and ending ` *                        CC_Hovered / CC_Clicked in the IMGUI idiom.`) with this.

```c
 *   Button("id", ...)  — scoped block, same shape as Row/Column/Box,
 *                        opens a CLAY_LEFT_TO_RIGHT element. While the
 *                        pointer is over it, the background alpha is
 *                        multiplied by CC_BUTTON_HOVER_ALPHA, or by
 *                        CC_BUTTON_PRESS_ALPHA while the left button
 *                        is held. Nothing else is injected. For an
 *                        element with no feedback, use Row.
```

- [x] Step 6: In the same comment block, replace the "Typical pattern" example (from the line ` *     Button("Save",` through the line ` *     }`) with this, which drops the manual hover swap.

```c
 *     Button("Save",
 *            .layout = { .padding        = PadAll(12),
 *                        .childAlignment = { .x = AlignXCenter(),
 *                                            .y = AlignYCenter() } },
 *            .backgroundColor = Color( 40, 100, 200, 255),
 *            .cornerRadius    = RadiusAll(8)) {
 *         Text("Save",
 *              .textColor = Color(255, 255, 255, 255),
 *              .fontSize  = 16);
 *     }
 *
 * A background with alpha 0 has nothing to scale, so a transparent
 * "ghost" button still needs its own swap:
 *
 *     .backgroundColor = CC_Hovered("Ghost") ? COLOR_HOVER
 *                                            : COLOR_TRANSPARENT
```

- [x] Step 7: In `include/ccompose.h`, replace the final comment and macro of the block (from `/* Button — scoped block for clickable elements. Identical expansion to` through the two-line `#define Button(id_literal, ...)` definition) with this.

```c
/* Alpha multipliers Button applies to its background while hovered and
 * while hovered with the left button held. Define either before
 * including this header, or with -D, to override. */
#ifndef CC_BUTTON_HOVER_ALPHA
#define CC_BUTTON_HOVER_ALPHA 0.85f
#endif
#ifndef CC_BUTTON_PRESS_ALPHA
#define CC_BUTTON_PRESS_ALPHA 0.70f
#endif

/* Opens a CLAY_LEFT_TO_RIGHT element like CC_OpenElement, scaling
 * decl.backgroundColor.a by the factors above from Clay's pointer
 * state. The runtime hook the Button macro expands into. */
CC_Scope CC_OpenButton(CC_String id, CC_ElementDeclaration decl);

#define CC_BUTTON_IMPL_(scope, id_string, ...)                                 \
  for (CC_Scope scope =                                                        \
           CC_OpenButton((id_string), (CC_ElementDeclaration){__VA_ARGS__});   \
       scope.active; CC_CloseScope(&scope))

/* Button — scoped block for clickable elements. LEFT_TO_RIGHT children
 * and every CC_ElementDeclaration field, like Row, plus the default
 * hover and press alpha feedback. Give it a non-empty id so
 * CC_Hovered / CC_Clicked have something to target. The feedback
 * itself also works with id == "". */
#define Button(id_literal, ...)                                                \
  CC_BUTTON_IMPL_(CC_SCOPE_NAME_(__COUNTER__), CC__Str(id_literal),            \
                  __VA_ARGS__)
```

- [x] Step 8: Run `cmake --build build && ctest --test-dir build --output-on-failure`.

- [x] Step 9: Run `cmake -S . -B build-override -DCCOMPOSE_BACKEND_RAYLIB=OFF -DCMAKE_C_FLAGS=-DCC_BUTTON_HOVER_ALPHA=0.5f && cmake --build build-override 2>&1 | tee build-override/build.log && ctest --test-dir build-override --output-on-failure`, then run `grep -ci redefin build-override/build.log` and confirm it reports no matches. Both `build` and `build-override` are ignored by the `build**` rule in `.gitignore`.

- [x] Step 10: Commit `tests/button_test.c`, `CMakeLists.txt`, `include/ccompose.h` and `src/ccompose.c`.

### Task 2: Demo relies on the default feedback → verify: `grep -cE 'CC_(Hovered|Clicked)\("(BtnPrimary|BtnSecondary|BtnNotifToggle)"\) *\?' examples/demo.c` reports no matches, and `cmake -S . -B build-demo && cmake --build build-demo` exits 0

**Files:**
- Modify: `examples/demo.c:237-253`
- Modify: `examples/demo.c:340-343`

- [ ] Step 1: In `render_interact`, replace the primary button's comment, its `primary_bg` variable and its `.backgroundColor` line. The block from `/* Primary — filled accent, darkens on press. */` through the `Button("BtnPrimary", ...)` opening line becomes this.

```c
    /* Primary: filled accent, default hover and press feedback. */
    Button("BtnPrimary",
           .layout = {.padding = PadSymmetric(18, 10),
                      .childAlignment =
                          ChildAlign(.x = AlignXCenter(), .y = AlignYCenter())},
           .backgroundColor = COLOR_ACCENT_DIM, .cornerRadius = RadiusAll(8)) {
```

- [ ] Step 2: Replace the secondary button's comment and opening lines with this.

```c
    /* Secondary: surface fill, default hover and press feedback. */
    Button("BtnSecondary", .layout = {.padding = PadSymmetric(18, 10)},
           .backgroundColor = COLOR_SURFACE_2, .cornerRadius = RadiusAll(8),
           .border = {.color = COLOR_BORDER, .width = BorderAll(1)}) {
```

- [ ] Step 3: Replace the `BtnNotifToggle` opening lines with this. The toggled color stays because it shows state.

```c
    Button("BtnNotifToggle", .layout = {.padding = PadSymmetric(18, 10)},
           .backgroundColor =
               toggle_notifications ? COLOR_PRESSED : COLOR_ACCENT_DIM,
           .cornerRadius = RadiusAll(8)) {
```

- [ ] Step 4: Leave `BtnGhost` and the sidebar navigation buttons as they are. Their idle background is `COLOR_TRANSPARENT`, which the default cannot scale.

- [ ] Step 5: Run `clang-format -i examples/demo.c`, then `git diff --stat examples/demo.c` to confirm only that file changed.

- [ ] Step 6: Run `cmake -S . -B build-demo && cmake --build build-demo`. The raylib backend is on by default, and CMake fetches raylib when `find_package` does not locate a matching one, so this step needs network access in that case. Confirm the build output has no unused-variable warning for `examples/demo.c`.

- [ ] Step 7: Run `grep -cE 'CC_(Hovered|Clicked)\("(BtnPrimary|BtnSecondary|BtnNotifToggle)"\) *\?' examples/demo.c`.

- [ ] Step 8: Commit `examples/demo.c`.

## Outside this plan

The wiki page `Interaction.md` is in the separate repository `rizukirr/ccompose.wiki`. Its "Button" section and "Typical pattern" example need the same rewrite as the header comment. That edit is prepared separately and the repository owner pushes it.
