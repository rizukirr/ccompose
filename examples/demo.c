/* ccompose showcase demo.
 *
 * A small multi-page tour of what the library can do:
 *   - Home          declarative layout, Text hierarchy, custom drawing
 *   - Layout        Fit / Grow / Fixed / Percent sizing with labels
 *   - Interaction   Button hover/click states, a toggle that holds state
 *   - Scroll        Horizontal carousel driven by CC_Scroll (wheel + drag)
 *   - Transitions   DefineTransition slide+fade on a toggleable card
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

/* ------- palette --------------------------------------------------------- */

static const CC_Color COLOR_BG = {14, 16, 20, 255};
static const CC_Color COLOR_SURFACE = {26, 28, 34, 255};
static const CC_Color COLOR_SURFACE_2 = {36, 38, 46, 255};
static const CC_Color COLOR_HOVER = {52, 54, 64, 255};
static const CC_Color COLOR_PRESSED = {70, 130, 122, 255};
static const CC_Color COLOR_ACCENT = {111, 173, 162, 255};
static const CC_Color COLOR_ACCENT_DIM = {70, 110, 104, 255};
static const CC_Color COLOR_BORDER = {56, 58, 68, 255};
static const CC_Color COLOR_TEXT = {236, 236, 236, 255};
static const CC_Color COLOR_MUTED = {150, 154, 168, 255};
static const CC_Color COLOR_TRANSPARENT = {0, 0, 0, 0};

/* ------- animated canvas ------------------------------------------------- */

typedef struct {
  float t;
  int dot_count;
} CanvasState;

static void paint_canvas(CC_BoundingBox bb, void *user) {
  CanvasState *s = (CanvasState *)user;

  DrawRectangleGradientEx((Rectangle){bb.x, bb.y, bb.width, bb.height},
                          (Color){38, 60, 74, 255}, (Color){28, 42, 64, 255},
                          (Color){48, 60, 90, 255}, (Color){26, 44, 58, 255});

  for (int i = 0; i < s->dot_count; i++) {
    float phase = s->t * 0.9f + (float)i * 0.65f;
    float nx = 0.5f + 0.42f * sinf(phase);
    float ny = 0.5f + 0.36f * cosf(phase * 1.27f);
    float cx = bb.x + nx * bb.width;
    float cy = bb.y + ny * bb.height;
    float r = 5.0f + 3.0f * sinf(phase * 1.7f);
    unsigned char alpha = (unsigned char)(120 + 90 * sinf(phase * 0.8f));
    DrawCircleV((Vector2){cx, cy}, r, (Color){170, 215, 220, alpha});
  }
}

/* ------- shared frame state --------------------------------------------- */

typedef enum {
  PAGE_HOME,
  PAGE_LAYOUT,
  PAGE_INTERACT,
  PAGE_SCROLL,
  PAGE_TRANS
} Page;

static Texture2D *avatar_tex = NULL;
static Page current_page = PAGE_HOME;
static CanvasState canvas_state = {.t = 0.0f, .dot_count = 14};
static CC_Scroll carousel_scroll;
static bool toggle_notifications = false;
static bool toggle_compact_mode = false;

/* ------- transitions ----------------------------------------------------- */

/* Slide-in-from-top + fade for the notifications card on the Transitions
 * page. The transition state lives in Clay; we only toggle visibility via
 * the conditional render. */
DefineTransition(notif_anim, 0.32f, .enter = {.slideY = -24, .fade = true},
                 .exit = {.slideY = -24, .fade = true});

/* ------- helpers --------------------------------------------------------- */

static const char *page_title(Page p) {
  switch (p) {
  case PAGE_HOME:
    return "Home";
  case PAGE_LAYOUT:
    return "Layout";
  case PAGE_INTERACT:
    return "Interaction";
  case PAGE_SCROLL:
    return "Scroll";
  case PAGE_TRANS:
    return "Transitions";
  }
  return "?";
}

