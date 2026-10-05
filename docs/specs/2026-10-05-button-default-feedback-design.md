---
title: button default feedback
date: 2026-10-05
status: draft
---

# button default feedback: Design

## Problem

`Button` is a pure alias for `Element(CLAY_LEFT_TO_RIGHT, id, ...)`. A button shows no reaction to the pointer unless the caller writes a `CC_Hovered(...) ? A : B` swap on every button, and there is no helper at all for the held-down look. `examples/demo.c` repeats that swap on five buttons.

## Goals

1. A `Button` with a non-transparent `.backgroundColor` reacts to hover with no extra caller code. Success: with the pointer over the button, the rectangle render command for that button has alpha equal to the declared alpha times `CC_BUTTON_HOVER_ALPHA` (default 0.85), truncated to an integer.
2. The same `Button` reacts to a held left press. Success: with the pointer over the button and the pointer state down, the rectangle alpha equals the declared alpha times `CC_BUTTON_PRESS_ALPHA` (default 0.70), truncated to an integer.
3. A `Button` the pointer is not over is untouched. Success: the rectangle color equals the declared color exactly, whether the pointer is up or down.
4. Both factors can be overridden at compile time. Success: building with `-DCC_BUTTON_HOVER_ALPHA=0.5f` changes the hover result and produces no redefinition warning.
5. `Row`, `Column`, `Box` and `Element` behave as before. Success: the three existing CTest targets pass unchanged.
6. The demo relies on the default. Success: `examples/demo.c` has no manual hover swap on `BtnPrimary`, `BtnSecondary` or `BtnNotifToggle`, and it builds.

## Non-goals

- Animated fading between states.
- Feedback for a button whose background alpha is 0. Ghost buttons keep a manual `CC_Hovered` swap.
- Per-button configuration of the factors, or a runtime setter.
- Tracking which element a press started on.
- Changes to `CC_Hovered`, `CC_Clicked`, or text and border colors.

## Constraints

- `Clay_ElementDeclaration` is Clay's struct. No fields are added to it and it is not wrapped.
- `include/clay.h` and `external/clay/` are vendored and stay unedited.
- The test must run headless under `CCOMPOSE_NO_BACKEND`, driving the pointer with `Clay_SetPointerState`.
- Naming follows `AGENTS.md`: public symbols are `CC_*`, file-local helpers are `cc__*`. Indentation matches the existing sources.
- C11.

## Approach

`Button(id, ...)` expands to a new public function `CC_OpenButton(CC_String id, Clay_ElementDeclaration decl)` through the existing `for`-scope pattern used by `CC_ELEMENT_IMPL_`. The function lives in `src/ccompose.c` next to `CC_OpenElement` and does this, in order:

1. Sets `decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT`.
2. Opens the element the same way `CC_OpenElement` does: `Clay__OpenElement()` for an empty id, `Clay__OpenElementWithId(Clay__HashString(id, 0))` otherwise.
3. If `Clay_Hovered()` is true, picks a factor. The factor is `CC_BUTTON_PRESS_ALPHA` when `Clay_GetPointerState().state` is `CLAY_POINTER_DATA_PRESSED` or `CLAY_POINTER_DATA_PRESSED_THIS_FRAME`, and `CC_BUTTON_HOVER_ALPHA` otherwise. It multiplies `decl.backgroundColor.a` by the factor.
4. Calls `Clay__ConfigureOpenElement(decl)` and returns an active `CC_Scope`.

`CC_BUTTON_HOVER_ALPHA` and `CC_BUTTON_PRESS_ALPHA` are defined in `include/ccompose.h` inside `#ifndef` guards with defaults `0.85f` and `0.70f`.

`Clay_Hovered()` reads the open element, so the effect also works for a button with an empty id. Hover is computed against the previous frame's layout, the same one-frame lag `CC_Hovered` already has.

The header comment on `Button` and the "Button + pointer interaction" block are rewritten to describe the default effect, and they name `Row` as the way to get an element with no injected behavior, since `Row` has the expansion `Button` used to have.

`examples/demo.c` drops the manual hover swap on `BtnPrimary`, `BtnSecondary` and `BtnNotifToggle`. `BtnGhost` keeps its swap because its idle background is transparent. The sidebar navigation buttons keep their swap for the same reason: their idle background is `COLOR_TRANSPARENT`.

The wiki page `Interaction.md` lives in a separate repository. Its edit is prepared outside this change and the repository owner pushes it.

Pushback recorded during brainstorm: the challenge was that a default reverses the documented "no hidden behavior" rule and stacks with existing manual swaps, and that an opt-in color helper would avoid both. The user asked for a recommendation, the recommendation was to keep the default because an unreactive button is the common mistake and `Row` is a free opt-out, and the user approved that design.

Known ceiling: pressing outside a button and dragging onto it shows the pressed look, because Clay does not record where a press started.

## Alternatives considered

An opt-in macro `CC_ButtonColor("id", base)` over `CC_Hovered`, with `Button` untouched. It is the smallest diff and breaks nothing. It was not chosen because the effect stops being a default, and callers who forget feedback today would still forget it.

The default effect plus an injected Clay transition on background color, so the alpha eases over about 100 ms. It looks better. It was deferred because the injected transition has to merge with a caller-supplied `.transition`, and it can be added on top of this change later without changing the public surface.

A macro-only version that appends a second `.backgroundColor` initializer after `__VA_ARGS__`. It was rejected because a repeated designated initializer triggers `-Woverride-init` and cannot read the caller's color.

## Testing

A new headless test `tests/button_test.c`, registered in `CMakeLists.txt` as `ccompose_button_test`. It declares a root containing one `Button` with a known opaque color and one `Row` with the same color, runs layout once so Clay has geometry, then checks the rectangle render commands across later frames:

- pointer away from both, up: button color equals the declared color.
- pointer over the button, up: button alpha equals `(float)255 * CC_BUTTON_HOVER_ALPHA` as stored by Clay.
- pointer over the button, down: button alpha equals `(float)255 * CC_BUTTON_PRESS_ALPHA`.
- pointer away, down: button color equals the declared color.
- pointer over the `Row`: row color equals the declared color.

One more case covers a `Button` with background alpha 0 under the pointer: alpha stays 0.

Verification commands: `cmake -S . -B build -DCCOMPOSE_BACKEND_RAYLIB=OFF`, `cmake --build build`, `ctest --test-dir build --output-on-failure`. The demo build is checked with the raylib backend on when raylib is available on the machine.

## Open questions

N/A: the color math, the opt-out and the scope were settled during brainstorm.
