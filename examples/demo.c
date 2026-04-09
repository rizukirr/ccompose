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
#include <stdio.h>
#include <string.h>

static const CC_Color COLOR_BG = {18, 18, 20, 255};
static const CC_Color COLOR_SURFACE = {28, 28, 34, 255};
static const CC_Color COLOR_HOVER = {48, 48, 58, 255};
static const CC_Color COLOR_ACCENT = {111, 173, 162, 255};
static const CC_Color COLOR_BORDER = {60, 60, 70, 255};
static const CC_Color COLOR_TEXT = {236, 236, 236, 255};
static const CC_Color COLOR_MUTED = {160, 160, 170, 255};
static const CC_Color COLOR_TRANSPARENT = {0, 0, 0, 0};

static void BuildUI(void) {
  static const char *current_page = "Home";

  while (CC_Running()) {
    CC_Begin();
    Column("Root",
           .layout = {.sizing = {Grow(), Grow()},
                      .padding = PadAll(24),
                      .childGap = 16},
           .backgroundColor = COLOR_BG) {

      /* Header card */
      Row("Header",
          .layout = {.sizing = {Grow(), Fixed(64)},
                     .padding = Pad(20, 20, 16, 16),
                     .childGap = 12,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = COLOR_SURFACE, .cornerRadius = RadiusAll(8),
          .border = {.color = COLOR_BORDER, .width = {1, 1, 1, 1, 0}}) {

        /* Box used as a single-child decoration: a fixed-size circular
         * badge holding a single character of text. */
        Box("HeaderBadge",
            .layout = {.sizing = {Fixed(36), Fixed(36)},
                       .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                          .y = CLAY_ALIGN_Y_CENTER}},
            .backgroundColor = COLOR_ACCENT, .cornerRadius = RadiusAll(999)) {
          Text("c", .textColor = (CC_Color){10, 10, 10, 255}, .fontSize = 22);
        }

        Column("HeaderText",
               .layout = {.sizing = {Grow(), Fit()}, .childGap = 4}) {
          Text("ccompose", .textColor = COLOR_ACCENT, .fontSize = 28);
          Text("A Jetpack-Compose-shaped wrapper around Clay",
               .textColor = COLOR_MUTED, .fontSize = 14);
        }
      }

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
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
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
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
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
          TextN(current_page, (int)strlen(current_page),
                .textColor = COLOR_TEXT, .fontSize = 32);
          Text("This window is laid out by Clay and driven by "
               "ccompose's Column / Row / Text macros.",
               .textColor = COLOR_MUTED, .fontSize = 16,
               .wrapMode = CLAY_TEXT_WRAP_WORDS);

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
}

int main(void) {
  CC_SetWindow(960, 640, "ccompose + Clay + raylib");
  CC_SetBackground((CC_Color){0, 0, 0, 255});
  CC_Init();

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

  BuildUI();

  CC_Shutdown();
  return 0;
}