static const char *page_blurb(Page p) {
  switch (p) {
  case PAGE_HOME:
    return "ccompose wraps Clay's immediate-mode layout engine in a "
           "Jetpack-Compose-shaped DSL. Every container is a scoped block; "
           "every field is a designated initializer on Clay's own struct.";
  case PAGE_LAYOUT:
    return "Four sizing modes cover most layouts: Fit shrink-wraps "
           "content, Grow fills remaining space (shared with sibling "
           "Grows), Fixed pins a pixel size, Percent takes a fraction of "
           "the parent.";
  case PAGE_INTERACT:
    return "Hover and click queries follow the immediate-mode idiom. "
           "Declare the Button, then ask if it was hovered or clicked "
           "this frame. No callbacks, no event threading.";
  case PAGE_SCROLL:
    return "CC_Scroll drives .clip.childOffset from raylib's wheel and "
           "click-drag input, with auto-clamping against measured content "
           "bounds. Try dragging or scrolling the strip below.";
  case PAGE_TRANS:
    return "DefineTransition declares a reusable preset at file scope. "
           "Enter and exit effects animate independently; built-in "
           "easings cover the common curves. Toggle the card below.";
  }
  return "";
}

/* Sidebar nav entry — a Button + a click query, side by side with the
 * idle/hover/active visuals. Centralizing it keeps the sidebar block
 * compact. */
static void NavEntry(const char *id, Page p) {
  bool active = (current_page == p);
  if (CC_Clicked(id))
    current_page = p;
  CC_Color bg = active           ? COLOR_ACCENT_DIM
                : CC_Hovered(id) ? COLOR_HOVER
                                 : COLOR_TRANSPARENT;
  Button(id,
         .layout = {.sizing = {Grow(), Fit()},
                    .padding = PadSymmetric(14, 10),
                    .childAlignment = ChildAlign(.y = AlignYCenter())},
         .backgroundColor = bg, .cornerRadius = RadiusAll(8)) {
    Text(page_title(p), .textColor = active ? COLOR_TEXT : COLOR_MUTED,
         .fontSize = 15);
  }
}

/* A small card with title + body, used by the Home feature grid. */
static void FeatureCard(const char *id, const char *title, const char *body) {
  Column(id,
         .layout = {.sizing = {Grow(), Fit()},
                    .padding = PadAll(18),
                    .childGap = 8},
         .backgroundColor = COLOR_SURFACE_2, .cornerRadius = RadiusAll(10),
         .border = {.color = COLOR_BORDER, .width = BorderAll(1)}) {
    Text(title, .fontSize = 16, .textColor = COLOR_ACCENT);
    Text(body, .fontSize = 13, .textColor = COLOR_MUTED,
         .wrapMode = CC_TEXT_WRAP_WORDS, .lineHeight = 18);
  }
}

/* Labeled block used by the Layout page. Renders a colored rectangle with
 * the sizing description centered inside. */
static void SizedBlock(const char *id, const char *label, Clay_SizingAxis w) {
  Column(id,
         .layout = {.sizing = {w, Fixed(48)},
                    .childAlignment =
                        ChildAlign(.x = AlignXCenter(), .y = AlignYCenter())},
         .backgroundColor = COLOR_ACCENT_DIM, .cornerRadius = RadiusAll(6)) {
    Text(label, .fontSize = 13, .textColor = COLOR_TEXT);
  }
}

/* ------- per-page bodies ------------------------------------------------- */

static void render_home(void) {
  Row("HomeFeatureRow", .layout = {.sizing = {Grow(), Fit()}, .childGap = 12}) {
    FeatureCard("CardCompose", "Compose-shaped DSL",
                "Column(\"id\", .field = ...) { children } — scoped blocks "
                "over Clay's element open/close API.");
    FeatureCard("CardAlloc", "Zero per-frame alloc",
                "Clay owns the layout arena. ccompose adds no hidden tree "
                "and no malloc inside the frame loop.");
  }
  Spacer(.height = Fixed(12));
  Row("HomeFeatureRow2",
      .layout = {.sizing = {Grow(), Fit()}, .childGap = 12}) {
    FeatureCard("CardBackends", "raylib + headless",
                "Bundled raylib renderer by default; build with "
                "-DCCOMPOSE_BACKEND_RAYLIB=OFF for first-class headless "
                "tests.");
    FeatureCard("CardSurface", "Clay all the way down",
                "Every macro field is a designated initializer on Clay's "
                "own struct — no wrappers, no relearning.");
  }
}

