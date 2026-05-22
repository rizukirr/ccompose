/* Headless transition test — compiles with -DCCOMPOSE_NO_BACKEND.
 *
 * Exercises:
 *   - DefineTransition macro generates correct config
 *   - Enter/exit callbacks modify TransitionData as expected
 *   - Easing functions produce correct interpolation values
 *   - Property bitmask auto-inference from slide/fade/raw fields
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "ccompose.h"

/* ---- Tolerance for float comparison ---- */
#define APPROX(a, b) (fabsf((a) - (b)) < 0.01f)

/* ---- DefineTransition: slide + fade ---- */
DefineTransition(slide_fade, 0.4f, .enter = {.slideX = -200, .fade = true},
                 .exit = {.slideX = -200, .fade = true}, );

/* ---- DefineTransition: raw Clay properties ---- */
DefineTransition(color_anim, 0.5f,
                 .properties = CC_TRANSITION_PROPERTY_BACKGROUND_COLOR,
                 .enter = {
                     .backgroundColor = {255, 0, 0, 255},
                 }, );

/* ---- DefineTransition: custom handler ---- */
DefineTransition(linear_slide, 0.3f, .handler = CC_Linear,
                 .enter = {.slideY = -40}, );

/* ---- DefineTransition: exit-only ---- */
DefineTransition(exit_only, 0.3f, .exit = {.slideX = 100, .fade = true}, );

/* ---- DefineTransition: mixed presets + raw ---- */
DefineTransition(mixed, 0.4f,
                 .enter =
                     {
                         .slideX = -100,
                         .fade = true,
                         .overlayColor = {0, 0, 0, 128},
                     },
                 .exit = {.slideX = -100, .fade = true}, );

static void test_slide_fade_config(void) {
  printf("  slide_fade config... ");

  CC_TransitionElementConfig cfg = slide_fade();

  assert(cfg.handler == CC_EaseOut);
  assert(APPROX(cfg.duration, 0.4f));
  assert(cfg.properties & CC_TRANSITION_PROPERTY_X);
  assert(cfg.properties & CC_TRANSITION_PROPERTY_BACKGROUND_COLOR);
  assert(cfg.enter.setInitialState != NULL);
  assert(cfg.exit.setFinalState != NULL);

  printf("OK\n");
}

static void test_enter_callback(void) {
  printf("  enter callback... ");

  CC_TransitionElementConfig cfg = slide_fade();
  CC_TransitionData target = {
      .boundingBox = {.x = 100, .y = 50, .width = 200, .height = 100},
      .backgroundColor = {30, 30, 30, 255},
  };

  CC_TransitionData result = cfg.enter.setInitialState(target, cfg.properties);

  assert(APPROX(result.boundingBox.x, -100.0f));
  assert(APPROX(result.boundingBox.y, 50.0f));
  assert(APPROX(result.backgroundColor.a, 0.0f));
  assert(APPROX(result.backgroundColor.r, 30.0f));

  printf("OK\n");
}

static void test_exit_callback(void) {
  printf("  exit callback... ");

  CC_TransitionElementConfig cfg = slide_fade();
  CC_TransitionData initial = {
      .boundingBox = {.x = 100, .y = 50, .width = 200, .height = 100},
      .backgroundColor = {30, 30, 30, 255},
  };

  CC_TransitionData result = cfg.exit.setFinalState(initial, cfg.properties);

  assert(APPROX(result.boundingBox.x, -100.0f));
  assert(APPROX(result.backgroundColor.a, 0.0f));

  printf("OK\n");
}

static void test_color_anim_config(void) {
  printf("  color_anim config... ");

  CC_TransitionElementConfig cfg = color_anim();

  assert(cfg.properties == CC_TRANSITION_PROPERTY_BACKGROUND_COLOR);
  assert(cfg.enter.setInitialState != NULL);

  CC_TransitionData target = {
      .backgroundColor = {30, 30, 30, 255},
  };
  CC_TransitionData result = cfg.enter.setInitialState(target, cfg.properties);

  assert(APPROX(result.backgroundColor.r, 255.0f));
  assert(APPROX(result.backgroundColor.g, 0.0f));

  printf("OK\n");
}

static void test_custom_handler(void) {
  printf("  custom handler... ");

  CC_TransitionElementConfig cfg = linear_slide();

  assert(cfg.handler == CC_Linear);
  assert(APPROX(cfg.duration, 0.3f));
  assert(cfg.properties & CC_TRANSITION_PROPERTY_Y);

  printf("OK\n");
}

