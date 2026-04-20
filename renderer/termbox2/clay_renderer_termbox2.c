/* clay_renderer_termbox2.c — termbox2 backend for ccompose.
 *
 * Included once from src/ccompose.c when CCOMPOSE_BACKEND_TERMBOX2 is
 * defined. Mirrors the surface area of clay_renderer_raylib.c plus the
 * raylib backend block in src/ccompose.c — i.e. defines the cc__backend_*
 * hooks AND the public CC_Set..., CC_Load..., CC_Running, CC_KeyPressed
 * symbols. Termbox2 is a TUI library, so:
 *   - Window dims come from the terminal, not CC_SetWindow (which is a no-op).
 *   - Fonts/images aren't supported (stubs return safe defaults).
 *   - Pixel coords from Clay are quantized to a fixed cell grid
 *     (CC_TB_CELL_W x CC_TB_CELL_H "logical pixels" per character cell).
 *   - Color is rendered in TB_OUTPUT_TRUECOLOR mode (24-bit RGB packed).
 */

/* termbox2 uses errno, fileno, ioctl, … but does not include the system
 * headers that declare them. Pull them in explicitly before TB_IMPL. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define TB_IMPL
#include "termbox2.h"

/* Logical-pixel size of one character cell. Clay still thinks in pixels;
 * we divide by these to land on a cell. ~2:1 height:width matches most
 * terminal cells, so layouts feel proportional. */
#define CC_TB_CELL_W 8
#define CC_TB_CELL_H 16

#define CC_TB_KEY_QUEUE_MAX 32

static CC_Color cc__tb_bg = {0, 0, 0, 255};
static double cc__tb_last_t = 0.0;
static float cc__tb_dt = 0.0f;

/* Edge-triggered key buffer drained each frame. Size is small because
 * Clay only needs "was this key pressed this frame?" — we don't try to
 * track full key state. */
static int cc__tb_keys_pressed[CC_TB_KEY_QUEUE_MAX];
static int cc__tb_key_count = 0;

static int cc__tb_mouse_x = 0;
static int cc__tb_mouse_y = 0;
static bool cc__tb_mouse_down = false;
static bool cc__tb_mouse_pressed_this_frame = false;
static bool cc__tb_should_close = false;

static double cc__tb_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int cc__tb_to_cc_key(uint16_t k, uint32_t ch) {
  switch (k) {
  case TB_KEY_ESC:         return CC_KEY_ESCAPE;
  case TB_KEY_ENTER:       return CC_KEY_ENTER;
  case TB_KEY_SPACE:       return CC_KEY_SPACE;
  case TB_KEY_TAB:         return CC_KEY_TAB;
  case TB_KEY_ARROW_LEFT:  return CC_KEY_LEFT;
  case TB_KEY_ARROW_RIGHT: return CC_KEY_RIGHT;
  case TB_KEY_ARROW_UP:    return CC_KEY_UP;
  case TB_KEY_ARROW_DOWN:  return CC_KEY_DOWN;
  default: break;
  }
  switch (ch) {
  case 'q': case 'Q': return CC_KEY_Q;
  case 'w': case 'W': return CC_KEY_W;
  case 'e': case 'E': return CC_KEY_E;
  case 'r': case 'R': return CC_KEY_R;
  default: return CC_KEY_NONE;
  }
}

static uintattr_t cc__tb_color(Clay_Color c) {
  return ((uintattr_t)(uint8_t)c.r << 16) |
         ((uintattr_t)(uint8_t)c.g << 8) |
         (uintattr_t)(uint8_t)c.b;
}

static Clay_Dimensions cc__tb_measure(Clay_StringSlice text,
                                      Clay_TextElementConfig *cfg, void *ud) {
  (void)cfg;
  (void)ud;
  return (Clay_Dimensions){(float)text.length * (float)CC_TB_CELL_W,
                           (float)CC_TB_CELL_H};
}