static void render_layout(void) {
  Column("LayoutRows", .layout = {.sizing = {Grow(), Fit()}, .childGap = 10}) {
    Text("Three Grow() children share the row evenly:", .fontSize = 13,
         .textColor = COLOR_MUTED);
    Row("RowGrow", .layout = {.sizing = {Grow(), Fit()}, .childGap = 8}) {
      SizedBlock("g1", "Grow()", Grow());
      SizedBlock("g2", "Grow()", Grow());
      SizedBlock("g3", "Grow()", Grow());
    }

    Spacer(.height = Fixed(4));
    Text("Fixed pin + Grow filler:", .fontSize = 13, .textColor = COLOR_MUTED);
    Row("RowMix", .layout = {.sizing = {Grow(), Fit()}, .childGap = 8}) {
      SizedBlock("m1", "Fixed(120)", Fixed(120));
      SizedBlock("m2", "Grow()", Grow());
      SizedBlock("m3", "Fixed(80)", Fixed(80));
    }

    Spacer(.height = Fixed(4));
    Text("Percent splits the remaining width:", .fontSize = 13,
         .textColor = COLOR_MUTED);
    Row("RowPct", .layout = {.sizing = {Grow(), Fit()}, .childGap = 8}) {
      SizedBlock("p1", "Percent(0.3)", Percent(0.3f));
      SizedBlock("p2", "Percent(0.7)", Percent(0.7f));
    }
  }
}

static void render_interact(void) {
  Row("ButtonRow",
      .layout = {.sizing = {Grow(), Fit()},
                 .childGap = 12,
                 .childAlignment = ChildAlign(.y = AlignYCenter())}) {

    /* Primary — filled accent, darkens on press. */
    CC_Color primary_bg = CC_Clicked("BtnPrimary")   ? COLOR_PRESSED
                          : CC_Hovered("BtnPrimary") ? COLOR_ACCENT
                                                     : COLOR_ACCENT_DIM;
    Button("BtnPrimary",
           .layout = {.padding = PadSymmetric(18, 10),
                      .childAlignment =
                          ChildAlign(.x = AlignXCenter(), .y = AlignYCenter())},
           .backgroundColor = primary_bg, .cornerRadius = RadiusAll(8)) {
      Text("Primary", .fontSize = 14, .textColor = COLOR_TEXT);
    }

    /* Secondary — surface fill with a hover swap. */
    Button("BtnSecondary", .layout = {.padding = PadSymmetric(18, 10)},
           .backgroundColor =
               CC_Hovered("BtnSecondary") ? COLOR_HOVER : COLOR_SURFACE_2,
           .cornerRadius = RadiusAll(8),
           .border = {.color = COLOR_BORDER, .width = BorderAll(1)}) {
      Text("Secondary", .fontSize = 14, .textColor = COLOR_TEXT);
    }

    /* Ghost — transparent until hover. */
    Button("BtnGhost", .layout = {.padding = PadSymmetric(18, 10)},
           .backgroundColor =
               CC_Hovered("BtnGhost") ? COLOR_HOVER : COLOR_TRANSPARENT,
           .cornerRadius = RadiusAll(8)) {
      Text("Ghost", .fontSize = 14, .textColor = COLOR_MUTED);
    }

    HSpacer();

    /* Toggle — holds state across frames. */
    if (CC_Clicked("BtnToggle"))
      toggle_compact_mode = !toggle_compact_mode;
    /* No border with RadiusAll(999) — see VersionPill above. */
    Button("BtnToggle",
           .layout = {.padding = PadSymmetric(18, 10),
                      .childGap = 10,
                      .childAlignment = ChildAlign(.y = AlignYCenter())},
           .backgroundColor =
               toggle_compact_mode ? COLOR_ACCENT_DIM : COLOR_SURFACE_2,
           .cornerRadius = RadiusAll(999)) {
      Box("ToggleDot", .layout = {.sizing = {Fixed(10), Fixed(10)}},
          .backgroundColor = toggle_compact_mode ? COLOR_ACCENT : COLOR_MUTED,
          .cornerRadius = RadiusAll(999));
      Text(toggle_compact_mode ? "Compact mode: ON" : "Compact mode: OFF",
           .fontSize = 13, .textColor = COLOR_TEXT);
    }
  }

  Spacer(.height = Fixed(20));
  Text("CC_Hovered / CC_Clicked read the previous frame's layout, so the "
       "queries can sit before or after the Button block.",
       .fontSize = 13, .textColor = COLOR_MUTED,
       .wrapMode = CC_TEXT_WRAP_WORDS);
}

