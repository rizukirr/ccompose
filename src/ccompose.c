/* ccompose.c — implementation for ccompose.h.
 *
 * This file owns:
 *   - Clay's static implementation (via CLAY_IMPLEMENTATION)
 *   - The raylib renderer's implementation (when the raylib backend is on)
 *   - All CC_* lifecycle, element-scope, and font-management functions
 *
 * Compile with:
 *   - -Iinclude       (for ccompose.h + clay.h)
 *   - -Irenderer/raylib (for clay_renderer_raylib.c + its local raylib.h)
 *   - -lraylib -lm    (link)
 *
 * Or just let CMake do it via the `ccompose` target in the top-level
 * CMakeLists.txt.
 */

#define CLAY_IMPLEMENTATION
#include "ccompose.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef CCOMPOSE_BACKEND_RAYLIB
/* Clay's raylib renderer is distributed as a .c file that defines the
 * Clay_Raylib_Initialize / Clay_Raylib_Render / Clay_Raylib_Close
 * functions plus the Raylib_MeasureText callback. Including it here
 * puts everything in one TU alongside Clay's own implementation. */
#include "clay_renderer_raylib.c"
#endif

/* ---------- Shared state ---------- */

static void *cc__arena_memory = NULL;
static bool cc__initialized = false;
static Clay_Dimensions cc__viewport = {800.0f, 600.0f};
static void (*cc__error_handler_override)(Clay_ErrorData) = NULL;

static CC_QuitFn cc__quit_fn = NULL;
static void *cc__quit_user = NULL;
static bool cc__quit_requested = false;

typedef struct {
  uint32_t hash;
  int32_t length;
  const char *chars;
} CC__InternEntry;

static CC__InternEntry *cc__intern_table = NULL;
static size_t cc__intern_cap = 0;
static size_t cc__intern_count = 0;

static uint32_t cc__intern_hash(const char *s, int32_t length) {
  uint32_t hash = 2166136261u;
  for (int32_t i = 0; i < length; ++i) {
    hash ^= (uint8_t)s[i];
    hash *= 16777619u;
  }
  return hash ? hash : 1u;
}

static void cc__intern_grow(void) {
  size_t new_cap = cc__intern_cap ? (cc__intern_cap * 2u) : 1024u;
  CC__InternEntry *new_table =
      (CC__InternEntry *)calloc(new_cap, sizeof(CC__InternEntry));
  if (new_table == NULL) {
    fprintf(stderr, "ccompose: out of memory growing string intern table\n");
    exit(EXIT_FAILURE);
  }

  for (size_t i = 0; i < cc__intern_cap; ++i) {
    if (cc__intern_table[i].chars == NULL)
      continue;

    size_t slot = (size_t)cc__intern_table[i].hash & (new_cap - 1u);
    while (new_table[slot].chars != NULL) {
      slot = (slot + 1u) & (new_cap - 1u);
    }
    new_table[slot] = cc__intern_table[i];
  }

  free(cc__intern_table);
  cc__intern_table = new_table;
  cc__intern_cap = new_cap;
}

static const char *cc__intern_get_or_add(const char *s, int32_t length) {
  if (cc__intern_cap == 0 ||
      (cc__intern_count + 1u) * 10u >= cc__intern_cap * 7u) {
    cc__intern_grow();
  }

  uint32_t hash = cc__intern_hash(s, length);
  size_t slot = (size_t)hash & (cc__intern_cap - 1u);

  while (cc__intern_table[slot].chars != NULL) {
    CC__InternEntry *entry = &cc__intern_table[slot];
    if (entry->hash == hash && entry->length == length &&
        memcmp(entry->chars, s, (size_t)length) == 0) {
      return entry->chars;
    }
    slot = (slot + 1u) & (cc__intern_cap - 1u);
  }

  char *copy = (char *)malloc((size_t)length + 1u);
  if (copy == NULL) {
    fprintf(stderr, "ccompose: out of memory interning string\n");
    exit(EXIT_FAILURE);
  }
  memcpy(copy, s, (size_t)length);
  copy[length] = '\0';

  cc__intern_table[slot] =
      (CC__InternEntry){.hash = hash, .length = length, .chars = copy};
  cc__intern_count++;
  return copy;
}

