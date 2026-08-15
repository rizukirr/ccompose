/* ccompose.h — Jetpack-Compose-shaped wrapper around Clay.
 * =========================================================================
 *
 * ccompose gives Clay a declarative, scoped-block UI syntax that reads
 * like Jetpack Compose while staying plain C. Every container opens a
 * real Clay element under the hood; every field you set maps 1:1 onto
 * Clay's own config structs. There is no hidden state, no tree, no
 * runtime allocation per frame.
 *
 * Backend: raylib by default. Build ccompose with
 * -DCCOMPOSE_BACKEND_RAYLIB=OFF for headless mode; the same option is
 * propagated to consumers as CCOMPOSE_NO_BACKEND via the CMake target.
 *
 * This is the public header. Link against libccompose (built from
 * src/ccompose.c) for the implementation — there is no
 * CCOMPOSE_IMPLEMENTATION define.
 *
 * -------------------------------------------------------------------------
 * Minimal usage
 * -------------------------------------------------------------------------
 *
 *   #include "ccompose.h"
 *
 *   int main(void) {
 *       CC_SetWindow(960, 640, "Hello");
 *       CC_Init();
 *       while (CC_Running()) {
 *           CC_Begin();
 *           Column("Root",
 *                  .layout = { .sizing  = { Grow(), Grow() },
 *                              .padding = PadAll(24) },
 *                  .backgroundColor = Color(18, 18, 20, 255)) {
 *               Text("Hello, world!",
 *                    .textColor = Color(255, 255, 255, 255),
 *                    .fontSize  = 32);
 *           }
 *           CC_End();
 *       }
 *       CC_Shutdown();
 *   }
 *
 * -------------------------------------------------------------------------
 * Mental model
 * -------------------------------------------------------------------------
 *
 *   Column("id", .field = value, ...) { children }
 *
 * expands to a `for` loop that runs exactly once. On entry it opens a
 * Clay element, configures it with the `CC_ElementDeclaration` literal
 * you wrote, and runs the block. On exit it closes the element. Every
 * nested Column/Row/Box/Text inside the block becomes a child of that
 * element. No hidden tree — Clay holds the layout state in its arena.
 *
 * Because the macro expands to a loop, you can't `break` out of a block
 * and expect the element to close (the `break` would skip the close
 * step). Use `return`/`goto` from inside a block only with care.
 *
 * -------------------------------------------------------------------------
 * What's in this header
 * -------------------------------------------------------------------------
 *
 *   Type aliases        CC_Decl, CC_TextStyle, CC_RenderCommandArray, ...
 *   Sugar               Fit(), Grow(), Fixed(), Percent(), PadAll(),
 *                       PadSymmetric(), PadOnly(), BorderAll(),
 *                       BorderSymmetric(), BorderOnly(), RadiusAll(),
 *                       RadiusTopBottom(), RadiusLeftRight(), RadiusOnly(),
 *                       Color(), ColorHex(),
 *                       AlignXStart/Center/End, AlignYStart/Center/End,
 *   Lifecycle           CC_Init, CC_Begin, CC_End, CC_Running, CC_Shutdown
 *                       CC_SetWindow, CC_SetWindowFlags, CC_SetBackground,
 *                       CC_SetErrorHandler, CC_LoadFont, CC_SetViewport
 *   Container macros    Column("id", ...), Row("id", ...), Box("id", ...),
 *                       Element(direction, "id", ...)
 *   Text macro          Text("string", ...)
 *   Leaf helpers        HSpacer(), VSpacer(), HDivider(), VDivider()
 *                       (auto-id, safe in loops)
 *   Transitions         DefineTransition(name, dur, .enter = {...}, ...)
 *                       CC_EaseOut, CC_Linear, CC_EaseIn, CC_EaseInOut
 *
 * -------------------------------------------------------------------------
 * Transitions
 * -------------------------------------------------------------------------
 *
 * Declare reusable transition presets at file scope with DefineTransition:
 *
 *   DefineTransition(sidebar_anim, 0.4f,
 *       .enter = { .slideX = -240, .fade = true },
 *       .exit  = { .slideX = -240, .fade = true },
 *   );
 *
 * Reference in any container (note the parentheses — it's a function call):
 *
 *   Column("Sidebar", .transition = sidebar_anim(),
 *          .layout = { .sizing = { Fixed(240), Grow() } }) { ... }
 *
 * High-level presets (.slideX, .slideY, .fade) and raw Clay properties
 * (.backgroundColor, .overlayColor, .borderColor, .borderWidth) can be
 * mixed. Properties bitmask is auto-inferred unless you set .properties
 * explicitly.
 *
 * Built-in easings: CC_EaseOut (default), CC_Linear, CC_EaseIn,
 * CC_EaseInOut. Pass a custom handler via .handler = my_easing_fn.
 */

#ifndef CCOMPOSE_H
#define CCOMPOSE_H

#include "clay.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef CCOMPOSE_NO_BACKEND
/* When the raylib backend is active, raylib.h is exposed from the public
 * header so user code can freely use raylib types (Vector2, Font, FLAG_*)
 * alongside ccompose macros without a separate include. */
#include "raylib.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Type aliases
 * =========================================================================
 *
 * These are all straightforward typedefs of the underlying Clay types —
 * use either spelling interchangeably. They exist so ccompose code can
 * have a consistent CC_ prefix for its own types without introducing any
 * new concepts. For example:
 *
 *     CC_RenderCommandArray commands = CC_End();
 *     for (int32_t i = 0; i < commands.length; ++i) {
 *         CC_RenderCommand *cmd = CC_RenderCommandArray_Get(&commands, i);
 *         // cmd->commandType is one of CLAY_RENDER_COMMAND_TYPE_*
 *     }
 */

/* Each CC_ name below is a plain `#define` onto the matching Clay_
 * symbol, so the two spellings are interchangeable everywhere — in
 * declarations, casts, designated initializers, sizeof, etc. */

/* Short legacy aliases. */
#define CC_Decl Clay_ElementDeclaration     /* element config struct */
#define CC_TextStyle Clay_TextElementConfig /* text config struct    */

/* Element / layout types. */
#define CC_ElementDeclaration Clay_ElementDeclaration
#define CC_LayoutConfig Clay_LayoutConfig
#define CC_LayoutDirection Clay_LayoutDirection
#define CC_LEFT_TO_RIGHT CLAY_LEFT_TO_RIGHT
#define CC_TOP_TO_BOTTOM CLAY_TOP_TO_BOTTOM
#define CC_Sizing Clay_Sizing
#define CC_SizingAxis Clay_SizingAxis
#define CC_SizingType Clay__SizingType
#define CC_SIZING_TYPE_FIT CLAY__SIZING_TYPE_FIT
#define CC_SIZING_TYPE_GROW CLAY__SIZING_TYPE_GROW
#define CC_SIZING_TYPE_PERCENT CLAY__SIZING_TYPE_PERCENT
#define CC_SIZING_TYPE_FIXED CLAY__SIZING_TYPE_FIXED
#define CC_Padding Clay_Padding
#define CC_CornerRadius Clay_CornerRadius
#define CC_ChildAlignment Clay_ChildAlignment
#define CC_LayoutAlignmentX Clay_LayoutAlignmentX
#define CC_ALIGN_X_LEFT CLAY_ALIGN_X_LEFT
#define CC_ALIGN_X_RIGHT CLAY_ALIGN_X_RIGHT
#define CC_ALIGN_X_CENTER CLAY_ALIGN_X_CENTER
#define CC_LayoutAlignmentY Clay_LayoutAlignmentY
#define CC_ALIGN_Y_TOP CLAY_ALIGN_Y_TOP
#define CC_ALIGN_Y_BOTTOM CLAY_ALIGN_Y_BOTTOM
#define CC_ALIGN_Y_CENTER CLAY_ALIGN_Y_CENTER
#define CC_TEXT_WRAP_WORDS CLAY_TEXT_WRAP_WORDS
#define CC_TEXT_WRAP_NEWLINES CLAY_TEXT_WRAP_NEWLINES
#define CC_TEXT_WRAP_NONE CLAY_TEXT_WRAP_NONE
#define CC_TextElementConfigWrapMode Clay_TextElementConfigWrapMode
#define CC_TextAlignment Clay_TextAlignment
#define CC_TEXT_ALIGN_LEFT CLAY_TEXT_ALIGN_LEFT
#define CC_TEXT_ALIGN_CENTER CLAY_TEXT_ALIGN_CENTER
#define CC_TEXT_ALIGN_RIGHT CLAY_TEXT_ALIGN_RIGHT
#define CC_BoundingBox Clay_BoundingBox

/* Per-feature config structs (the designated-initializer fields on
 * CC_ElementDeclaration). */
#define CC_ImageElementConfig Clay_ImageElementConfig
#define CC_FloatingElementConfig Clay_FloatingElementConfig
#define CC_FloatingAttachToElement Clay_FloatingAttachToElement
#define CC_ATTACH_TO_NONE CLAY_ATTACH_TO_NONE
#define CC_ATTACH_TO_PARENT CLAY_ATTACH_TO_PARENT
#define CC_ATTACH_TO_ELEMENT_WITH_ID CLAY_ATTACH_TO_ELEMENT_WITH_ID
#define CC_ATTACH_TO_ROOT CLAY_ATTACH_TO_ROOT
#define CC_ATTACH_POINT_LEFT_TOP CLAY_ATTACH_POINT_LEFT_TOP
#define CC_ATTACH_POINT_LEFT_CENTER CLAY_ATTACH_POINT_LEFT_CENTER
#define CC_ATTACH_POINT_LEFT_BOTTOM CLAY_ATTACH_POINT_LEFT_BOTTOM
#define CC_ATTACH_POINT_CENTER_TOP CLAY_ATTACH_POINT_CENTER_TOP
#define CC_ATTACH_POINT_CENTER_CENTER CLAY_ATTACH_POINT_CENTER_CENTER
#define CC_ATTACH_POINT_CENTER_BOTTOM CLAY_ATTACH_POINT_CENTER_BOTTOM
#define CC_ATTACH_POINT_RIGHT_TOP CLAY_ATTACH_POINT_RIGHT_TOP
#define CC_ATTACH_POINT_RIGHT_CENTER CLAY_ATTACH_POINT_RIGHT_CENTER
#define CC_ATTACH_POINT_RIGHT_BOTTOM CLAY_ATTACH_POINT_RIGHT_BOTTOM
#define CC_ClipElementConfig Clay_ClipElementConfig
#define CC_AspectRatioElementConfig Clay_AspectRatioElementConfig
#define CC_BorderElementConfig Clay_BorderElementConfig
#define CC_BorderWidth Clay_BorderWidth
#define CC_CustomElementConfig Clay_CustomElementConfig
#define CC_TextElementConfig Clay_TextElementConfig

