---
title: shadow
date: 2026-11-03
status: approved
---

# shadow: Design

## Problem

ccompose has no way to give an element a drop shadow. Clay (vendored, post v0.14 main snapshot) has no shadow support either, so updating Clay does not help.

## Goals

1. `Column("card", .cornerRadius = RadiusAll(8), .custom = Shadow(.offset = {0, 4}, .blur = 12))` compiles and, in the raylib build, draws a soft shadow behind the card that follows its corner radius. Verified by building the demo and looking at it.
2. The element emits exactly one CUSTOM render command whose `customData` points to a struct with `_tag == -2` and the passed `offset`, `blur`, `spread`, `color`. That command comes before the element's background RECTANGLE command. Verified headless in `tests/shadow_test.c`.
3. `Shadow()` with `color.a == 0` stores `{0, 0, 0, 64}`, and a negative `blur` is stored as `0`. Verified headless.
4. 128 `Shadow()` calls in one frame succeed and call 129 returns `customData == NULL`. After the next `CC_Begin()` calls succeed again. Verified headless.
5. `cmake --build build` and `ctest --test-dir build --output-on-failure` pass in both the raylib build and the `-DCCOMPOSE_BACKEND_RAYLIB=OFF` build.

## Non-goals

- Shadows on `Text` elements.
- Inset shadows.
- More than one shadow per element.
- A shader based blur.
- A shadow and a `Draw` callback on the same element.

## Constraints

- Must work on Windows, Linux, macOS and the Emscripten web build without custom GLSL.
- `external/clay` and `include/clay.h` are not edited.
- C11, 4 space indent, `CC_*` public names, `cc__*` file local names.
- Run limits: at most 3 verify rounds, at most $0 on paid runs.

## Approach

Public API in `include/ccompose.h`:

```c
typedef struct {
  CC_Vector2 offset; /* shadow shift in px                                 */
  float blur;        /* soft edge width in px, 0 = hard shadow             */
  float spread;      /* grows (+) or shrinks (-) the shadow before blur    */
  CC_Color color;    /* alpha 0 means default Color(0, 0, 0, 64)           */
} CC_ShadowOpts;

CC_CustomElementConfig CC_Shadow(CC_ShadowOpts opts);
#define Shadow(...) CC_Shadow((CC_ShadowOpts){__VA_ARGS__})
```

`src/ccompose.c`: `CC_Shadow` takes a slot from a static per-frame pool `cc__shadow_pool[CC_DRAW_POOL_SIZE]` of `{int _tag; CC_ShadowOpts opts;}` with `_tag = CC_SHADOW_TAG (-2)`, applies the defaults (blur below 0 becomes 0, color alpha 0 becomes `{0,0,0,64}`) and returns `(CC_CustomElementConfig){.customData = slot}`. When the pool is full it returns `{NULL}`. `CC_Begin()` resets the pool in both the raylib and the headless path. The pool exists in headless builds so tests can inspect it.

`renderer/raylib/clay_renderer_raylib.c`: in the `CLAY_RENDER_COMMAND_TYPE_CUSTOM` case, before the draw slot check, a forward declared `CC_ShadowSlotFwd` with tag `-2` is recognised. The renderer draws the shadow without a scissor and breaks without skipping the next background command. Clay emits an element's CUSTOM command before its background RECTANGLE, so the shadow lands behind the element and above earlier siblings.

Drawing:

1. Base rect is the element bounding box moved by `offset` and grown by `spread` on each side. Base radius is `cornerRadius.topLeft + spread`, clamped to at least 0.
2. If `blur == 0`, draw one `DrawRectangleRounded` in `color`.
3. Otherwise draw 8 layers from largest to smallest. Layer `i` (8 down to 1) grows by `blur * i / 8` with radius `base radius + grow`. Each layer uses alpha `1 - (1 - a)^(1/8)` so the fully overlapped centre composites to exactly the requested alpha `a`.

Known limit: Clay culls elements that are fully off screen, so the shadow of such an element is not drawn even if it would reach on screen. Marked with a `vibekit:` comment.

Pushback record: the user first asked about Windows support, raylib versus OpenGL and WebGL, and what the smaller version meant, then chose the smaller framing (stacked rounded rectangles, no shader).

## Alternatives considered

- Floating layered element, no renderer change. Rejected because a floating root with zIndex -1 paints before the root tree and is hidden by page backgrounds, and with zIndex 0 paints after the main tree and covers the element.
- CUSTOM command drawn by a GLSL fragment shader with a true blur. Rejected because it needs separate `#version 330` and `#version 100` sources to cover desktop and web, and only looks clearly better for blurs above about 60 px.
- Reusing `CC_AcquireDrawSlot`. Rejected because the renderer scissors draw slots to the element box, which would clip the shadow.

## Testing

`tests/shadow_test.c`, headless, registered with `ccompose_add_test(ccompose_shadow_test tests/shadow_test.c)`, covering goals 2, 3 and 4. Goal 1 is checked by building the raylib demo where one existing card in `examples/demo.c` gets a shadow, with a screenshot for the PR. Goal 5 by running both build configurations.

Docs: a header comment above `CC_ShadowOpts` covering fields, defaults, pool limit, the Draw exclusion and a usage snippet, plus a `Shadow` mention in the `.custom` entry of the element scope comment block.

## Open questions

N/A: all decisions were settled in the brainstorm dialogue.