static void cc__intern_shutdown(void) {
  if (cc__intern_table == NULL)
    return;

  for (size_t i = 0; i < cc__intern_cap; ++i) {
    if (cc__intern_table[i].chars != NULL) {
      free((void *)cc__intern_table[i].chars);
    }
  }

  free(cc__intern_table);
  cc__intern_table = NULL;
  cc__intern_cap = 0;
  cc__intern_count = 0;
}

static void cc__default_error_handler(Clay_ErrorData error) {
  fprintf(stderr, "ccompose/clay error: %.*s\n", (int)error.errorText.length,
          error.errorText.chars);
}

/* ---------- Backend: raylib ---------- */

#ifdef CCOMPOSE_BACKEND_RAYLIB

#define CC_MAX_FONTS 16
static Font cc__fonts[CC_MAX_FONTS];
static int cc__font_count = 0;
static int cc__font_global_id = 0;

/* Per-frame pool of CC_ImageRef slots. ImgFill/Fit/Crop hand out slots
 * from this pool so the pointer they return outlives the call-site
 * block scope that a compound literal would give them. Reset by
 * CC_Begin() at the top of every frame. */
#define CC_IMAGE_REF_POOL_SIZE 128
static CC_ImageRef cc__image_ref_pool[CC_IMAGE_REF_POOL_SIZE];
static int cc__image_ref_pool_used = 0;

CC_ImageRef *CC_AcquireImageRef(Texture2D *texture, CC_ImageScale scale) {
  if (cc__image_ref_pool_used >= CC_IMAGE_REF_POOL_SIZE)
    return NULL;
  CC_ImageRef *r = &cc__image_ref_pool[cc__image_ref_pool_used++];
  r->texture = texture;
  r->scale = scale;
  return r;
}

#define CC_DRAW_POOL_SIZE 128
#define CC_DRAW_SLOT_TAG (-1)

static CC_DrawSlot cc__draw_pool[CC_DRAW_POOL_SIZE];
static int cc__draw_pool_used = 0;
static int cc__window_width = 800;
static int cc__window_height = 600;
static const char *cc__window_title = "ccompose";
static unsigned int cc__window_flags =
    FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
static Clay_Color cc__background_color = {0, 0, 0, 255};

void CC_SetWindow(int width, int height, const char *title) {
  cc__window_width = width;
  cc__window_height = height;
  if (title)
    cc__window_title = title;
}

void CC_SetWindowFlags(unsigned int raylib_flags) {
  cc__window_flags = raylib_flags;
}

void CC_SetBackground(CC_Color color) { cc__background_color = color; }

int CC_LoadGlobalFont(const char *path, int base_size) {
  if (!cc__initialized || cc__font_count >= CC_MAX_FONTS)
    return -1;

  int f = CC_LoadFont(path, base_size);
  if (f < 0)
    return -1;
  cc__font_global_id = f;
  return cc__font_global_id;
}

int CC_GetGlobalFontId(void) {
  return (cc__font_global_id >= 0) ? cc__font_global_id : 0;
}

int CC_LoadFont(const char *path, int base_size) {
  if (!cc__initialized || cc__font_count >= CC_MAX_FONTS)
    return -1;
  Font f = LoadFontEx(path, base_size, 0, 400);
  if (f.glyphCount == 0) {
    /* Load failed — raylib returns its default font in that case.
     * Don't burn a slot on it. */
    return -1;
  }
  SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
  cc__fonts[cc__font_count] = f;
  return cc__font_count++;
}

Texture2D CC_LoadImage(const char *path) { return LoadTexture(path); }

void CC_UnloadImage(Texture2D texture) { UnloadTexture(texture); }

static void cc__backend_init(void) {
  Clay_Raylib_Initialize(cc__window_width, cc__window_height, cc__window_title,
                         cc__window_flags);
  cc__fonts[0] = GetFontDefault();
  cc__font_count = 1;
  cc__font_global_id = 0;
  cc__viewport =
      (Clay_Dimensions){(float)GetScreenWidth(), (float)GetScreenHeight()};
}

static void cc__backend_after_clay_init(void) {
  Clay_SetMeasureTextFunction(Raylib_MeasureText, cc__fonts);
}