/* Primitive / value types. */
#define CC_Color Clay_Color           /* {r, g, b, a} 0–255 */
#define CC_String Clay_String         /* {chars, length}    */
#define CC_Dimensions Clay_Dimensions /* {width, height}    */
#define CC_Vector2 Clay_Vector2       /* {x, y} floats      */
#define CC_ElementId Clay_ElementId   /* hashed element id  */
#define CC_ErrorData Clay_ErrorData   /* error callback data*/

/* Intern runtime strings into permanent immutable storage so Clay can
 * safely use the static-string fast path. */
CC_String CC_StrIntern(const char *s);

/* Internal: wraps a C string into an interned CC_String.
 * Used by container/text macros — not part of the public API. */
#define CC__Str(s) CC_StrIntern(s)

/* Render command types returned by CC_End(). */
#define CC_RenderCommand Clay_RenderCommand
#define CC_RenderCommandArray Clay_RenderCommandArray
#define CC_RenderCommandArray_Get Clay_RenderCommandArray_Get
#define CC_RENDER_COMMAND_TYPE_CUSTOM CLAY_RENDER_COMMAND_TYPE_CUSTOM

/* Transition types. */
#define CC_TransitionElementConfig Clay_TransitionElementConfig
#define CC_TransitionData Clay_TransitionData
#define CC_TransitionArgs Clay_TransitionCallbackArguments
#define CC_TransitionProperty Clay_TransitionProperty
#define CC_TransitionState Clay_TransitionState
#define CC_TransitionEnterTrigger Clay_TransitionEnterTriggerType
#define CC_TransitionExitTrigger Clay_TransitionExitTriggerType
#define CC_TransitionInteraction Clay_TransitionInteractionHandlingType
#define CC_ExitTransitionOrdering Clay_ExitTransitionSiblingOrdering

/* Transition property flags — bitmask of what to animate. */
#define CC_TRANSITION_PROPERTY_NONE CLAY_TRANSITION_PROPERTY_NONE
#define CC_TRANSITION_PROPERTY_X CLAY_TRANSITION_PROPERTY_X
#define CC_TRANSITION_PROPERTY_Y CLAY_TRANSITION_PROPERTY_Y
#define CC_TRANSITION_PROPERTY_POSITION CLAY_TRANSITION_PROPERTY_POSITION
#define CC_TRANSITION_PROPERTY_WIDTH CLAY_TRANSITION_PROPERTY_WIDTH
#define CC_TRANSITION_PROPERTY_HEIGHT CLAY_TRANSITION_PROPERTY_HEIGHT
#define CC_TRANSITION_PROPERTY_DIMENSIONS CLAY_TRANSITION_PROPERTY_DIMENSIONS
#define CC_TRANSITION_PROPERTY_BOUNDING_BOX                                    \
  CLAY_TRANSITION_PROPERTY_BOUNDING_BOX
#define CC_TRANSITION_PROPERTY_BACKGROUND_COLOR                                \
  CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR
#define CC_TRANSITION_PROPERTY_OVERLAY_COLOR                                   \
  CLAY_TRANSITION_PROPERTY_OVERLAY_COLOR
#define CC_TRANSITION_PROPERTY_CORNER_RADIUS                                   \
  CLAY_TRANSITION_PROPERTY_CORNER_RADIUS
#define CC_TRANSITION_PROPERTY_BORDER_COLOR                                    \
  CLAY_TRANSITION_PROPERTY_BORDER_COLOR
#define CC_TRANSITION_PROPERTY_BORDER_WIDTH                                    \
  CLAY_TRANSITION_PROPERTY_BORDER_WIDTH
#define CC_TRANSITION_PROPERTY_BORDER CLAY_TRANSITION_PROPERTY_BORDER

/* Transition state enum values. */
#define CC_TRANSITION_STATE_IDLE CLAY_TRANSITION_STATE_IDLE
#define CC_TRANSITION_STATE_ENTERING CLAY_TRANSITION_STATE_ENTERING
#define CC_TRANSITION_STATE_TRANSITIONING CLAY_TRANSITION_STATE_TRANSITIONING
#define CC_TRANSITION_STATE_EXITING CLAY_TRANSITION_STATE_EXITING

/* Enter trigger types. */
#define CC_TRANSITION_ENTER_SKIP_ON_FIRST_PARENT_FRAME                         \
  CLAY_TRANSITION_ENTER_SKIP_ON_FIRST_PARENT_FRAME
#define CC_TRANSITION_ENTER_TRIGGER_ON_FIRST_PARENT_FRAME                      \
  CLAY_TRANSITION_ENTER_TRIGGER_ON_FIRST_PARENT_FRAME

/* Exit trigger types. */
#define CC_TRANSITION_EXIT_SKIP_WHEN_PARENT_EXITS                              \
  CLAY_TRANSITION_EXIT_SKIP_WHEN_PARENT_EXITS
#define CC_TRANSITION_EXIT_TRIGGER_WHEN_PARENT_EXITS                           \
  CLAY_TRANSITION_EXIT_TRIGGER_WHEN_PARENT_EXITS

/* Interaction handling during transitions. */
#define CC_TRANSITION_DISABLE_INTERACTIONS_WHILE_TRANSITIONING_POSITION        \
  CLAY_TRANSITION_DISABLE_INTERACTIONS_WHILE_TRANSITIONING_POSITION
#define CC_TRANSITION_ALLOW_INTERACTIONS_WHILE_TRANSITIONING_POSITION          \
  CLAY_TRANSITION_ALLOW_INTERACTIONS_WHILE_TRANSITIONING_POSITION

/* Exit transition sibling ordering. */
#define CC_EXIT_TRANSITION_ORDERING_UNDERNEATH_SIBLINGS                        \
  CLAY_EXIT_TRANSITION_ORDERING_UNDERNEATH_SIBLINGS
#define CC_EXIT_TRANSITION_ORDERING_NATURAL_ORDER                              \
  CLAY_EXIT_TRANSITION_ORDERING_NATURAL_ORDER
#define CC_EXIT_TRANSITION_ORDERING_ABOVE_SIBLINGS                             \
  CLAY_EXIT_TRANSITION_ORDERING_ABOVE_SIBLINGS

/* Easing function alias. */
#define CC_EaseOut Clay_EaseOut

/* Built-in easing functions. All follow the same signature as Clay_EaseOut:
 * return true when the transition is complete (ratio >= 1). */
bool CC_Linear(CC_TransitionArgs arguments);
bool CC_EaseIn(CC_TransitionArgs arguments);
bool CC_EaseInOut(CC_TransitionArgs arguments);

/* Pointer / hit-test function used by CC_Hovered. */
#define CC_PointerOver Clay_PointerOver

/* Runtime string-id constructor from a CC_String. */
#define CC_SID CLAY_SID

/* Index-offset element id — hash a (label, index) pair into a unique
 * CC_ElementId. Useful for list rows and grid cells without having to
 * snprintf a unique string per iteration:
 *
 *     for (int i = 0; i < count; ++i) {
 *         if (Clay_PointerOver(CC_IDI("row", i))) ...;
 *     }
 *
 * The label must be a string literal (uses CLAY_STRING). */
#define CC_IDI(label, index) CLAY_IDI(label, index)

/* =========================================================================
 * Sugar — sizing, padding, color, alignment
 * =========================================================================
 *
 * Shorter aliases for Clay's CLAY_* macros. Every one of these expands to
 * an expression of the right Clay type, so they can be used anywhere
 * Clay expects them (including inside raw CLAY() blocks).
 */

/* ------- Sizing ---------------------------------------------------------
 *
 * Used as the `.width` / `.height` of a `CC_Sizing`, e.g.
 *
 *     .layout = { .sizing = { Grow(), Fixed(240) } }
 *
 *   Fit()        — shrink-wrap around children (the Clay default).
 *                  An element with Fit on both axes will be just big
 *                  enough to contain its children + padding.
 *
 *   Grow()       — consume all remaining space along this axis, shared
 *                  proportionally with sibling Grow()s. Acts like CSS
 *                  `flex: 1`. Most "fill the screen" containers use
 *                  `.sizing = { Grow(), Grow() }`.
 *
 *   Fixed(n)     — exactly n pixels on this axis. Use for sidebars,
 *                  toolbars, avatars, anything with a hard size.
 *
 *   Percent(p)   — p * parent-size, where p is a float in [0.0, 1.0].
 *                  Clay errors out for values outside this range, so
 *                  0.5f is half the parent, not 50.
 */
#define Fit() CLAY_SIZING_FIT(0, 0)
#define Grow() CLAY_SIZING_GROW(0, 0)
#define Fixed(n) CLAY_SIZING_FIXED(n)
#define Percent(p) CLAY_SIZING_PERCENT(p)