static void cc__tb_drain_events(void) {
  cc__tb_key_count = 0;
  cc__tb_mouse_pressed_this_frame = false;

  struct tb_event ev;
  while (tb_peek_event(&ev, 0) == TB_OK) {
    if (ev.type == TB_EVENT_RESIZE) {
      Clay_SetLayoutDimensions((Clay_Dimensions){
          (float)ev.w * (float)CC_TB_CELL_W,
          (float)ev.h * (float)CC_TB_CELL_H,
      });
    } else if (ev.type == TB_EVENT_KEY) {
      if (ev.key == TB_KEY_CTRL_C)
        cc__tb_should_close = true;
      int k = cc__tb_to_cc_key(ev.key, ev.ch);
      if (k != CC_KEY_NONE && cc__tb_key_count < CC_TB_KEY_QUEUE_MAX)
        cc__tb_keys_pressed[cc__tb_key_count++] = k;
    } else if (ev.type == TB_EVENT_MOUSE) {
      cc__tb_mouse_x = ev.x;
      cc__tb_mouse_y = ev.y;
      bool was = cc__tb_mouse_down;
      cc__tb_mouse_down = (ev.key == TB_KEY_MOUSE_LEFT);
      if (cc__tb_mouse_down && !was)
        cc__tb_mouse_pressed_this_frame = true;
    }
  }
}

static void cc__tb_fill(int x, int y, int w, int h, uintattr_t bg) {
  int W = tb_width(), H = tb_height();
  for (int row = y; row < y + h && row < H; ++row) {
    if (row < 0)
      continue;
    for (int col = x; col < x + w && col < W; ++col) {
      if (col < 0)
        continue;
      tb_set_cell(col, row, ' ', 0, bg);
    }
  }
}

static void cc__tb_box(int x, int y, int w, int h, uintattr_t fg) {
  if (w < 2 || h < 2)
    return;
  int W = tb_width(), H = tb_height();
#define CC_TB_PUT(cx, cy, cch)                                                 \
  do {                                                                         \
    if ((cx) >= 0 && (cx) < W && (cy) >= 0 && (cy) < H)                        \
      tb_set_cell((cx), (cy), (cch), fg, 0);                                   \
  } while (0)
  CC_TB_PUT(x, y, 0x250C);
  CC_TB_PUT(x + w - 1, y, 0x2510);
  CC_TB_PUT(x, y + h - 1, 0x2514);
  CC_TB_PUT(x + w - 1, y + h - 1, 0x2518);
  for (int i = 1; i < w - 1; ++i) {
    CC_TB_PUT(x + i, y, 0x2500);
    CC_TB_PUT(x + i, y + h - 1, 0x2500);
  }
  for (int i = 1; i < h - 1; ++i) {
    CC_TB_PUT(x, y + i, 0x2502);
    CC_TB_PUT(x + w - 1, y + i, 0x2502);
  }
#undef CC_TB_PUT
}

/* ---------- Backend hooks ---------- */

static void cc__backend_init(void) {
  if (tb_init() != 0) {
    fprintf(stderr, "ccompose/termbox2: tb_init failed\n");
    exit(EXIT_FAILURE);
  }
  tb_set_output_mode(TB_OUTPUT_TRUECOLOR);
  tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
  cc__viewport = (Clay_Dimensions){
      (float)tb_width() * (float)CC_TB_CELL_W,
      (float)tb_height() * (float)CC_TB_CELL_H,
  };
  cc__tb_last_t = cc__tb_now();
}

static void cc__backend_after_clay_init(void) {
  Clay_SetMeasureTextFunction(cc__tb_measure, NULL);
}

static void cc__backend_begin_frame(void) {
  double now = cc__tb_now();
  cc__tb_dt = (float)(now - cc__tb_last_t);
  cc__tb_last_t = now;

  cc__tb_drain_events();

  Clay_SetLayoutDimensions((Clay_Dimensions){
      (float)tb_width() * (float)CC_TB_CELL_W,
      (float)tb_height() * (float)CC_TB_CELL_H,
  });
  Clay_SetPointerState(
      (Clay_Vector2){
          (float)cc__tb_mouse_x * (float)CC_TB_CELL_W,
          (float)cc__tb_mouse_y * (float)CC_TB_CELL_H,
      },
      cc__tb_mouse_down);
  Clay_UpdateScrollContainers(true, (Clay_Vector2){0, 0}, cc__tb_dt);

  uintattr_t bg = cc__tb_color(cc__tb_bg);
  tb_set_clear_attrs(0, bg);
  tb_clear();
}