static void cc__backend_begin_frame(void) {
  Clay_SetLayoutDimensions(
      (Clay_Dimensions){(float)GetScreenWidth(), (float)GetScreenHeight()});
  Vector2 mouse = GetMousePosition();
  Clay_SetPointerState((Clay_Vector2){mouse.x, mouse.y},
                       IsMouseButtonDown(MOUSE_BUTTON_LEFT));
  Vector2 wheel = GetMouseWheelMoveV();
  Clay_UpdateScrollContainers(true, (Clay_Vector2){wheel.x, wheel.y},
                              GetFrameTime());
  cc__draw_pool_used = 0;
}

static void cc__backend_end_frame(Clay_RenderCommandArray commands) {
  BeginDrawing();
  ClearBackground((Color){
      (unsigned char)cc__background_color.r,
      (unsigned char)cc__background_color.g,
      (unsigned char)cc__background_color.b,
      (unsigned char)cc__background_color.a,
  });
  Clay_Raylib_Render(commands, cc__fonts);
  EndDrawing();
}

static void cc__backend_shutdown(void) {
  for (int i = 1; i < cc__font_count; ++i) {
    UnloadFont(cc__fonts[i]);
  }
  cc__font_count = 0;
  cc__font_global_id = 0;
  Clay_Raylib_Close();
}

static float cc__backend_frame_time(void) { return GetFrameTime(); }

bool CC_Running(void) {
  if (cc__quit_requested)
    return false;
  if (WindowShouldClose())
    return false;
  if (cc__quit_fn) {
    if (cc__quit_fn(cc__quit_user))
      return false;
  } else if (IsKeyPressed(KEY_ESCAPE)) {
    return false;
  }
  return true;
}