/* Sizing struct sugar — collapses `.sizing = { Grow(), Grow() }` to
 * `SizingAll(Grow())` for the common both-axes-same case.
 *
 *   SizingAll(s)            — both width and height use sizing s.
 *   SizingOnly(.width = ..., .height = ...) — designated init;
 *                             unspecified axis defaults to Fit().
 */
#define SizingAll(s) ((CC_Sizing){(s), (s)})
#define SizingOnly(...) ((CC_Sizing){__VA_ARGS__})

/* ------- Padding --------------------------------------------------------
 *
 * Used as the `.padding` of a `CC_LayoutConfig`. Inspired by Flutter's
 * EdgeInsets — three helpers cover every shape:
 *
 *   PadAll(n)              — n pixels on all four sides.
 *   PadSymmetric(h, v)     — h on left/right, v on top/bottom.
 *   PadOnly(.left = 8,     — designated init; unspecified sides default
 *           .top  = 4)       to 0. Use for arbitrary per-side padding.
 *
 * Padding stacks inside the element's bounding box — it eats into the
 * available space for children, it does not grow the box.
 */
#define PadAll(n) CLAY_PADDING_ALL(n)
#define PadSymmetric(h, v) ((CC_Padding){(h), (h), (v), (v)})
#define PadOnly(...) ((CC_Padding){__VA_ARGS__})

/* ------- Border width ---------------------------------------------------
 *
 * Used as the `.width` of a `CC_BorderElementConfig`. Same three-helper
 * shape as padding, plus a `.betweenChildren` slot that Clay uses to
 * draw dividers between siblings along the parent's layout direction.
 *
 *   BorderAll(n)           — n pixels on all four sides, no
 *                            between-children divider.
 *   BorderSymmetric(h, v)  — h on left/right, v on top/bottom, no
 *                            between-children divider.
 *   BorderOnly(.left = 1,  — designated init; unspecified default to 0.
 *              .betweenChildren = 1)
 *
 * Color is set separately on the same `.border` config:
 *
 *     .border = { .color = COLOR_BORDER, .width = BorderAll(1) }
 */
#define BorderAll(n) ((CC_BorderWidth){(n), (n), (n), (n), 0})
#define BorderSymmetric(h, v) ((CC_BorderWidth){(h), (h), (v), (v), 0})
#define BorderOnly(...) ((CC_BorderWidth){__VA_ARGS__})

/* ------- Corner radius -------------------------------------------------
 *
 *   RadiusAll(n)           — all four corners = n. Use large values
 *                            (e.g. 999) for a pill/circle shape.
 *   RadiusTopBottom(t, b)  — top corners = t, bottom corners = b.
 *                            Useful for cards with a rounded top edge
 *                            and a flat bottom.
 *   RadiusLeftRight(l, r)  — left corners = l, right corners = r.
 *   RadiusOnly(.topLeft = 8,
 *              .topRight = 8) — designated init; unspecified corners
 *                            default to 0.
 */
#define RadiusAll(n) CLAY_CORNER_RADIUS(n)
#define RadiusTopBottom(t, b) ((CC_CornerRadius){(t), (t), (b), (b)})
#define RadiusLeftRight(l, r) ((CC_CornerRadius){(l), (r), (l), (r)})
#define RadiusOnly(...) ((CC_CornerRadius){__VA_ARGS__})

/* ------- Color ---------------------------------------------------------
 *
 *   Color(r, g, b, a)      — a CC_Color literal. Channels are 0–255,
 *                            alpha included. Example:
 *                              .backgroundColor = Color(24, 24, 24, 255)
 *
 *   ColorHex(rgb)          — a CC_Color from a 0xRRGGBB hex literal,
 *                            with alpha forced to 255 (fully opaque):
 *                              .backgroundColor = ColorHex(0x1E1E22)
 *
 *   ColorHexA(rgba)        — a CC_Color from a 0xRRGGBBAA hex literal
 *                            that includes the alpha channel in the
 *                            bottom byte:
 *                              .overlayColor = ColorHexA(0x00000020)
 */
#define Color(r, g, b, a) ((CC_Color){(r), (g), (b), (a)})
#define ColorHex(rgb)                                                          \
  ((CC_Color){(float)(((uint32_t)(rgb) >> 16) & 0xFFu),                        \
              (float)(((uint32_t)(rgb) >> 8) & 0xFFu),                         \
              (float)((uint32_t)(rgb) & 0xFFu), 255.0f})
#define ColorHexA(rgba)                                                        \
  ((CC_Color){(float)(((uint32_t)(rgba) >> 24) & 0xFFu),                       \
              (float)(((uint32_t)(rgba) >> 16) & 0xFFu),                       \
              (float)(((uint32_t)(rgba) >> 8) & 0xFFu),                        \
              (float)((uint32_t)(rgba) & 0xFFu)})

/* ------- Child alignment ------------------------------------------------
 *
 * Used as the `.childAlignment` of a CC_LayoutConfig, e.g.
 *
 *     .layout = { .childAlignment = { .x = AlignXCenter(),
 *                                     .y = AlignYCenter() } }
 *
 *   Horizontal (x):  AlignXStart | AlignXCenter | AlignXEnd
 *   Vertical   (y):  AlignYStart | AlignYCenter | AlignYEnd
 *
 * Names are axis-tagged so they can't be silently mixed up — assigning
 * a Y-axis value to `.x` is a compile-time type mismatch.
 *
 * Alignment applies to children as a group along the cross-axis of the
 * parent's layout direction. In a Row, `.y = AlignYCenter()` vertically
 * centers every child; in a Column, `.x = AlignXCenter()` horizontally
 * centers every child.
 */
#define AlignXStart() CLAY_ALIGN_X_LEFT
#define AlignXCenter() CLAY_ALIGN_X_CENTER
#define AlignXEnd() CLAY_ALIGN_X_RIGHT
#define AlignYStart() CLAY_ALIGN_Y_TOP
#define AlignYCenter() CLAY_ALIGN_Y_CENTER
#define AlignYEnd() CLAY_ALIGN_Y_BOTTOM

/* ChildAlign — sugar for the `.childAlignment` field. Designated init
 * so you can set one axis or both:
 *
 *     .layout = { .childAlignment = ChildAlign(.x = AlignXCenter(),
 *                                              .y = AlignYCenter()) }
 *
 * Unspecified axis defaults to the Start of that axis (X = left,
 * Y = top), matching Clay's default.
 */
#define ChildAlign(...) ((CC_ChildAlignment){__VA_ARGS__})

/* ------- Floating shorthands ------------------------------------------
 *
 * Sugar for the two most common `.floating` patterns: overlay anchored
 * to the immediate parent (background image, badge), and overlay
 * anchored to the root (modal, app-level toast).
 *
 *   FloatOnParent(z)  — attach to parent at z-index z.
 *   FloatOnRoot(z)    — attach to root at z-index z.
 *
 * For attach-to-by-id, custom attach points, or offsets, write the
 * literal `CC_FloatingElementConfig` directly.
 */
#define FloatOnParent(z)                                                       \
  ((CC_FloatingElementConfig){.attachTo = CC_ATTACH_TO_PARENT, .zIndex = (z)})
#define FloatOnRoot(z)                                                         \
  ((CC_FloatingElementConfig){.attachTo = CC_ATTACH_TO_ROOT, .zIndex = (z)})

/* ------- Clip viewport shorthands -------------------------------------
 *
 * Sugar for the `.clip` field that turns an element into a scroll
 * viewport on one or both axes.
 *
 *   ClipX(offset)   — horizontal scroll, .childOffset = offset.
 *   ClipY(offset)   — vertical scroll, .childOffset = offset.
 *   ClipXY(offset)  — both axes, .childOffset = offset.
 *
 * `offset` is a CC_Vector2. Pair with a CC_Scroll state and
 * CC_ScrollUpdate to drive the offset from mouse wheel + drag:
 *
 *     CC_ScrollUpdate(&s, "List",
 *                     .vertical = true, .wheel = true, .drag = true);
 *     Column("List", .clip = ClipY(s.offset), ...) { ... }
 */
#define ClipX(offset)                                                          \
  ((CC_ClipElementConfig){.horizontal = true, .childOffset = (offset)})
#define ClipY(offset)                                                          \
  ((CC_ClipElementConfig){.vertical = true, .childOffset = (offset)})
#define ClipXY(offset)                                                         \
  ((CC_ClipElementConfig){                                                     \
      .horizontal = true, .vertical = true, .childOffset = (offset)})

/* =========================================================================
 * Lifecycle
 * =========================================================================
 *
 * ccompose bundles a raylib-based window, a Clay arena, and a default
 * text measure function behind six lifecycle calls. The typical program
 * shape is:
 *
 *     CC_SetWindow(w, h, title);   // optional, before CC_Init
 *     CC_Init();                   // once at startup
 *     while (CC_Running()) {       // main loop
 *         CC_Begin();              //   start a frame
 *         // ... Column/Row/Text macros here ...
 *         CC_End();                //   end frame, draw, present
 *     }
 *     CC_Shutdown();               // also runs via atexit()
 *
 * You don't have to call CC_Shutdown() — it's registered with atexit()
 * inside CC_Init() — but doing so makes teardown order explicit if you
 * care.
 *
 * ------- Overriding the raylib backend's defaults ------------------------
 *
 * Override BEFORE CC_Init() (they're captured at init time):
 *
 *     CC_SetWindow(1280, 720, "My App");
 *     CC_SetWindowFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
 *     CC_SetBackground(Color(0, 0, 0, 255));
 *     CC_SetErrorHandler(my_error_sink);
 *
 * Override AFTER CC_Init() by calling Clay directly — once the Clay
 * context exists, its setters just mutate its state:
 *
 *     Clay_SetMeasureTextFunction(my_measure, my_user_data);
 *     Clay_SetLayoutDimensions((CC_Dimensions){w, h});
 *     Clay_SetPointerState((CC_Vector2){mx, my}, down);
 *     Clay_SetMaxElementCount(16384);   // note: affects next reset only
 *
 * For custom fonts, use CC_LoadFont() which returns a fontId you pass
 * to Text(.fontId = ..., .fontSize = ...), or to CC_InitFont() to make
 * it the default for every Text().
 *
 * If you outgrow this lifecycle and want full control (custom arena,
 * multiple Clay contexts, a non-raylib renderer), ignore CC_Init /
 * CC_Begin / CC_End entirely and call Clay_Initialize / Clay_BeginLayout
 * / Clay_EndLayout yourself. Every element macro in this header works
 * identically regardless of how Clay was set up.
 */