static void render_scroll(void) {
  CC_ScrollUpdate(&carousel_scroll, "Carousel", .horizontal = true,
                  .drag = true, .wheel = true);

  Text("Drag with the mouse or scroll the wheel — clamped against content "
       "bounds:",
       .fontSize = 13, .textColor = COLOR_MUTED,
       .wrapMode = CC_TEXT_WRAP_WORDS);
  Spacer(.height = Fixed(12));

  static const char *titles[] = {"Macros", "Layout",     "Text",
                                 "Images", "Buttons",    "Floating",
                                 "Scroll", "Transitions"};
  static const char *subtitles[] = {
      "Column / Row / Box", "Fit / Grow / Fixed",    "Wrap, align, fonts",
      "Fit / Fill / Crop",  "Hover & click queries", "Modals & tooltips",
      "CC_Scroll helper",   "Slide + fade presets"};
  int card_count = (int)(sizeof(titles) / sizeof(titles[0]));

  Row("Carousel", .clip = ClipX(carousel_scroll.offset),
      .layout = {.sizing = {Grow(), Fit()}, .childGap = 12}) {
    for (int i = 0; i < card_count; i++) {
      char id[24];
      snprintf(id, sizeof id, "ScrollCard-%d", i);
      Column(id,
             .layout = {.sizing = {Fixed(180), Fit()},
                        .padding = PadAll(16),
                        .childGap = 6},
             .backgroundColor = COLOR_SURFACE_2, .cornerRadius = RadiusAll(10),
             .border = {.color = COLOR_BORDER, .width = BorderAll(1)}) {
        Text(titles[i], .fontSize = 16, .textColor = COLOR_ACCENT);
        Text(subtitles[i], .fontSize = 12, .textColor = COLOR_MUTED,
             .wrapMode = CC_TEXT_WRAP_WORDS, .lineHeight = 17);
      }
    }
  }
}

static void render_trans(void) {
  if (CC_Clicked("BtnNotifToggle"))
    toggle_notifications = !toggle_notifications;

  Row("TransControls",
      .layout = {.sizing = {Grow(), Fit()},
                 .childGap = 12,
                 .childAlignment = ChildAlign(.y = AlignYCenter())}) {
    Button("BtnNotifToggle", .layout = {.padding = PadSymmetric(18, 10)},
           .backgroundColor = toggle_notifications           ? COLOR_PRESSED
                              : CC_Hovered("BtnNotifToggle") ? COLOR_ACCENT
                                                             : COLOR_ACCENT_DIM,
           .cornerRadius = RadiusAll(8)) {
      Text(toggle_notifications ? "Dismiss notification" : "Show notification",
           .fontSize = 14, .textColor = COLOR_TEXT);
    }
    HSpacer();
    Text(toggle_notifications ? "Card slides + fades out on dismiss"
                              : "Card slides + fades in on show",
         .fontSize = 13, .textColor = COLOR_MUTED);
  }

  Spacer(.height = Fixed(20));

  if (toggle_notifications) {
    Row("NotifCard", .transition = notif_anim(),
        .layout = {.sizing = {Grow(), Fit()},
                   .padding = PadAll(16),
                   .childGap = 12,
                   .childAlignment = ChildAlign(.y = AlignYCenter())},
        .backgroundColor = COLOR_ACCENT_DIM, .cornerRadius = RadiusAll(10),
        .border = {.color = COLOR_ACCENT, .width = BorderAll(1)}) {
      Box("NotifDot", .layout = {.sizing = {Fixed(8), Fixed(8)}},
          .backgroundColor = COLOR_ACCENT, .cornerRadius = RadiusAll(999));
      Column("NotifText",
             .layout = {.sizing = {Grow(), Fit()}, .childGap = 2}) {
        Text("New release", .fontSize = 14, .textColor = COLOR_TEXT);
        Text("v0.0.1-alpha01 tagged — CC_Scroll lands with this build.",
             .fontSize = 12, .textColor = COLOR_TEXT,
             .wrapMode = CC_TEXT_WRAP_WORDS);
      }
    }
  }
}

/* ------- frame ----------------------------------------------------------- */