bool CC__MousePressedThisFrame(void) {
  return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int cc__map_key(CC_Key k) {
  switch (k) {
  case CC_KEY_ESCAPE: return KEY_ESCAPE;
  case CC_KEY_ENTER:  return KEY_ENTER;
  case CC_KEY_SPACE:  return KEY_SPACE;
  case CC_KEY_TAB:    return KEY_TAB;
  case CC_KEY_Q:      return KEY_Q;
  case CC_KEY_W:      return KEY_W;
  case CC_KEY_E:      return KEY_E;
  case CC_KEY_R:      return KEY_R;
  case CC_KEY_LEFT:   return KEY_LEFT;
  case CC_KEY_RIGHT:  return KEY_RIGHT;
  case CC_KEY_UP:     return KEY_UP;
  case CC_KEY_DOWN:   return KEY_DOWN;
  default:            return 0;
  }
}

bool CC_KeyPressed(CC_Key key) {
  int rk = cc__map_key(key);
  return rk ? IsKeyPressed(rk) : false;
}

CC_DrawSlot *CC_AcquireDrawSlot(CC_DrawFn fn, void *user) {
  if (cc__draw_pool_used >= CC_DRAW_POOL_SIZE)
    return NULL;

  CC_DrawSlot *s = &cc__draw_pool[cc__draw_pool_used++];
  s->_tag = CC_DRAW_SLOT_TAG;
  s->fn = fn;
  s->user = user;
  return s;
}

#elif defined(CCOMPOSE_BACKEND_TERMBOX2)

/* termbox2 backend lives in its own renderer file (mirrors how raylib
 * splits Clay_Raylib_* into clay_renderer_raylib.c). It defines the
 * cc__backend_* hooks plus all CC_Set..., CC_Load..., CC_Running, and
 * CC_KeyPressed symbols that the headless block stubs out below. */
#include "clay_renderer_termbox2.c"

#else /* CCOMPOSE_NO_BACKEND */

void CC_SetWindow(int width, int height, const char *title) {
  (void)width;
  (void)height;
  (void)title;
}
void CC_SetWindowFlags(unsigned int raylib_flags) { (void)raylib_flags; }
void CC_SetBackground(Clay_Color color) { (void)color; }
CC_DrawSlot *CC_AcquireDrawSlot(CC_DrawFn fn, void *user) {
  (void)fn;
  (void)user;
  return NULL;
}
int CC_LoadFont(const char *path, int base_size) {
  (void)path;
  (void)base_size;
  return -1;
}
int CC_LoadGlobalFont(const char *path, int base_size) {
  (void)path;
  (void)base_size;
  return -1;
}
int CC_GetGlobalFontId(void) { return 0; }
bool CC_Running(void) {
  if (cc__quit_requested)
    return false;
  if (cc__quit_fn && cc__quit_fn(cc__quit_user))
    return false;
  return true;
}
bool CC__MousePressedThisFrame(void) { return false; }
bool CC_KeyPressed(CC_Key key) { (void)key; return false; }

static Clay_Dimensions cc__default_measure_text(Clay_StringSlice text,
                                                Clay_TextElementConfig *config,
                                                void *userData) {
  (void)userData;
  float s = (config && config->fontSize > 0) ? (float)config->fontSize : 14.0f;
  return (Clay_Dimensions){(float)text.length * s * 0.5f, s};
}

#endif /* CCOMPOSE_NO_BACKEND */

/* ---------- Shared lifecycle ---------- */

void CC_SetViewport(float width, float height) {
  cc__viewport.width = width;
  cc__viewport.height = height;
  if (cc__initialized) {
    Clay_SetLayoutDimensions(cc__viewport);
  }
}

void CC_SetErrorHandler(void (*handler)(Clay_ErrorData)) {
  cc__error_handler_override = handler;
}

void CC_SetQuitHandler(CC_QuitFn fn, void *user) {
  cc__quit_fn = fn;
  cc__quit_user = user;
}

void CC_RequestQuit(void) { cc__quit_requested = true; }

bool CC_QuitRequested(void) { return cc__quit_requested; }

CC_String CC_StrIntern(const char *s) {
  if (s == NULL || s[0] == '\0') {
    return (CC_String){.isStaticallyAllocated = true, .length = 0, .chars = ""};
  }

  int32_t length = (int32_t)strlen(s);
  const char *interned = cc__intern_get_or_add(s, length);
  return (CC_String){
      .isStaticallyAllocated = true, .length = length, .chars = interned};
}

void CC_Shutdown(void) {
#ifndef CCOMPOSE_NO_BACKEND
  if (cc__initialized) {
    cc__backend_shutdown();
  }
#endif
  cc__intern_shutdown();
  if (cc__arena_memory != NULL) {
    free(cc__arena_memory);
    cc__arena_memory = NULL;
  }
  cc__initialized = false;
}

void CC_Init(void) {
  if (cc__initialized)
    return;

#ifndef CCOMPOSE_NO_BACKEND
  cc__backend_init();
#endif

  uint32_t memory_size = Clay_MinMemorySize();
  cc__arena_memory = malloc(memory_size);
  if (cc__arena_memory == NULL) {
    fprintf(stderr,
            "ccompose: out of memory allocating Clay arena (%u bytes)\n",
            (unsigned)memory_size);
    exit(EXIT_FAILURE);
  }

  Clay_Arena arena =
      Clay_CreateArenaWithCapacityAndMemory(memory_size, cc__arena_memory);
  Clay_Initialize(arena, cc__viewport,
                  (Clay_ErrorHandler){.errorHandlerFunction =
                                          cc__error_handler_override
                                              ? cc__error_handler_override
                                              : cc__default_error_handler});

#ifndef CCOMPOSE_NO_BACKEND
  cc__backend_after_clay_init();
#else
  Clay_SetMeasureTextFunction(cc__default_measure_text, NULL);
#endif

  atexit(CC_Shutdown);
  cc__initialized = true;
}

void CC_Begin(void) {
  if (!cc__initialized)
    CC_Init();
#ifdef CCOMPOSE_BACKEND_RAYLIB
  cc__image_ref_pool_used = 0;
#endif
#ifndef CCOMPOSE_NO_BACKEND
  cc__backend_begin_frame();
#endif
  Clay_BeginLayout();
}

CC_RenderCommandArray CC_End(void) {
#ifndef CCOMPOSE_NO_BACKEND
  Clay_RenderCommandArray commands = Clay_EndLayout(cc__backend_frame_time());
  cc__backend_end_frame(commands);
  return commands;
#else
  return Clay_EndLayout(0.0f);
#endif
}

/* ---------- Element scope ---------- */

/* Fallback colour when caller leaves .color zeroed. Alpha != 0 so it's
 * visible, mid-grey matches typical dark-theme dividers., */
static const CC_Color CC_DividerDefaultColor = {0x50, 0x54, 0x5C, 0xFF};

static void cc__leaf(CC_LayoutDirection dir, CC_ElementDeclaration decl) {
  decl.layout.layoutDirection = dir;
  Clay__OpenElement();
  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();
}

void CC_HSpacer(void) {
  cc__leaf(CC_LEFT_TO_RIGHT,
           (CC_ElementDeclaration){
               .layout = {.sizing = {.width = CLAY_SIZING_GROW(0, 0),
                                     .height = CLAY_SIZING_FIT(0, 0)}}});
}

void CC_VSpacer(void) {
  cc__leaf(CC_TOP_TO_BOTTOM,
           (CC_ElementDeclaration){
               .layout = {.sizing = {.width = CLAY_SIZING_GROW(0, 0),
                                     .height = CLAY_SIZING_FIT(0, 0)}}});
}

void CC_HDivider(CC_DividerOpts opts) {
  float t = opts.thickness > 0.0f ? opts.thickness : 1.0f;
  CC_Color c = (opts.color.a == 0) ? CC_DividerDefaultColor : opts.color;
  Clay_SizingAxis w =
      opts.size > 0.0f ? CLAY_SIZING_FIXED(opts.size) : CLAY_SIZING_GROW(0, 0);
  cc__leaf(
      CC_LEFT_TO_RIGHT,
      (CC_ElementDeclaration){
          .layout = {.sizing = {.width = w, .height = CLAY_SIZING_FIXED(t)}},
          .backgroundColor = c});
}

void CC_VDivider(CC_DividerOpts opts) {
  float t = opts.thickness > 0.0f ? opts.thickness : 1.0f;
  CC_Color c = (opts.color.a == 0) ? CC_DividerDefaultColor : opts.color;
  Clay_SizingAxis h =
      opts.size > 0.0f ? CLAY_SIZING_FIXED(opts.size) : CLAY_SIZING_GROW(0, 0);
  cc__leaf(
      CC_TOP_TO_BOTTOM,
      (CC_ElementDeclaration){
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(t), .height = h}},
          .backgroundColor = c});
}