/* Set the window size and title used by the next CC_Init() call. Has no
 * effect after CC_Init() (changing the window mid-session is raylib's
 * job — call SetWindowSize / SetWindowTitle directly). Title pointer
 * must outlive the call; passing NULL leaves the previous title. */
void CC_SetWindow(int width, int height, const char *title);

/* Set the raylib window flags (FLAG_WINDOW_RESIZABLE, FLAG_MSAA_4X_HINT,
 * FLAG_VSYNC_HINT, FLAG_WINDOW_HIGHDPI, etc.) used by the next CC_Init()
 * call. Default is RESIZABLE | MSAA_4X_HINT | VSYNC_HINT. */
void CC_SetWindowFlags(unsigned int raylib_flags);

/* Set the background color used by CC_End() (via ClearBackground). The
 * color applies on every subsequent frame — call again to change it. */
void CC_SetBackground(CC_Color color);

/* Install a custom Clay error handler. If unset, ccompose prints error
 * text to stderr. Must be called BEFORE CC_Init(); Clay bakes the
 * handler into its state on initialization. */
void CC_SetErrorHandler(void (*handler)(CC_ErrorData));

/* Idempotent: safe to call multiple times. First call:
 *   - Creates the raylib window with CC_SetWindow / CC_SetWindowFlags
 *   - Loads the default raylib font into slot 0
 *   - Allocates Clay's arena via Clay_MinMemorySize()
 *   - Calls Clay_Initialize with the stored viewport + error handler
 *   - Installs Raylib_MeasureText as Clay's measure function
 *   - Registers CC_Shutdown with atexit()
 * Subsequent calls are no-ops. */
void CC_Init(void);

/* Unload custom fonts, Clay_Raylib_Close (closes the window), free the
 * arena. Also runs automatically via atexit(), so explicit calls are
 * only needed when you want deterministic teardown order. Safe to call
 * multiple times. */
void CC_Shutdown(void);

/* Returns whether the window is still open (raylib's
 * !WindowShouldClose()). Use as your main-loop condition:
 *
 *     while (CC_Running()) { ... }
 *
 * In headless mode (CCOMPOSE_NO_BACKEND) this always returns true — you
 * have to break out of the loop yourself. */
bool CC_Running(void);

/* Start a new frame. Reads raylib's input state (window size, mouse
 * position, mouse button, scroll wheel, frame time), feeds it into Clay
 * via Clay_SetLayoutDimensions / Clay_SetPointerState /
 * Clay_UpdateScrollContainers, then calls Clay_BeginLayout(). After
 * this, Column/Row/Box/Text macros inside the current block get
 * recorded into Clay's layout. */
void CC_Begin(void);

/* End the current frame. Calls Clay_EndLayout(GetFrameTime()) to get
 * the render command list, then:
 *   - BeginDrawing()
 *   - ClearBackground(CC_SetBackground value)
 *   - Clay_Raylib_Render(commands, ccompose_fonts)
 *   - EndDrawing()
 *
 * Returns the same command array Clay emitted, in case you want to
 * inspect or post-process it — by the time you see the return value the
 * frame has already been drawn. In headless mode this just returns the
 * commands without drawing anything. */
CC_RenderCommandArray CC_End(void);

/* Run the main loop by repeatedly calling the user-supplied frame
 * function until the window closes. Portable across native and web:
 *
 *     static void frame(void) {
 *         CC_Begin();
 *         Column("Root", ...) { ... }
 *         CC_End();
 *     }
 *
 *     int main(void) {
 *         CC_SetWindow(...);
 *         CC_Init();
 *         CC_RUN_LOOP(frame);
 *         CC_Shutdown();
 *     }
 *
 * Native builds expand to `while (CC_Running()) frame();`. Emscripten
 * builds hand the frame off to `emscripten_set_main_loop` because the
 * browser owns the event loop — a plain while-loop would hang the tab.
 *
 * The frame function must take no arguments and return void. Pass shared
 * state through a file-scope pointer (see examples/demo.c). */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define CC_RUN_LOOP(frame_fn) emscripten_set_main_loop((frame_fn), 0, 1)
#else
#define CC_RUN_LOOP(frame_fn)                                                  \
  do {                                                                         \
    while (CC_Running())                                                       \
      (frame_fn)();                                                            \
  } while (0)
#endif

/* Callback signature for Draw / DrawRow / DrawColumn elements.
 *
 *   bb   — the bounding box Clay computed for the element, in screen
 *          pixels (top-left origin, y grows down). Use bb.x / bb.y /
 *          bb.width / bb.height to position raylib primitives.
 *   user — the opaque context pointer you passed to the macro. Typically
 *          a struct of per-draw state (colors, counters, animation
 *          phase, etc.). May be NULL if the callback doesn't need any. */
typedef void (*CC_DrawFn)(CC_BoundingBox bb, void *user);

/* One entry in ccompose's per-frame draw-slot pool. Constructed by
 * CC_AcquireDrawSlot and stashed in a Clay element's .custom.customData.
 * The patched raylib renderer checks `_tag` to distinguish ccompose
 * slots from other users of the CUSTOM render command channel (e.g.
 * the vendored 3D-model demo); don't set it yourself. */
typedef struct {
  int _tag;
  CC_DrawFn fn;
  void *user;
} CC_DrawSlot;

/* Acquire a CC_DrawSlot from ccompose's per-frame pool. The pool resets
 * at the start of every CC_Begin(), so the returned pointer is valid
 * until the next frame starts — which is exactly when the renderer stops
 * reading it. Returns NULL if the pool is full (CC_DRAW_POOL_SIZE entries
 * per frame); in that case the Draw element renders nothing.
 *
 * The Draw / DrawRow / DrawColumn macros wrap this — prefer them at call
 * sites. Call this directly only when attaching a draw callback to a
 * container created with a non-Draw macro (e.g. Element with a runtime
 * direction):
 *
 *     Element(direction, "canvas",
 *             .layout = {.sizing = {Grow(), Fixed(200)}},
 *             .custom = {.customData =
 *                            CC_AcquireDrawSlot(paint_fn, &state)}) {}
 */
CC_DrawSlot *CC_AcquireDrawSlot(CC_DrawFn fn, void *user);

/* Load a font file into a new slot. Returns the fontId to pass to
 * Text(.fontId = ..., .fontSize = ...), or -1 if the font couldn't be
 * loaded (raylib returns its default font on failure; we don't burn a
 * slot on that). Must be called AFTER CC_Init() so raylib has an active
 * GL context. The default raylib font is always present at id 0, so you
 * never need to load a font at all for basic rendering.
 *
 * Loaded fonts are automatically unloaded by CC_Shutdown(). Up to
 * CC_MAX_FONTS (16) custom slots are available.
 *
 * Example:
 *
 *     int FONT_TITLE = CC_LoadFont("resources/Roboto-Bold.ttf", 48);
 *     ...
 *     Text("Big title",
 *          .fontId   = FONT_TITLE,
 *          .fontSize = 48,
 *          .textColor = Color(255, 255, 255, 255));
 */
int CC_LoadFont(const char *path, int base_size);

/* Make an already-loaded font the global default text font, and set the
 * global default text size.
 *
 * Pass a fontId returned by CC_LoadFont() (or 0 for the raylib default
 * font). Text() / TextN() use these defaults whenever the call does not
 * explicitly choose a non-zero .fontId / .fontSize:
 *
 *   int body = CC_LoadFont("resources/Inter-Regular.ttf", 28);
 *   CC_InitFont(body, 28);
 *
 *   Text("A");                                  // body font, size 28
 *   Text("B", .fontSize = 16);                  // body font, size 16
 *   Text("C", .fontId = TITLE_ID);              // explicit override
 *   Text("D", .fontId = 0);                     // also uses global font
 *
 * base_size <= 0 leaves the global text size unchanged; it starts at
 * CC_DEFAULT_FONT_SIZE (16).
 *
 * Returns:
 *   - fontId on success
 *   - -1 if fontId was never handed out by CC_LoadFont (globals are left
 *     unchanged)
 *
 * Works in headless mode (CCOMPOSE_NO_BACKEND) — the ids and sizes flow
 * into emitted render commands either way; only the bounds check differs
 * (no fonts are ever loaded, so any id Clay's uint16_t fontId field can
 * carry is accepted).
 */
int CC_InitFont(int fontId, int base_size);

/* Returns the currently configured global text font id.
 * Defaults to 0 (raylib default font). */
int CC_GetGlobalFontId(void);

/* Returns the currently configured global text size.
 * Defaults to CC_DEFAULT_FONT_SIZE (16). */
int CC_GetGlobalFontSize(void);

/* Set the project-wide default text color used by every Text(...)
 * element that does not specify its own .textColor. Defaults to
 * ColorHex(0xFFFFFF) (opaque white) at startup.
 *
 *   CC_SetGlobalFontColor(ColorHex(0xDCDCDC));
 *   Text("status", .fontSize = 14);  // renders with the global color
 *   Text("error",  .fontSize = 14,
 *        .textColor = Color(230, 80, 80, 255));  // per-call override
 *
 * Returns 0. Safe to call before or after CC_Init(); the value is
 * stored in a file-scope static and read at every Text() expansion.
 * Works identically in headless mode — the color flows into emitted
 * text render commands either way. */