static void test_exit_only(void) {
  printf("  exit-only... ");

  CC_TransitionElementConfig cfg = exit_only();

  assert(cfg.enter.setInitialState == NULL);
  assert(cfg.exit.setFinalState != NULL);
  assert(cfg.properties & CC_TRANSITION_PROPERTY_X);
  assert(cfg.properties & CC_TRANSITION_PROPERTY_BACKGROUND_COLOR);

  printf("OK\n");
}

static void test_mixed_config(void) {
  printf("  mixed presets + raw... ");

  CC_TransitionElementConfig cfg = mixed();

  assert(cfg.properties & CC_TRANSITION_PROPERTY_X);
  assert(cfg.properties & CC_TRANSITION_PROPERTY_BACKGROUND_COLOR);
  assert(cfg.properties & CC_TRANSITION_PROPERTY_OVERLAY_COLOR);

  CC_TransitionData target = {
      .boundingBox = {.x = 200},
      .backgroundColor = {40, 40, 40, 255},
      .overlayColor = {0, 0, 0, 0},
  };
  CC_TransitionData result = cfg.enter.setInitialState(target, cfg.properties);

  assert(APPROX(result.boundingBox.x, 100.0f));
  assert(APPROX(result.backgroundColor.a, 0.0f));
  assert(APPROX(result.overlayColor.a, 128.0f));

  printf("OK\n");
}

static void test_easing_linear(void) {
  printf("  CC_Linear... ");

  CC_TransitionData initial = {
      .boundingBox = {.x = 0, .y = 0, .width = 100, .height = 100},
  };
  CC_TransitionData current = initial;
  CC_TransitionData target = {
      .boundingBox = {.x = 200, .y = 0, .width = 100, .height = 100},
  };

  CC_TransitionArgs args = {
      .transitionState = CC_TRANSITION_STATE_TRANSITIONING,
      .initial = initial,
      .current = &current,
      .target = target,
      .elapsedTime = 0.25f,
      .duration = 0.5f,
      .properties = CC_TRANSITION_PROPERTY_X,
  };

  bool done = CC_Linear(args);
  assert(!done);
  assert(APPROX(current.boundingBox.x, 100.0f));

  args.elapsedTime = 0.5f;
  done = CC_Linear(args);
  assert(done);
  assert(APPROX(current.boundingBox.x, 200.0f));

  printf("OK\n");
}

static void test_easing_ease_in(void) {
  printf("  CC_EaseIn... ");

  CC_TransitionData initial = {
      .boundingBox = {.x = 0},
  };
  CC_TransitionData current = initial;
  CC_TransitionData target = {
      .boundingBox = {.x = 100},
  };

  CC_TransitionArgs args = {
      .initial = initial,
      .current = &current,
      .target = target,
      .elapsedTime = 0.5f,
      .duration = 1.0f,
      .properties = CC_TRANSITION_PROPERTY_X,
  };

  CC_EaseIn(args);
  assert(APPROX(current.boundingBox.x, 12.5f));

  args.elapsedTime = 1.0f;
  CC_EaseIn(args);
  assert(APPROX(current.boundingBox.x, 100.0f));

  printf("OK\n");
}

static void test_easing_ease_in_out(void) {
  printf("  CC_EaseInOut... ");

  CC_TransitionData initial = {
      .boundingBox = {.x = 0},
  };
  CC_TransitionData current = initial;
  CC_TransitionData target = {
      .boundingBox = {.x = 100},
  };

  CC_TransitionArgs args = {
      .initial = initial,
      .current = &current,
      .target = target,
      .elapsedTime = 0.5f,
      .duration = 1.0f,
      .properties = CC_TRANSITION_PROPERTY_X,
  };

  CC_EaseInOut(args);
  assert(APPROX(current.boundingBox.x, 50.0f));

  args.elapsedTime = 1.0f;
  CC_EaseInOut(args);
  assert(APPROX(current.boundingBox.x, 100.0f));

  printf("OK\n");
}

int main(void) {
  printf("transition_test:\n");

  test_slide_fade_config();
  test_enter_callback();
  test_exit_callback();
  test_color_anim_config();
  test_custom_handler();
  test_exit_only();
  test_mixed_config();
  test_easing_linear();
  test_easing_ease_in();
  test_easing_ease_in_out();

  printf("All transition tests passed.\n");
  return 0;
}
