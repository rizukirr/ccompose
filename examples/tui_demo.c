/* tui_demo.c — termbox2-backend example for ccompose.
 *
 * Mirrors the structure of landing.c: a sidebar + main pane with a
 * counter that reacts to arrow keys and space. Proves that the same
 * Column/Row/Box/Text DSL renders both to a raylib window and to a
 * terminal without any DSL changes.
 *
 * Controls:
 *   ← / → : decrement / increment counter
 *   space : reset
 *   q / esc: quit
 */

#include "ccompose.h"

#include <stdio.h>

static int counter = 0;

#define COLOR_BG      Color(18, 18, 22, 255)
#define COLOR_PANEL   Color(28, 28, 34, 255)
#define COLOR_BORDER  Color(80, 80, 96, 255)
#define COLOR_TEXT    Color(220, 220, 230, 255)
#define COLOR_MUTED   Color(140, 140, 160, 255)
#define COLOR_ACCENT  Color(120, 200, 140, 255)

static void frame(void) {
  if (CC_KeyPressed(CC_KEY_LEFT))
    counter--;
  if (CC_KeyPressed(CC_KEY_RIGHT))
    counter++;
  if (CC_KeyPressed(CC_KEY_SPACE))
    counter = 0;

  char counter_str[32];
  snprintf(counter_str, sizeof(counter_str), "%d", counter);

  CC_Begin();

  Row("Root",
      .layout = {.sizing = {Grow(), Grow()}, .padding = PadAll(16), .childGap = 16},
      .backgroundColor = COLOR_BG) {

    /* Sidebar */
    Column("Sidebar",
           .layout = {.sizing = {Fixed(240), Grow()},
                      .padding = PadAll(16),
                      .childGap = 8},
           .backgroundColor = COLOR_PANEL,
           .border = {.color = COLOR_BORDER,
                      .width = {.left = 1, .right = 1, .top = 1, .bottom = 1}}) {
      Text("ccompose", .textColor = COLOR_ACCENT, .fontSize = 16);
      Text("TUI backend", .textColor = COLOR_MUTED, .fontSize = 16);
      VSpacer();
      Text("-> right arrow", .textColor = COLOR_TEXT, .fontSize = 16);
      Text("<- left arrow",  .textColor = COLOR_TEXT, .fontSize = 16);
      Text("space = reset",  .textColor = COLOR_TEXT, .fontSize = 16);
      Text("q/esc = quit",   .textColor = COLOR_MUTED, .fontSize = 16);
    }

    /* Main pane */
    Column("Main",
           .layout = {.sizing = {Grow(), Grow()},
                      .padding = PadAll(24),
                      .childGap = 16,
                      .childAlignment = {.x = AlignCenter(), .y = AlignMiddle()}},
           .backgroundColor = COLOR_PANEL,
           .border = {.color = COLOR_BORDER,
                      .width = {.left = 1, .right = 1, .top = 1, .bottom = 1}}) {
      Text("Counter",   .textColor = COLOR_MUTED,  .fontSize = 16);
      Text(counter_str, .textColor = COLOR_ACCENT, .fontSize = 16);
    }
  }

  CC_End();
}

int main(void) {
  CC_SetBackground(COLOR_BG);
  CC_Init();

  CC_RUN_LOOP(frame);

  CC_Shutdown();
  return 0;
}