int CC_SetGlobalFontColor(CC_Color color);

/* Returns the currently configured global text color. */
CC_Color CC_GetGlobalFontColor(void);

#ifndef CCOMPOSE_NO_BACKEND
/* Load an image file from disk and upload it to the GPU as a Texture2D
 * that can be passed to the Image() macro. Thin wrapper over raylib's
 * LoadTexture — supports PNG, JPG, BMP, TGA, GIF, QOI, and anything else
 * raylib handles.
 *
 * Must be called AFTER CC_Init() so raylib has an active GL context.
 * Call CC_UnloadImage() before CC_Shutdown() to release the GPU texture
 * (or let process exit clean it up in throwaway programs).
 *
 * On failure raylib returns a zero-id Texture2D; check tex.id == 0 if
 * you need to distinguish load errors from success.
 *
 * Example:
 *
 *     CC_Init();
 *     Texture2D avatar = CC_LoadImage("resources/avatar.png");
 *     while (CC_Running()) {
 *         CC_Begin();
 *         Image("Avatar", &avatar, .layout = {.sizing = {Fixed(64),
 * Fixed(64)}}); CC_End();
 *     }
 *     CC_UnloadImage(avatar);
 *     CC_Shutdown();
 *
 * Not available in headless mode (CCOMPOSE_NO_BACKEND) — Texture2D is a
 * raylib type, so the declaration is compiled out entirely. */
Texture2D CC_LoadImage(const char *path);

/* Release a texture previously returned by CC_LoadImage(). Thin wrapper
 * over UnloadTexture. Safe to call on a zero-id texture. Must be called
 * before CC_Shutdown() while the GL context is still alive. */
void CC_UnloadImage(Texture2D texture);
#endif

/* Set the Clay viewport for headless mode (CCOMPOSE_NO_BACKEND). When
 * the raylib backend is active this is effectively a no-op — CC_Begin
 * reads GetScreenWidth/GetScreenHeight every frame and overwrites
 * whatever you set here. Useful in tests to give Clay a deterministic
 * frame size without opening a real window. */
void CC_SetViewport(float width, float height);

/* =========================================================================
 * Element scope — Column, Row, Box, Element
 * =========================================================================
 *
 * All four macros open a Clay element, run your block as its children,
 * and close the element on exit. They take the same arguments:
 *
 *     Column("id",  .field = value, ...) { children }
 *     Row("id",     .field = value, ...) { children }
 *     Box("id",     .field = value, ...) { children }
 *     Element(dir, "id", .field = value, ...) { children }
 *
 * The first argument is a **string literal** ID. Pass "" for anonymous
 * elements — Clay still generates an internal ID for layout bookkeeping,
 * but you can't target it from Clay_OnHover, CC_PointerOver,
 * scroll-offset persistence, or floating .parentId anchoring. Use a
 * real ID when you need any of those features, "" when you don't.
 *
 * The macro enforces the string-literal constraint at compile time via
 * the `"" id ""` literal-concatenation trick: pass a const char*, NULL,
 * an int, or anything else and you'll get a compile error right at the
 * call site.
 *
 * The remaining arguments are `CC_ElementDeclaration` designated
 * initializers. **Every Clay field is available** — ccompose doesn't
 * wrap or rename anything:
 *
 *     .layout           — CC_LayoutConfig: sizing, padding, gap,
 *                         alignment, layoutDirection (overwritten by
 *                         Column/Row/Box to guarantee the direction).
 *     .backgroundColor  — CC_Color. If you also set .image, this acts
 *                         as a tint on the image.
 *     .overlayColor     — color blended over the element and its
 *                         children ("mix" in GLSL terms).
 *     .cornerRadius     — CC_CornerRadius (use RadiusAll(n) for a
 *                         uniform radius, or build by hand for per-
 *                         corner control).
 *     .image            — CC_ImageElementConfig, a .imageData void*
 *                         passed to the renderer (Texture2D* for the
 *                         raylib backend). Turns the element into an
 *                         IMAGE render command.
 *     .floating         — CC_FloatingElementConfig: takes the element
 *                         out of normal flow, positions it relative to
 *                         a parent / the root / a specific ID, with a
 *                         zIndex and optional attachPoints.
 *     .clip             — CC_ClipElementConfig: .horizontal/.vertical
 *                         booleans + .childOffset Vector2 to make the
 *                         element a scroll viewport.
 *     .aspectRatio      — CC_AspectRatioElementConfig: width/height
 *                         ratio as a single float.
 *     .border           — CC_BorderElementConfig: color + per-side
 *                         pixel widths.
 *     .custom           — CC_CustomElementConfig: .customData void*
 *                         that becomes a CUSTOM render command the
 *                         renderer can draw however it wants.
 *     .userData         — arbitrary void* forwarded to every render
 *                         command generated by this element.
 *
 * See include/clay.h for each sub-struct's full field list — ccompose
 * deliberately doesn't duplicate that documentation.
 *
 * ------- Why three macros if Clay only has two directions? --------------
 *
 * Column  → CLAY_TOP_TO_BOTTOM. Vertical stacks, lists, sidebars.
 * Row     → CLAY_LEFT_TO_RIGHT. Horizontal stacks, toolbars, split panes.
 * Box     → CLAY_TOP_TO_BOTTOM (same as Column internally) — the name
 *           signals "direction doesn't matter here" for two patterns:
 *             1. Single-child decoration / frame (image box, avatar,
 *                badge, aspect-ratio container).
 *             2. Stacked overlays where every child uses .floating to
 *                take itself out of normal flow.
 *
 * Element(direction, "id", ...) is the generic form: use it when you
 * need to choose the direction at runtime (e.g. a toolbar that flips
 * between horizontal and vertical based on window orientation).
 *
 * ------- Nesting and shadowing ------------------------------------------
 *
 * The macros use __COUNTER__ to generate unique scope variable names,
 * so nested blocks don't trigger -Wshadow even at arbitrary depth:
 *
 *     Column("A") {
 *         Row("B") {
 *             Box("C") {
 *                 Column("", .layout = { .childGap = 4 }) {
 *                     Text("deep",
 *                          .textColor = Color(255,255,255,255),
 *                          .fontSize  = 12);
 *                 }
 *             }
 *         }
 *     }
 *
 * ------- Control flow caveats -------------------------------------------
 *
 * The macro expands to a `for` loop with a single iteration. Normal
 * `break` statements inside the block will exit the loop WITHOUT
 * running the close step — leaving a dangling open element in Clay's
 * internal stack, which Clay will report as an UNBALANCED_OPEN_CLOSE
 * error at end-of-layout. If you need early exit, use `continue`
 * (which runs the loop's step clause, closing the element), or wrap
 * the block in a helper function and `return`.
 *
 * Don't put the macro as the unbraced body of an `if` / `while` /
 * `else` — wrap in `{ ... }` if you need that shape, because the
 * macro expands to a statement that wants to own its own scope.
 */

/* A single-iteration-loop scope tracker. You shouldn't create these
 * directly — the Column/Row/Box macros construct them. */
typedef struct {
  int active;
} CC_Scope;

/* Opens a Clay element with the given id (or anonymously if id.length
 * is 0), forces decl.layout.layoutDirection to `direction`, and configures
 * the element via Clay__ConfigureOpenElement(). You generally don't
 * call this directly — it's the runtime hook the Column/Row/Box macros
 * expand into. */
CC_Scope CC_OpenElement(CC_String id, CC_LayoutDirection direction,
                        CC_ElementDeclaration decl);

/* Closes the element opened by CC_OpenElement. Idempotent: only closes
 * once per scope, subsequent calls are no-ops. */
void CC_CloseScope(CC_Scope *scope);

/* __COUNTER__ guarantees a unique scope variable name per call site, so
 * nested Column/Row/Box blocks don't trigger -Wshadow. */
#define CC_JOIN2_(a, b) a##b
#define CC_JOIN_(a, b) CC_JOIN2_(a, b)
#define CC_SCOPE_NAME_(c) CC_JOIN_(cc_scope_, CC_JOIN_(c, _))

/* CC_ELEMENT_IMPL_ takes a CC_String id (built by CC__Str in the
 * Element macro) and wraps the element body in a single-iteration
 * for loop. */
#define CC_ELEMENT_IMPL_(scope, direction, id_string, ...)                     \
  for (CC_Scope scope = CC_OpenElement((id_string), (direction),               \
                                       (CC_ElementDeclaration){__VA_ARGS__});  \
       scope.active; CC_CloseScope(&scope))

/* Element — the generic container. Use when you need to pick the
 * layout direction at runtime; otherwise prefer Column / Row / Box.
 *
 *     Element(orientation == VERTICAL ? CLAY_TOP_TO_BOTTOM
 *                                     : CLAY_LEFT_TO_RIGHT,
 *             "Toolbar", .layout = { .childGap = 8 }) { ... }
 */
#define Element(direction, id_literal, ...)                                    \
  CC_ELEMENT_IMPL_(CC_SCOPE_NAME_(__COUNTER__), (direction),                   \
                   CC__Str(id_literal), __VA_ARGS__)

/* Column — vertical stack (CLAY_TOP_TO_BOTTOM). Children are laid out
 * from top to bottom with `.layout.childGap` between them.
 *
 *     Column("Sidebar",
 *            .layout = { .sizing   = { Fixed(220), Grow() },
 *                        .padding  = PadAll(16),
 *                        .childGap = 8 },
 *            .backgroundColor = COLOR_SURFACE,
 *            .cornerRadius    = RadiusAll(8)) {
 *         Text("Menu", .textColor = COLOR_TEXT, .fontSize = 12);
 *         // ...
 *     }
 */