static void frame(void) {
  canvas_state.t = (float)GetTime();

  CC_Begin();
  Column("Root",
         .layout = {.sizing = SizingAll(Grow()),
                    .padding = PadAll(20),
                    .childGap = 16},
         .backgroundColor = COLOR_BG) {

    /* Header — animated gradient canvas + brand. DrawRow's paint
     * callback runs before children, so the avatar / text / pill
     * render on top of the gradient. */
    DrawRow("Header", paint_canvas, &canvas_state,
            .layout = {.sizing = {Grow(), Fixed(140)},
                       .padding = PadAll(20),
                       .childGap = 14,
                       .childAlignment = ChildAlign(.y = AlignYCenter())},
            .cornerRadius = RadiusAll(12),
            .border = {.color = COLOR_BORDER, .width = BorderAll(1)}) {
      if (avatar_tex && avatar_tex->id != 0) {
        Image("Avatar", ImgCrop(avatar_tex),
              .layout = {.sizing = {Fixed(64), Fixed(64)}},
              .cornerRadius = RadiusAll(999));
      } else {
        Box("AvatarFallback",
            .layout = {.sizing = {Fixed(64), Fixed(64)},
                       .childAlignment = ChildAlign(.x = AlignXCenter(),
                                                    .y = AlignYCenter())},
            .backgroundColor = COLOR_ACCENT, .cornerRadius = RadiusAll(999)) {
          Text("cc", .fontSize = 24, .textColor = COLOR_BG);
        }
      }
      Column("HeaderText",
             .layout = {.sizing = {Grow(), Fit()}, .childGap = 4}) {
        Text("ccompose", .fontSize = 30, .textColor = COLOR_TEXT);
        Text("Declarative C UIs on top of Clay + raylib", .fontSize = 14,
             .textColor = COLOR_MUTED, .wrapMode = CC_TEXT_WRAP_WORDS);
      }
      /* No border here — combining RadiusAll(999) with a border makes the
       * raylib renderer draw huge arcs (DrawRing uses the raw radius, and
       * Clay does not clamp it to the element size). */
      Box("VersionPill", .layout = {.padding = PadSymmetric(12, 6)},
          .backgroundColor = COLOR_SURFACE, .cornerRadius = RadiusAll(999)) {
        Text("v0.0.1-alpha01", .fontSize = 12, .textColor = COLOR_MUTED);
      }
    }

    /* Body — sidebar + content. */
    Row("Body", .layout = {.sizing = SizingAll(Grow()), .childGap = 16}) {

      Column("Sidebar",
             .layout = {.sizing = {Fixed(220), Grow()},
                        .padding = PadAll(14),
                        .childGap = 4},
             .backgroundColor = COLOR_SURFACE, .cornerRadius = RadiusAll(12),
             .border = {.color = COLOR_BORDER, .width = BorderAll(1)}) {
        Text("PAGES", .fontSize = 11, .textColor = COLOR_MUTED,
             .letterSpacing = 1);
        Spacer(.height = Fixed(4));
        NavEntry("NavHome", PAGE_HOME);
        NavEntry("NavLayout", PAGE_LAYOUT);
        NavEntry("NavInteract", PAGE_INTERACT);
        NavEntry("NavScroll", PAGE_SCROLL);
        NavEntry("NavTrans", PAGE_TRANS);
        VSpacer();
        Text("ESC to quit", .fontSize = 11, .textColor = COLOR_MUTED);
      }

      Column("Content",
             .layout = {.sizing = SizingAll(Grow()),
                        .padding = PadAll(22),
                        .childGap = 14},
             .backgroundColor = COLOR_SURFACE, .cornerRadius = RadiusAll(12),
             .border = {.color = COLOR_BORDER, .width = BorderAll(1)}) {
        Text(page_title(current_page), .fontSize = 26, .textColor = COLOR_TEXT);
        Text(page_blurb(current_page), .fontSize = 14, .textColor = COLOR_MUTED,
             .wrapMode = CC_TEXT_WRAP_WORDS, .lineHeight = 20);
        Spacer(.height = Fixed(12));

        switch (current_page) {
        case PAGE_HOME:
          render_home();
          break;
        case PAGE_LAYOUT:
          render_layout();
          break;
        case PAGE_INTERACT:
          render_interact();
          break;
        case PAGE_SCROLL:
          render_scroll();
          break;
        case PAGE_TRANS:
          render_trans();
          break;
        }
      }
    }
  }
  CC_End();
}

/* ------- main ------------------------------------------------------------ */

int main(void) {
  CC_SetWindow(1040, 700, "ccompose showcase");
  CC_SetBackground(COLOR_BG);
  CC_Init();

  int global_font_id =
      CC_InitFont(CC_LoadFont("examples/resources/Inter-Regular.ttf", 28), 16);
  CC_SetGlobalFontColor(COLOR_TEXT);
  if (global_font_id < 0) {
    fprintf(stderr,
            "ccompose demo: global font load failed; using raylib default\n");
  }

  Texture2D avatar_texture = CC_LoadImage("examples/resources/profile.png");
  if (avatar_texture.id != 0)
    avatar_tex = &avatar_texture;

  CC_RUN_LOOP(frame);

  if (avatar_tex)
    CC_UnloadImage(avatar_texture);
  CC_Shutdown();
  return 0;
}
