/* ccompose minimal demo.
 *
 * Build via CMake (from the ccompose project root):
 *     cmake -S . -B build
 *     cmake --build build
 *     ./build/examples/ccompose_demo
 *
 * Close the window or press ESC to quit.
 */

#include "../include/ccompose.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

typedef struct {
  float t;
  int dot_count;
} CanvasState;

static const CC_Color COLOR_BG = {18, 18, 20, 255};
static const CC_Color COLOR_SURFACE = {28, 28, 34, 255};
static const CC_Color COLOR_HOVER = {48, 48, 58, 255};
static const CC_Color COLOR_ACCENT = {111, 173, 162, 255};
static const CC_Color COLOR_BORDER = {60, 60, 70, 255};
static const CC_Color COLOR_TEXT = {236, 236, 236, 255};
static const CC_Color COLOR_MUTED = {160, 160, 170, 255};
static const CC_Color COLOR_TRANSPARENT = {0, 0, 0, 0};

static void paint_canvas(CC_BoundingBox bb, void *user) {
  CanvasState *s = (CanvasState *)user;

  /* Gradient background, bb-relative */
  DrawRectangleGradientV((int)bb.x, (int)bb.y, (int)bb.width, (int)bb.height,
                         (Color){30, 40, 70, 255}, (Color){70, 30, 90, 255});

  /* Bouncing dots using sin/cos, relative to bb*/
  for (int i = 0; i < s->dot_count; i++) {
    float phase = s->t * 1.2f + (float)i * 0.9f;
    float nx = 0.5f + 0.4f * sinf(phase);
    float ny = 0.5f + 0.35 * cosf(phase * 1.3f);
    float cx = bb.x + nx * bb.width;
    float cy = bb.y + ny * bb.height;
    float r = 6.0f + 3.0f * sinf(phase * 2.0f);
    unsigned char alpha = (unsigned char)(160 + 60 * sinf(phase));
    DrawCircleV((Vector2){cx, cy}, r, (Color){180, 220, 255, alpha});
  }

  DrawRectangleLinesEx(
      (Rectangle){bb.x + 1, bb.y + 1, bb.width - 2, bb.height - 2}, 1.0f,
      (Color){255, 255, 255, 255});
}

/* Shared state for the frame callback. Lifted to file scope so the
 * same function pointer works on native (while-loop) and web
 * (emscripten_set_main_loop) — see CC_RUN_LOOP in ccompose.h. */
static Texture2D *avatar_tex = NULL;
static const char *current_page = "Home";
static CanvasState canvas_state = {.t = 0.0f, .dot_count = 12};

