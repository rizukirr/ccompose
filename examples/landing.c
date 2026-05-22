/* ccompose landing page.
 *
 * Centered marketing text over a procedural star-dust background.
 * Works identically on native and web (emscripten) builds because the
 * background is CPU-drawn via raylib primitives — no GLSL version gymnastics.
 *
 * Native:
 *     cmake -S . -B build && cmake --build build
 *     ./build/examples/ccompose_landing
 *
 * Web:
 *     emcmake cmake -S . -B build-web -DCCOMPOSE_BUILD_TESTS=OFF
 *     cmake --build build-web
 *     python3 -m http.server -d build-web/examples 8080
 */

#include "../include/ccompose.h"
#include "raylib.h"
#include <math.h>
#include <stdlib.h>

#define STAR_COUNT 420

typedef struct {
  float nx, ny; /* normalized position 0..1  */
  float vx, vy; /* drift velocity            */
  float radius; /* base radius               */
  float phase;  /* twinkle phase offset      */
  float depth;  /* 0..1 — parallax + alpha   */
  unsigned char r, g, b;
} Star;

typedef struct {
  Star stars[STAR_COUNT];
  float t;
  int initialized;
} Starfield;

static const CC_Color COLOR_TEXT = {240, 242, 248, 255};
static const CC_Color COLOR_MUTED = {150, 158, 180, 255};
static const CC_Color COLOR_ACCENT = {165, 205, 255, 255};

static Starfield g_field = {0};

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

static void init_starfield(Starfield *f) {
  srand(1337);
  for (int i = 0; i < STAR_COUNT; i++) {
    Star *s = &f->stars[i];
    s->nx = frand();
    s->ny = frand();
    s->depth = 0.2f + frand() * 0.8f;
    s->vx = (frand() - 0.5f) * 0.01f * s->depth;
    s->vy = (frand() * 0.6f + 0.2f) * 0.006f * s->depth;
    s->radius = 0.4f + frand() * 1.8f * s->depth;
    s->phase = frand() * 6.2831853f;
    /* Palette: faint blue/white/gold dust. */
    float hue = frand();
    if (hue < 0.55f) {
      s->r = 210;
      s->g = 220;
      s->b = 255;
    } else if (hue < 0.9f) {
      s->r = 245;
      s->g = 245;
      s->b = 255;
    } else {
      s->r = 255;
      s->g = 220;
      s->b = 170;
    }
  }
  f->initialized = 1;
}

static void paint_starfield(CC_BoundingBox bb, void *user) {
  Starfield *f = (Starfield *)user;
  if (!f->initialized)
    init_starfield(f);

  /* Deep-space vertical gradient. */
  DrawRectangleGradientV((int)bb.x, (int)bb.y, (int)bb.width, (int)bb.height,
                         (Color){8, 10, 24, 255}, (Color){18, 12, 36, 255});

  /* Soft radial glow near the center. */
  float cx = bb.x + bb.width * 0.5f;
  float cy = bb.y + bb.height * 0.5f;
  for (int i = 8; i > 0; i--) {
    float k = (float)i / 8.0f;
    unsigned char a = (unsigned char)(14 * k);
    DrawPoly((Vector2){cx, cy}, 128, 320.0f * k, 0.0f,
             (Color){90, 120, 220, a});
  }

  /* Stars with drift + twinkle. */
  for (int i = 0; i < STAR_COUNT; i++) {
    Star *s = &f->stars[i];
    s->nx += s->vx * GetFrameTime();
    s->ny += s->vy * GetFrameTime();
    if (s->nx < 0)
      s->nx += 1.0f;
    if (s->nx > 1)
      s->nx -= 1.0f;
    if (s->ny > 1)
      s->ny -= 1.0f;
    if (s->ny < 0)
      s->ny += 1.0f;

    float twinkle = 0.55f + 0.45f * sinf(f->t * 2.0f + s->phase);
    unsigned char alpha =
        (unsigned char)(255.0f * twinkle * (0.35f + 0.65f * s->depth));
    float px = bb.x + s->nx * bb.width;
    float py = bb.y + s->ny * bb.height;
    DrawCircleV((Vector2){px, py}, s->radius * (0.8f + 0.4f * twinkle),
                (Color){s->r, s->g, s->b, alpha});
  }
}

static void frame(void) {
  CC_Begin();
  g_field.t = (float)GetTime();

  DrawColumn("Root", paint_starfield, &g_field,
             .layout = {.sizing = {Grow(), Grow()},
                        .padding = PadAll(24),
                        .childAlignment = {.x = CC_ALIGN_X_CENTER,
                                           .y = CC_ALIGN_Y_CENTER}}) {

    Column("Center", .layout = {.sizing = {Fit(), Fit()},
                                .childGap = 18,
                                .childAlignment = {.x = CC_ALIGN_X_CENTER}}) {

      Text("CCompose", .textColor = COLOR_TEXT, .fontSize = 96);

      Text("Jetpack Compose-Like UI library in C", .textColor = COLOR_MUTED,
           .fontSize = 22, .wrapMode = CC_TEXT_WRAP_WORDS);

      Box("Pill",
          .layout = {.padding = PadSymmetric(20, 10),
                     .childAlignment = {.x = CC_ALIGN_X_CENTER,
                                        .y = CC_ALIGN_Y_CENTER}},
          .backgroundColor = {255, 255, 255, 18},
          .cornerRadius = RadiusAll(20)) {
        Text("Can your React do this?", .textColor = COLOR_ACCENT,
             .fontSize = 18);
      }
    }
  }

  CC_End();
}

int main(void) {
  CC_SetWindow(1100, 720, "ccompose — landing");
  CC_SetBackground((CC_Color){0, 0, 0, 255});
  CC_Init();

  CC_LoadGlobalFont("examples/resources/Roboto-Regular.ttf", 96);

  CC_RUN_LOOP(frame);

  CC_Shutdown();
  return 0;
}