#define Column(id_literal, ...)                                                \
  Element(CLAY_TOP_TO_BOTTOM, id_literal, __VA_ARGS__)

/* Row — horizontal stack (CLAY_LEFT_TO_RIGHT). Children are laid out
 * left to right with `.layout.childGap` between them. Use
 * `.layout.childAlignment.y = AlignYCenter()` to vertically center the
 * children within the row's height.
 *
 *     Row("Toolbar",
 *         .layout = { .sizing = { Grow(), Fixed(40) },
 *                     .padding = PadAll(8),
 *                     .childGap = 12,
 *                     .childAlignment = { .y = AlignYCenter() } }) {
 *         Text("File", .textColor = COLOR_TEXT, .fontSize = 14);
 *         Text("Edit", .textColor = COLOR_TEXT, .fontSize = 14);
 *         Text("View", .textColor = COLOR_TEXT, .fontSize = 14);
 *     }
 */
#define Row(id_literal, ...)                                                   \
  Element(CLAY_LEFT_TO_RIGHT, id_literal, __VA_ARGS__)

/* Box — a direction-neutral container. Clay has no native z-stack
 * layout mode (children always flow LTR or TTB), so Box is most useful
 * for two patterns:
 *
 *   1. Single-child decoration / frame. The child fills the box (e.g.
 *      via `.layout.sizing = { Grow(), Grow() }`) and the box carries
 *      the styling:
 *
 *          Box("Avatar",
 *              .layout = { .sizing = { Fixed(64), Fixed(64) } },
 *              .image  = { .imageData = my_texture },
 *              .cornerRadius = RadiusAll(999)) { }
 *
 *   2. Stacked / overlay children — each child opts into .floating so
 *      it's taken out of normal flow and layered on top:
 *
 *          Box("Card",
 *              .layout = { .sizing = { Grow(), Fixed(220) } },
 *              .backgroundColor = COLOR_SURFACE,
 *              .cornerRadius    = RadiusAll(12)) {
 *
 *              // background image layer
 *              Box("CardBg",
 *                  .layout = { .sizing = { Grow(), Grow() } },
 *                  .image  = { .imageData = bg_texture },
 *                  .floating = { .attachTo = CLAY_ATTACH_TO_PARENT,
 *                                .zIndex = 0 }) { }
 *
 *              // foreground text layer
 *              Column("CardText",
 *                  .layout = { .padding = PadAll(16) },
 *                  .floating = { .attachTo = CLAY_ATTACH_TO_PARENT,
 *                                .zIndex = 1 }) {
 *                  Text("Title", .textColor = COLOR_TEXT,
 *                                .fontSize = 24);
 *              }
 *          }
 *
 * Internally Box maps to CLAY_TOP_TO_BOTTOM — any direction works when
 * there are 0–1 normal children or when all children are floating. */
#define Box(id_literal, ...)                                                   \
  Element(CLAY_TOP_TO_BOTTOM, id_literal, __VA_ARGS__)

/* =========================================================================
 * Spacer / Divider — layout leaves
 * =========================================================================
 *
 * These are leaf calls (no block, no id). Internally they open an anonymous
 * Clay element, configure it and close it in one call — so they are safe to
 * use in loops.
 *
 *     Row("Toolbar"){
 *         Text("File", ...);
 *         HSpacer();
 *         Text("Edit", ...);
 *     }
 *
 *     Column("Menu"){
 *         Text("Item A", ...);
 *         HDivider();
 *         HDivider(.thickness = 2, .color = Color(0x33, 0x33, 0x33, 0xff));
 *         Text("Item B", ...);
 *     }
 */
typedef struct {
  float length;    /* size along the divider's flow axis (width for HDivider,
                      height for VDivider). 0 = Grow() — fill the cross-axis. */
  float thickness; /* size across the divider's flow axis. Default 1px. */
  CC_Color color;  /* defaults to the global font color when {0,0,0,0}. */
} CC_DividerOpts;

/* Spacer opts — both axes default to Fit() (zero-initialized SizingAxis is
 * FIT since CLAY__SIZING_TYPE_FIT == 0). Override either axis with any sugar
 * helper:
 *
 *     Spacer(.width = Grow());                // flexible horizontal spacer
 *     Spacer(.height = Fixed(12));            // 12px vertical gap
 *     Spacer(.width = Grow(), .height = Grow()); // grow on both axes
 */
typedef struct {
  Clay_SizingAxis width;
  Clay_SizingAxis height;
} CC_SpacerOpts;

void CC_HSpacer(void); /* grows on width  — flexible gap inside a Row */
void CC_VSpacer(void); /* grows on height — flexible gap inside a Column */
void CC_Spacer(CC_SpacerOpts opts);
void CC_HDivider(CC_DividerOpts opts);
void CC_VDivider(CC_DividerOpts opts);

#define HSpacer() CC_HSpacer()
#define VSpacer() CC_VSpacer()
#define Spacer(...) CC_Spacer((CC_SpacerOpts){__VA_ARGS__})
#define HDivider(...) CC_HDivider((CC_DividerOpts){__VA_ARGS__})
#define VDivider(...) CC_VDivider((CC_DividerOpts){__VA_ARGS__})

/* Draw / DrawRow / DrawColumn — layout elements whose computed bounding
 * box is handed to a user callback, so you can issue arbitrary raylib
 * draw calls positioned and sized by Clay.
 *
 * The callback has the signature:
 *
 *     void paint(CC_BoundingBox bb, void *user);
 *
 * It fires once per frame, inline at this element's position in the
 * render command stream (so children stack on top, and siblings rendered
 * after still paint on top). `user` is the opaque context pointer you
 * passed to the macro — it must stay valid until the end of CC_End().
 *
 * Three flavors differ only in child layout direction:
 *     Draw        — top-to-bottom (same as Box).
 *     DrawRow     — left-to-right (same as Row).
 *     DrawColumn  — top-to-bottom (same as Column).
 *
 * Leaf form (no children):
 *     Draw("canvas", paint_fn, &state,
 *          .layout = {.sizing = {Grow(), Fixed(200)}},
 *          .cornerRadius = RadiusAll(8));
 *
 * Block form (children stack on top of the callback's output):
 *     Draw("canvas", paint_fn, &state, .layout = {...}) {
 *         Text("Overlay", .fontSize = 20);
 *     }
 *
 * `.custom` is assigned last inside the macro, so a user-supplied
 * `.custom = {...}` field is silently overwritten — the slot owns
 * that channel. Don't pass your own `.custom`.
 *
 * Slot storage is a fixed-size per-frame pool (see CC_AcquireDrawSlot).
 * No heap allocation, auto-reset every CC_Begin(). */
#define Draw(id_literal, fn_, user_, ...)                                      \
  Box((id_literal), __VA_ARGS__,                                               \
      .custom = {.customData = CC_AcquireDrawSlot((fn_), (user_))})

#define DrawRow(id_literal, fn_, user_, ...)                                   \
  Row((id_literal), __VA_ARGS__,                                               \
      .custom = {.customData = CC_AcquireDrawSlot((fn_), (user_))})

#define DrawColumn(id_literal, fn_, user_, ...)                                \
  Column((id_literal), __VA_ARGS__,                                            \
         .custom = {.customData = CC_AcquireDrawSlot((fn_), (user_))})

/* =========================================================================
 * Transitions
 * =========================================================================
 *
 * DefineTransition declares a reusable transition preset at file scope.
 * It generates static enter/exit callback functions and a static inline
 * config function that you call by name:
 *
 *     DefineTransition(sidebar_anim, 0.4f,
 *         .enter = { .slideX = -240, .fade = true },
 *         .exit  = { .slideX = -240, .fade = true },
 *     );
 *
 *     Column("Sidebar", .transition = sidebar_anim(), ...) { ... }
 *
 * High-level presets (slideX/Y, fade) and raw Clay properties
 * (backgroundColor, overlayColor, borderColor, borderWidth) can be
 * mixed freely. The macro auto-infers the CC_TransitionProperty
 * bitmask from the fields you set.
 */

/* Effect description for one side of a transition (enter or exit). */
typedef struct CC_TransitionEffect {
  /* High-level presets. */
  float slideX; /* Pixel offset on X axis. Negative = left. */
  float slideY; /* Pixel offset on Y axis. Negative = up.   */
  bool fade;    /* true = animate backgroundColor alpha from/to 0. */

  /* Raw Clay properties — set these for fine-grained control.
   * A zero-valued color (all fields 0) is treated as "not set" and
   * won't contribute to the properties bitmask. */
  CC_Color backgroundColor;
  CC_Color overlayColor;
  CC_Color borderColor;
  CC_BorderWidth borderWidth;

  /* Enter-specific: when the enter transition fires relative to
   * the parent's first frame. Default (0) = SKIP_ON_FIRST_PARENT_FRAME. */
  CC_TransitionEnterTrigger enterTrigger;

  /* Exit-specific: when the exit transition fires relative to
   * the parent exiting. Default (0) = SKIP_WHEN_PARENT_EXITS. */
  CC_TransitionExitTrigger exitTrigger;

  /* Exit-specific: z-ordering of the exiting element among siblings.
   * Default (0) = UNDERNEATH_SIBLINGS. */
  CC_ExitTransitionOrdering siblingOrdering;
} CC_TransitionEffect;

/* ------- Internal: property bitmask inference --------------------------
 *
 * These macros inspect a CC_TransitionEffect to build the correct
 * CC_TransitionProperty bitmask. Zero-valued fields are treated as unset.
 */

/* True if a CC_Color has any non-zero channel. */
#define CC__COLOR_SET_(c) ((c).r != 0 || (c).g != 0 || (c).b != 0 || (c).a != 0)

/* True if a CC_BorderWidth has any non-zero edge. */
#define CC__BORDER_WIDTH_SET_(bw)                                              \
  ((bw).left != 0 || (bw).right != 0 || (bw).top != 0 || (bw).bottom != 0 ||   \
   (bw).betweenChildren != 0)