CC_Scope CC_OpenElement(CC_String id, Clay_LayoutDirection direction,
                        Clay_ElementDeclaration decl) {
  decl.layout.layoutDirection = direction;
  if (id.length == 0) {
    /* Anonymous element — Clay generates an internal ID. */
    Clay__OpenElement();
  } else {
    Clay__OpenElementWithId(Clay__HashString(id, 0));
  }
  Clay__ConfigureOpenElement(decl);
  return (CC_Scope){.active = 1};
}

void CC_CloseScope(CC_Scope *scope) {
  if (!scope->active)
    return;
  Clay__CloseElement();
  scope->active = 0;
}

/* ========================================================================
 * Easing functions
 * ========================================================================
 *
 * Each function follows Clay_EaseOut's contract:
 *   - Compute a curve value from elapsedTime / duration
 *   - Lerp every flagged property from initial -> target by that curve
 *   - Return true when the animation is complete (ratio >= 1)
 *
 * The interpolation body is identical across all easings — only the
 * curve differs. CC__APPLY_LERP_ factors out the repetitive part.
 */

/* (t) must be a plain variable or constant — it is evaluated once per
 * active property (up to 16 times). */
#define CC__APPLY_LERP_(args, t)                                               \
  do {                                                                         \
    if ((args).properties & CLAY_TRANSITION_PROPERTY_X)                        \
      (args).current->boundingBox.x = CLAY__LERP(                              \
          (args).initial.boundingBox.x, (args).target.boundingBox.x, (t));     \
    if ((args).properties & CLAY_TRANSITION_PROPERTY_Y)                        \
      (args).current->boundingBox.y = CLAY__LERP(                              \
          (args).initial.boundingBox.y, (args).target.boundingBox.y, (t));     \
    if ((args).properties & CLAY_TRANSITION_PROPERTY_WIDTH)                    \
      (args).current->boundingBox.width =                                      \
          CLAY__LERP((args).initial.boundingBox.width,                         \
                     (args).target.boundingBox.width, (t));                    \
    if ((args).properties & CLAY_TRANSITION_PROPERTY_HEIGHT)                   \
      (args).current->boundingBox.height =                                     \
          CLAY__LERP((args).initial.boundingBox.height,                        \
                     (args).target.boundingBox.height, (t));                   \
    if ((args).properties & CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR) {       \
      (args).current->backgroundColor = (Clay_Color){                          \
          .r = CLAY__LERP((args).initial.backgroundColor.r,                    \
                          (args).target.backgroundColor.r, (t)),               \
          .g = CLAY__LERP((args).initial.backgroundColor.g,                    \
                          (args).target.backgroundColor.g, (t)),               \
          .b = CLAY__LERP((args).initial.backgroundColor.b,                    \
                          (args).target.backgroundColor.b, (t)),               \
          .a = CLAY__LERP((args).initial.backgroundColor.a,                    \
                          (args).target.backgroundColor.a, (t)),               \
      };                                                                       \
    }                                                                          \
    if ((args).properties & CLAY_TRANSITION_PROPERTY_OVERLAY_COLOR) {          \
      (args).current->overlayColor = (Clay_Color){                             \
          .r = CLAY__LERP((args).initial.overlayColor.r,                       \
                          (args).target.overlayColor.r, (t)),                  \
          .g = CLAY__LERP((args).initial.overlayColor.g,                       \
                          (args).target.overlayColor.g, (t)),                  \
          .b = CLAY__LERP((args).initial.overlayColor.b,                       \
                          (args).target.overlayColor.b, (t)),                  \
          .a = CLAY__LERP((args).initial.overlayColor.a,                       \
                          (args).target.overlayColor.a, (t)),                  \
      };                                                                       \
    }                                                                          \
    if ((args).properties & CLAY_TRANSITION_PROPERTY_BORDER_COLOR) {           \
      (args).current->borderColor = (Clay_Color){                              \
          .r = CLAY__LERP((args).initial.borderColor.r,                        \
                          (args).target.borderColor.r, (t)),                   \
          .g = CLAY__LERP((args).initial.borderColor.g,                        \
                          (args).target.borderColor.g, (t)),                   \
          .b = CLAY__LERP((args).initial.borderColor.b,                        \
                          (args).target.borderColor.b, (t)),                   \
          .a = CLAY__LERP((args).initial.borderColor.a,                        \
                          (args).target.borderColor.a, (t)),                   \
      };                                                                       \
    }                                                                          \
    if ((args).properties & CLAY_TRANSITION_PROPERTY_BORDER_WIDTH) {           \
      (args).current->borderWidth = (Clay_BorderWidth){                        \
          .left = (uint16_t)CLAY__LERP((args).initial.borderWidth.left,        \
                                       (args).target.borderWidth.left, (t)),   \
          .right = (uint16_t)CLAY__LERP((args).initial.borderWidth.right,      \
                                        (args).target.borderWidth.right, (t)), \
          .top = (uint16_t)CLAY__LERP((args).initial.borderWidth.top,          \
                                      (args).target.borderWidth.top, (t)),     \
          .bottom =                                                            \
              (uint16_t)CLAY__LERP((args).initial.borderWidth.bottom,          \
                                   (args).target.borderWidth.bottom, (t)),     \
          .betweenChildren = (uint16_t)CLAY__LERP(                             \
              (args).initial.borderWidth.betweenChildren,                      \
              (args).target.borderWidth.betweenChildren, (t)),                 \
      };                                                                       \
    }                                                                          \
    /* CLAY_TRANSITION_PROPERTY_CORNER_RADIUS (64): Clay_TransitionData        \
     * has no cornerRadius field; not interpolable in current Clay API         \
     * and intentionally unhandled (same as Clay_EaseOut). */                  \
  } while (0)