static void frame(void) {
  CC_Begin();
  canvas_state.t = (float)GetTime();
  Column("Root",
           .layout = {.sizing = {Grow(), Grow()},
                      .padding = PadAll(24),
                      .childGap = 16},
           .backgroundColor = COLOR_BG) {

      DrawRow("Canvas", paint_canvas, &canvas_state,
              .layout = {.sizing = {Grow(), Fixed(200)},
                         .padding = Pad(20, 20, 16, 16),
                         .childGap = 12,
                         .childAlignment = {.y = CC_ALIGN_Y_CENTER}},
              .cornerRadius = RadiusAll(8)) {

        /* Box used as a single-child decoration: a fixed-size circular
         * badge holding a single character of text. */
        Image("Avatar", ImgCrop(avatar_tex),
              .layout = {.sizing = {Fixed(50), Fixed(50)}},
              .cornerRadius = RadiusAll(999));

        Column("HeaderText",
               .layout = {.sizing = {Fit(), Fit()}, .childGap = 4}) {
          Text("ccompose", .textColor = COLOR_ACCENT, .fontSize = 28);
          Text("A Jetpack-Compose-shaped wrapper around Clay",
               .textColor = COLOR_MUTED, .fontSize = 14,
               .wrapMode = CC_TEXT_WRAP_WORDS);
        }
      }
      /* Header card */
      /* Two-column body */
      Row("Body", .layout = {.sizing = {Grow(), Grow()}, .childGap = 16}) {

        /* Sidebar */
        Column("Sidebar",
               .layout = {.sizing = {Fixed(220), Grow()},
                          .padding = PadAll(16),
                          .childGap = 8},
               .backgroundColor = COLOR_SURFACE, .cornerRadius = RadiusAll(8),
               .border = {.color = COLOR_BORDER, .width = {1, 1, 1, 1, 0}}) {
          Text("Navigation", .textColor = COLOR_MUTED, .fontSize = 12);

          /* Each sidebar entry is a Button. The idle background is
           * transparent so the sidebar's own surface shows through;
           * hovering fades in COLOR_HOVER; clicking rebinds
           * current_page. Queries sit right next to the Button so the
           * pairing is obvious. */
          if (CC_Clicked("NavHome"))
            current_page = "Home";
          Button("NavHome",
                 .layout = {.sizing = {Grow(), Fit()},
                            .padding = Pad(12, 12, 8, 8),
                            .childAlignment = {.y = CC_ALIGN_Y_CENTER}},
                 .backgroundColor =
                     CC_Hovered("NavHome") ? COLOR_HOVER : COLOR_TRANSPARENT,
                 .cornerRadius = RadiusAll(6)) {
            Text("Home", .textColor = COLOR_TEXT, .fontSize = 16);
          }

          if (CC_Clicked("NavColumn"))
            current_page = "Column";
          Button("NavColumn",
                 .layout = {.sizing = {Grow(), Fit()},
                            .padding = Pad(12, 12, 8, 8),
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                 .backgroundColor =
                     CC_Hovered("NavColumn") ? COLOR_HOVER : COLOR_TRANSPARENT,
                 .cornerRadius = RadiusAll(6)) {
            Text("Column", .textColor = COLOR_TEXT, .fontSize = 16);
          }

          if (CC_Clicked("NavRow"))
            current_page = "Row";
          Button("NavRow",
                 .layout = {.sizing = {Grow(), Fit()},
                            .padding = Pad(12, 12, 8, 8),
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                 .backgroundColor =
                     CC_Hovered("NavRow") ? COLOR_HOVER : COLOR_TRANSPARENT,
                 .cornerRadius = RadiusAll(6)) {
            Text("Row", .textColor = COLOR_TEXT, .fontSize = 16);
          }

          if (CC_Clicked("NavText"))
            current_page = "Text";
          Button("NavText",
                 .layout = {.sizing = {Grow(), Fit()},
                            .padding = Pad(12, 12, 8, 8),
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                 .backgroundColor =
                     CC_Hovered("NavText") ? COLOR_HOVER : COLOR_TRANSPARENT,
                 .cornerRadius = RadiusAll(6)) {
            Text("Text", .textColor = COLOR_TEXT, .fontSize = 16);
          }

          if (CC_Clicked("NavLayout"))
            current_page = "Layout";
          Button("NavLayout",
                 .layout = {.sizing = {Grow(), Fit()},
                            .padding = Pad(12, 12, 8, 8),
                            .childAlignment = {.y = CC_ALIGN_Y_CENTER}},
                 .backgroundColor =
                     CC_Hovered("NavLayout") ? COLOR_HOVER : COLOR_TRANSPARENT,
                 .cornerRadius = RadiusAll(6)) {
            Text("Layout", .textColor = COLOR_TEXT, .fontSize = 16);
          }
        }

        /* Main content */
        Column("Content",
               .layout = {.sizing = {Grow(), Grow()},
                          .padding = PadAll(24),
                          .childGap = 12},
               .backgroundColor = COLOR_SURFACE, .cornerRadius = RadiusAll(8),
               .border = {.color = COLOR_BORDER, .width = {1, 1, 1, 1, 0}}) {
          /* current_page is a runtime pointer (rebound by the sidebar
           * Buttons), so we use TextN() which takes (chars, length) —
           * Text() is string-literal-only by design. */
          Text(current_page, .textColor = COLOR_TEXT, .fontSize = 32);
          Text("This window is laid out by Clay and driven by "
               "ccompose's Column / Row / Text macros.",
               .textColor = COLOR_MUTED, .fontSize = 16,
               .wrapMode = CC_TEXT_WRAP_WORDS);

          /* Anonymous accent pill — empty ID = no stable name. */
          Row("", .layout = {.padding = Pad(14, 14, 8, 8), .childGap = 8},
              .backgroundColor = COLOR_ACCENT, .cornerRadius = RadiusAll(999)) {
            Text("Press ESC to exit", .textColor = (CC_Color){10, 10, 10, 255},
                 .fontSize = 14);
          }
        }
      }
    }
  CC_End();
}

static bool quit_on_q(void *user) {
  (void)user;
  if (CC_KeyPressed(CC_KEY_Q))
    CC_RequestQuit();
  return CC_KeyPressed(CC_KEY_ESCAPE);
}

int main(void) {
  CC_SetWindow(960, 640, "ccompose + Clay + raylib");
  CC_SetBackground((CC_Color){0, 0, 0, 255});
  CC_Init();
  CC_SetQuitHandler(quit_on_q, NULL);

  int global_font_id =
      CC_LoadGlobalFont("examples/resources/Roboto-Regular.ttf", 24);
  if (global_font_id >= 0) {
    fprintf(stderr, "ccompose demo: global font loaded: %s (id=%d)\n",
            "examples/resources/Roboto-Regular.ttf", global_font_id);
  }
  if (global_font_id < 0) {
    fprintf(
        stderr,
        "ccompose demo: global font load failed, using default font (id=0)\n");
  }

  Texture2D avatar_texture = CC_LoadImage("examples/resources/profile.jpg");
  avatar_tex = &avatar_texture;

  CC_UnloadImage(avatar_texture);
  CC_Shutdown();
  return 0;
}