/* Compute property flags from an effect. */
#define CC__EFFECT_PROPS_(e)                                                   \
  (((e).slideX != 0 ? CC_TRANSITION_PROPERTY_X : 0) |                          \
   ((e).slideY != 0 ? CC_TRANSITION_PROPERTY_Y : 0) |                          \
   ((e).fade ? CC_TRANSITION_PROPERTY_BACKGROUND_COLOR : 0) |                  \
   (CC__COLOR_SET_((e).backgroundColor)                                        \
        ? CC_TRANSITION_PROPERTY_BACKGROUND_COLOR                              \
        : 0) |                                                                 \
   (CC__COLOR_SET_((e).overlayColor) ? CC_TRANSITION_PROPERTY_OVERLAY_COLOR    \
                                     : 0) |                                    \
   (CC__COLOR_SET_((e).borderColor) ? CC_TRANSITION_PROPERTY_BORDER_COLOR      \
                                    : 0) |                                     \
   (CC__BORDER_WIDTH_SET_((e).borderWidth)                                     \
        ? CC_TRANSITION_PROPERTY_BORDER_WIDTH                                  \
        : 0))

/* Check whether an effect has any fields set at all. */
#define CC__EFFECT_ACTIVE_(e)                                                  \
  ((e).slideX != 0 || (e).slideY != 0 || (e).fade ||                           \
   CC__COLOR_SET_((e).backgroundColor) || CC__COLOR_SET_((e).overlayColor) ||  \
   CC__COLOR_SET_((e).borderColor) || CC__BORDER_WIDTH_SET_((e).borderWidth))

/* ------- DefineTransition macro ----------------------------------------
 *
 * Usage (file scope only):
 *
 *   DefineTransition(name, duration,
 *       [.handler    = easing_fn,]
 *       [.properties = CC_TRANSITION_PROPERTY_*,]
 *       [.interactionHandling = ...,]
 *       .enter = { .slideX = -240, .fade = true },
 *       .exit  = { .slideX = -240, .fade = true },
 *   );
 *
 * Expands to:
 *   - static enter/exit callback functions (name##__enter__ / name##__exit__)
 *   - a static inline function returning CC_TransitionElementConfig
 *
 * Reference as: .transition = name()
 *
 * If .handler is omitted, defaults to CC_EaseOut.
 * If .properties is omitted, auto-inferred from enter/exit fields.
 */

/* Internal struct carrying the full DefineTransition(...) arguments. */
typedef struct CC__TransitionDef {
  bool (*handler)(CC_TransitionArgs);
  float duration;
  CC_TransitionProperty properties;
  CC_TransitionInteraction interactionHandling;
  CC_TransitionEffect enter;
  CC_TransitionEffect exit;
} CC__TransitionDef;

/* Apply a CC_TransitionEffect to a CC_TransitionData (shared by
 * enter and exit callbacks). */
#define CC__APPLY_EFFECT_(t, e)                                                \
  do {                                                                         \
    (t).boundingBox.x += (e).slideX;                                           \
    (t).boundingBox.y += (e).slideY;                                           \
    if ((e).fade)                                                              \
      (t).backgroundColor.a = 0;                                               \
    if (CC__COLOR_SET_((e).backgroundColor))                                   \
      (t).backgroundColor = (e).backgroundColor;                               \
    if (CC__COLOR_SET_((e).overlayColor))                                      \
      (t).overlayColor = (e).overlayColor;                                     \
    if (CC__COLOR_SET_((e).borderColor))                                       \
      (t).borderColor = (e).borderColor;                                       \
    if (CC__BORDER_WIDTH_SET_((e).borderWidth))                                \
      (t).borderWidth = (e).borderWidth;                                       \
  } while (0)

#define DefineTransition(name, dur, ...)                                       \
  /* Enter callback: applies enter effects to target state. */                 \
  static CC_TransitionData name##__enter__(CC_TransitionData t,                \
                                           CC_TransitionProperty p) {          \
    (void)p;                                                                   \
    CC_TransitionEffect e =                                                    \
        ((CC__TransitionDef){.duration = (dur), __VA_ARGS__}).enter;           \
    CC__APPLY_EFFECT_(t, e);                                                   \
    return t;                                                                  \
  }                                                                            \
  /* Exit callback: applies exit effects to initial state. */                  \
  static CC_TransitionData name##__exit__(CC_TransitionData t,                 \
                                          CC_TransitionProperty p) {           \
    (void)p;                                                                   \
    CC_TransitionEffect e =                                                    \
        ((CC__TransitionDef){.duration = (dur), __VA_ARGS__}).exit;            \
    CC__APPLY_EFFECT_(t, e);                                                   \
    return t;                                                                  \
  }                                                                            \
  /* Config function: returns a fully built CC_TransitionElementConfig. */     \
  /* Usage: .transition = name()                                          */   \
  static inline CC_TransitionElementConfig name(void) {                        \
    CC__TransitionDef d = {.duration = (dur), __VA_ARGS__};                    \
    CC_TransitionProperty props =                                              \
        d.properties                                                           \
            ? d.properties                                                     \
            : (CC__EFFECT_PROPS_(d.enter) | CC__EFFECT_PROPS_(d.exit));        \
    return (CC_TransitionElementConfig){                                       \
        .handler = d.handler ? d.handler : CC_EaseOut,                         \
        .duration = d.duration,                                                \
        .properties = props,                                                   \
        .interactionHandling = d.interactionHandling,                          \
        .enter =                                                               \
            {                                                                  \
                .setInitialState =                                             \
                    CC__EFFECT_ACTIVE_(d.enter) ? name##__enter__ : NULL,      \
                .trigger = d.enter.enterTrigger,                               \
            },                                                                 \
        .exit =                                                                \
            {                                                                  \
                .setFinalState =                                               \
                    CC__EFFECT_ACTIVE_(d.exit) ? name##__exit__ : NULL,        \
                .trigger = d.exit.exitTrigger,                                 \
                .siblingOrdering = d.exit.siblingOrdering,                     \
            },                                                                 \
    };                                                                         \
  }                                                                            \
  typedef int name##__semicolon_absorber__

/* =========================================================================
 * Image
 * =========================================================================
 *
 * Leaf element — not a scoped block. Image() takes a string-literal id,
 * a `CC_ImageRef *` (texture + scale mode), and optional Clay fields:
 *
 *     Image("Avatar", ImgCrop(&avatar_tex),
 *           .layout       = { .sizing = { Fixed(64), Fixed(64) } },
 *           .cornerRadius = RadiusAll(999));
 *
 * ------- CC_ImageRef and scale modes ------------------------------------
 *
 * The second argument is a `CC_ImageRef *` — a small struct pairing a
 * `Texture2D *` with a `CC_ImageScale` enum that tells the renderer how
 * the texture should fit inside its element bounding box:
 *
 *   CC_IMAGE_FILL  — stretch to fill the box, aspect ratio ignored.
 *                    This is the default and the zero value, so a bare
 *                    `{&tex}` behaves like Fill.
 *   CC_IMAGE_FIT   — "contain": preserve aspect, shrink to fit inside
 *                    the box, with transparent letterbox / pillarbox
 *                    margins on whichever axis has slack.
 *   CC_IMAGE_CROP  — "cover": preserve aspect, scale so the image
 *                    covers the whole box, cropping overflow evenly
 *                    from both sides of the longer axis.
 *
 * Three sugar macros exist so you don't have to type compound literals:
 *
 *     ImgFill(&tex)  →  &(CC_ImageRef){&tex, CC_IMAGE_FILL}
 *     ImgFit(&tex)   →  &(CC_ImageRef){&tex, CC_IMAGE_FIT}
 *     ImgCrop(&tex)  →  &(CC_ImageRef){&tex, CC_IMAGE_CROP}
 *
 * The compound literal lives until the end of the enclosing block, so
 * passing them directly as an argument to Image() inside a frame's
 * BuildUI() is safe — they're valid until CC_End() returns.
 *
 * ------- cornerRadius ---------------------------------------------------
 *
 * Image honors `.cornerRadius` on all three scale modes — the raylib
 * backend masks through a cached RenderTexture2D, so `RadiusAll(999)`
 * gives you a circular avatar regardless of scale mode. Zero radius
 * (the default) skips the mask pass for the fast path.
 *
 * ------- Other Clay fields ----------------------------------------------
 *
 * Image accepts every CC_ElementDeclaration field the container macros
 * do — .layout, .backgroundColor (used as the image tint on the raylib
 * backend), .border, .floating, etc. See the Column/Row/Box section
 * above for the full menu. */

#ifndef CC_IMAGE_REF_DEFINED_
#define CC_IMAGE_REF_DEFINED_ 1
typedef enum {
  CC_IMAGE_FILL = 0, /* default: stretch to fill box, aspect ignored */
  CC_IMAGE_FIT,      /* contain: preserve aspect, letterbox margins   */
  CC_IMAGE_CROP,     /* cover: preserve aspect, crop overflow         */
} CC_ImageScale;
#endif

#ifndef CCOMPOSE_NO_BACKEND
#ifndef CC_IMAGE_REF_STRUCT_DEFINED_
#define CC_IMAGE_REF_STRUCT_DEFINED_ 1
typedef struct {
  Texture2D *texture;
  CC_ImageScale scale;
} CC_ImageRef;
#endif

/* Acquire a CC_ImageRef slot from ccompose's per-frame pool. The pool
 * resets at the start of every CC_Begin(), so the returned pointer is
 * valid until the next frame starts — which is exactly when Clay stops
 * reading it. Returns NULL if the pool is full (CC_IMAGE_REF_POOL_SIZE
 * entries per frame); in that case Image() will silently draw nothing.
 *
 * The ImgFill / ImgFit / ImgCrop sugar macros below wrap this — prefer
 * them at call sites. */