bool CC_Linear(CC_TransitionArgs arguments) {
  float ratio = 1.0f;
  if (arguments.duration > 0)
    ratio = CLAY__MIN(arguments.elapsedTime / arguments.duration, 1.0f);
  CC__APPLY_LERP_(arguments, ratio);
  return ratio >= 1.0f;
}

bool CC_EaseIn(CC_TransitionArgs arguments) {
  float ratio = 1.0f;
  if (arguments.duration > 0)
    ratio = CLAY__MIN(arguments.elapsedTime / arguments.duration, 1.0f);
  float curve = ratio * ratio * ratio; /* cubic ease-in */
  CC__APPLY_LERP_(arguments, curve);
  return ratio >= 1.0f;
}

bool CC_EaseInOut(CC_TransitionArgs arguments) {
  float ratio = 1.0f;
  if (arguments.duration > 0)
    ratio = CLAY__MIN(arguments.elapsedTime / arguments.duration, 1.0f);
  float curve = ratio < 0.5f
                    ? 4.0f * ratio * ratio * ratio
                    : 1.0f - (-2.0f * ratio + 2.0f) * (-2.0f * ratio + 2.0f) *
                                 (-2.0f * ratio + 2.0f) / 2.0f;
  CC__APPLY_LERP_(arguments, curve);
  return ratio >= 1.0f;
}