static void cc__backend_end_frame(Clay_RenderCommandArray cmds) {
  for (int32_t i = 0; i < cmds.length; ++i) {
    Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
    Clay_BoundingBox b = cmd->boundingBox;
    int x = (int)(b.x / (float)CC_TB_CELL_W);
    int y = (int)(b.y / (float)CC_TB_CELL_H);
    int w = (int)((b.width + (float)CC_TB_CELL_W - 1) / (float)CC_TB_CELL_W);
    int h = (int)((b.height + (float)CC_TB_CELL_H - 1) / (float)CC_TB_CELL_H);

    switch (cmd->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
      cc__tb_fill(x, y, w, h,
                  cc__tb_color(cmd->renderData.rectangle.backgroundColor));
      break;
    case CLAY_RENDER_COMMAND_TYPE_BORDER:
      cc__tb_box(x, y, w, h, cc__tb_color(cmd->renderData.border.color));
      break;
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_StringSlice s = cmd->renderData.text.stringContents;
      uintattr_t fg = cc__tb_color(cmd->renderData.text.textColor);
      int max = w < (int)s.length ? w : (int)s.length;
      char buf[1024];
      int n = max < (int)sizeof(buf) - 1 ? max : (int)sizeof(buf) - 1;
      if (n < 0)
        n = 0;
      memcpy(buf, s.chars, (size_t)n);
      buf[n] = 0;
      if (y >= 0 && y < tb_height() && x < tb_width()) {
        int px = x < 0 ? 0 : x;
        tb_print(px, y, fg, 0, buf);
      }
      break;
    }
    /* termbox2 backend ignores image / scissor / overlay / custom — no
     * meaningful TUI rendering for them. */
    default:
      break;
    }
  }
  tb_present();
}

static void cc__backend_shutdown(void) { tb_shutdown(); }

static float cc__backend_frame_time(void) { return cc__tb_dt; }

/* ---------- Public CC_* surface (TUI variants) ---------- */

void CC_SetWindow(int width, int height, const char *title) {
  (void)width;
  (void)height;
  (void)title; /* terminal-driven; title would need OSC escape */
}

void CC_SetWindowFlags(unsigned int flags) { (void)flags; }

void CC_SetBackground(CC_Color color) { cc__tb_bg = color; }

int CC_LoadFont(const char *path, int base_size) {
  (void)path;
  (void)base_size;
  return 0; /* TUI has one font: the terminal's. */
}

int CC_LoadGlobalFont(const char *path, int base_size) {
  (void)path;
  (void)base_size;
  return 0;
}

int CC_GetGlobalFontId(void) { return 0; }

CC_DrawSlot *CC_AcquireDrawSlot(CC_DrawFn fn, void *user) {
  (void)fn;
  (void)user;
  return NULL;
}

bool CC_Running(void) {
  if (cc__quit_requested)
    return false;
  if (cc__tb_should_close)
    return false;
  if (cc__quit_fn) {
    if (cc__quit_fn(cc__quit_user))
      return false;
  } else {
    for (int i = 0; i < cc__tb_key_count; ++i)
      if (cc__tb_keys_pressed[i] == CC_KEY_ESCAPE)
        return false;
  }
  return true;
}

bool CC__MousePressedThisFrame(void) {
  return cc__tb_mouse_pressed_this_frame;
}

bool CC_KeyPressed(CC_Key key) {
  for (int i = 0; i < cc__tb_key_count; ++i)
    if (cc__tb_keys_pressed[i] == (int)key)
      return true;
  return false;
}