CC_ImageRef *CC_AcquireImageRef(Texture2D *texture, CC_ImageScale scale);

/* One-liner sugar for the common scale modes — uses the per-frame pool
 * so the returned pointer stays valid through CC_End(). */
#define ImgFill(tex_ptr) CC_AcquireImageRef((tex_ptr), CC_IMAGE_FILL)
#define ImgFit(tex_ptr) CC_AcquireImageRef((tex_ptr), CC_IMAGE_FIT)
#define ImgCrop(tex_ptr) CC_AcquireImageRef((tex_ptr), CC_IMAGE_CROP)
#endif

#define CC_IMAGE_IMPL_(scope, id_string, ref_expr, ...)                        \
  do {                                                                         \
    CC_Scope scope =                                                           \
        CC_OpenElement((id_string), CLAY_TOP_TO_BOTTOM,                        \
                       (CC_ElementDeclaration){                                \
                           __VA_ARGS__, .image = {.imageData = (ref_expr)}});  \
    CC_CloseScope(&scope);                                                     \
  } while (0)

#define Image(id_literal, ref_expr, ...)                                       \
  CC_IMAGE_IMPL_(CC_SCOPE_NAME_(__COUNTER__), CC__Str(id_literal), ref_expr,   \
                 __VA_ARGS__)

/* =========================================================================
 * Text
 * =========================================================================
 *
 * Leaf element — not a scoped block. Text() takes a string and a
 * `CC_TextElementConfig` literal with the text style:
 *
 *     Text("Hello, world!",
 *          .textColor     = Color(255, 255, 255, 255),
 *          .fontId        = 0,                       // default font
 *          .fontSize      = 14,
 *          .letterSpacing = 0,
 *          .lineHeight    = 0,
 *          .wrapMode      = CLAY_TEXT_WRAP_WORDS,    // default
 *          .textAlignment = CLAY_TEXT_ALIGN_LEFT);   // default
 *
 * See include/clay.h for the full CC_TextElementConfig field list.
 *
 * Clay needs a measure function to know how wide text will render.
 * ccompose installs raylib's Raylib_MeasureText by default, so out of
 * the box Text() works with the built-in raylib font at any size. To
 * use custom fonts:
 *
 *   - Per-call: use CC_LoadFont() and pass the returned id as .fontId.
 *   - Global default: pass the id from CC_LoadFont() to CC_InitFont();
 *     Text calls with omitted (or zero) .fontId will use that global id.
 *
 * Font-id precedence inside Text:
 *   1) if .fontId is non-zero, that explicit value wins
 *   2) otherwise, ccompose injects CC_GetGlobalFontId()
 *   3) if no global font was set, this falls back to 0 (raylib default)
 *
 * Font-size precedence is the same: an explicit non-zero .fontSize wins,
 * otherwise ccompose injects CC_GetGlobalFontSize().
 *
 * Clay does NOT copy the chars — the pointer must remain valid at
 * least until Clay_EndLayout() (i.e. until CC_End() returns). Stack
 * buffers inside the current frame's BuildUI() are fine; pointers to
 * freed memory are not.
 */

#define CC_DEFAULT_FONT_SIZE 16

/* String-literal text. Compile-time length via sizeof. */
static inline CC_TextElementConfig
CC__TextStyleWithGlobalFont(CC_TextElementConfig style) {
  if (style.fontId == 0) {
    int global_font = CC_GetGlobalFontId();
    style.fontId = (uint16_t)((global_font >= 0) ? global_font : 0);
  }

  if (style.fontSize == 0)
    style.fontSize = (uint16_t)CC_GetGlobalFontSize();

  if (style.textColor.a == 0 && style.textColor.r == 0 &&
      style.textColor.g == 0 && style.textColor.b == 0)
    style.textColor = CC_GetGlobalFontColor();

  return style;
}

/* Text element. Accepts any C string — length computed via strlen. */
#define Text(str, ...)                                                         \
  Clay__OpenTextElement(CC__Str(str), CC__TextStyleWithGlobalFont((            \
                                          CC_TextElementConfig){__VA_ARGS__}))

/* =========================================================================
 * Button + pointer interaction
 * =========================================================================
 *
 * Clay tracks pointer state against the previous frame's layout —
 * ccompose feeds it from inside CC_Begin(). Three pieces sit on top:
 *
 *   CC_Hovered("id")   — true while the mouse is over the element.
 *   CC_Clicked("id")   — true on the frame the left mouse button was
 *                        pressed while hovering that element.
 *   Button("id", ...)  — scoped block, same shape as Row/Column/Box,
 *                        opens a CLAY_LEFT_TO_RIGHT element. No
 *                        injected styling, no implicit hover
 *                        behavior — a semantic alias that pairs with
 *                        CC_Hovered / CC_Clicked in the IMGUI idiom.
 *
 * Typical pattern:
 *
 *     if (CC_Clicked("Save")) save_file();
 *
 *     Button("Save",
 *            .layout = { .padding        = PadAll(12),
 *                        .childAlignment = { .x = AlignXCenter(),
 *                                            .y = AlignYCenter() } },
 *            .backgroundColor = CC_Hovered("Save")
 *                                   ? Color( 80, 140, 220, 255)
 *                                   : Color( 40, 100, 200, 255),
 *            .cornerRadius    = RadiusAll(8)) {
 *         Text("Save",
 *              .textColor = Color(255, 255, 255, 255),
 *              .fontSize  = 16);
 *     }
 *
 * CC_Clicked / CC_Hovered can be queried before or after the Button
 * block in the same frame. The first frame a button becomes visible it
 * will not register a hover until the next CC_Begin tick, because
 * Clay's pointer check runs against the previous frame's layout.
 *
 * The id passed to Button / CC_Hovered / CC_Clicked is hashed at
 * runtime via CC_SID. For indexed ids (e.g. list rows), use Row/Box +
 * CC_PointerOver(CLAY_IDI("row", i)) directly.
 *
 * In headless mode (CCOMPOSE_NO_BACKEND) there is no input source, so
 * CC__MousePressedThisFrame() returns false and CC_Clicked() is
 * always false. CC_Hovered() still works against whatever pointer
 * state you push via Clay_SetPointerState(), which is useful in tests.
 */

/* Internal — returns true on the frame the primary mouse button
 * transitioned from up to down. Backed by IsMouseButtonPressed() with
 * the raylib backend, always false in headless mode. Exposed only so
 * the CC_Clicked macro can reference it; prefer CC_Clicked in user
 * code. */
bool CC__MousePressedThisFrame(void);

/* True while the cursor is over the element with the given id.
 * Thin wrapper around CC_PointerOver(CC_SID(CC__Str(id))). */
#define CC_Hovered(id) CC_PointerOver(CC_SID(CC__Str(id)))

/* True on the single frame the user clicks (left mouse press) while
 * hovering the element with the given id. Short-circuits on the mouse
 * check so the Clay id hash is only computed on press frames. */
#define CC_Clicked(id) (CC__MousePressedThisFrame() && CC_Hovered(id))

/* Button — scoped block for clickable elements. Identical expansion to
 * Row: CLAY_LEFT_TO_RIGHT children, every CC_ElementDeclaration
 * field available, no hidden behavior. Give it a non-empty string
 * literal id so CC_Hovered / CC_Clicked have something to target;
 * with id == "" it degrades to a plain LTR element. */
#define Button(id_literal, ...)                                                \
  Element(CLAY_LEFT_TO_RIGHT, id_literal, __VA_ARGS__)

/* =========================================================================
 * Scroll — input glue for .clip viewports
 * =========================================================================
 *
 * Clay's .clip turns an element into a scroll viewport with a
 * .childOffset Vector2. ccompose's job here is to drive that offset
 * from raylib's mouse wheel and (optionally) click-and-drag input,
 * with auto-clamping against content bounds.
 *
 * Usage:
 *
 *     static CC_Scroll s_list;
 *
 *     // before the layout block for this frame:
 *     CC_ScrollUpdate(&s_list, "MyScroll",
 *                     .horizontal = true, .drag = true, .wheel = true);
 *
 *     // in the layout:
 *     Row("MyScroll",
 *         .clip = { .horizontal = true, .childOffset = s_list.offset },
 *         ...) {
 *         // children must have intrinsic widths (Fixed / Fit), not Grow,
 *         // otherwise they compress to the viewport and nothing scrolls.
 *     }
 *
 * State lives in the caller-owned CC_Scroll. Offset is clamped against
 * the element's content/viewport size reported by the *previous*
 * frame's layout (Clay_GetScrollContainerData) — set .noClamp to opt
 * out, e.g. when content size is unstable or you want overscroll.
 *
 * Headless mode (CCOMPOSE_NO_BACKEND): CC_ScrollUpdate is a no-op.
 */

typedef struct {
  CC_Vector2 offset; /* feed into .clip.childOffset */
  bool dragging;     /* tracks an active press-drag (internal) */
} CC_Scroll;

typedef struct {
  bool horizontal;  /* enable horizontal axis */
  bool vertical;    /* enable vertical axis */
  bool drag;        /* enable click-and-drag scrolling */
  bool wheel;       /* enable mouse-wheel scrolling */
  float wheelSpeed; /* pixels per wheel unit (default 40) */
  bool noClamp;     /* skip auto-clamp against content bounds */
} CC_ScrollConfig;

void CC__ScrollUpdate(CC_Scroll *state, const char *id, CC_ScrollConfig cfg);

#define CC_ScrollUpdate(state_ptr, id_literal, ...)                            \
  CC__ScrollUpdate((state_ptr), (id_literal), (CC_ScrollConfig){__VA_ARGS__})

#ifdef __cplusplus
}
#endif

#endif /* CCOMPOSE_H */
