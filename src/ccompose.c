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

#ifndef CCOMPOSE_NO_BACKEND
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

static void cc__default_error_handler(Clay_ErrorData error) {
  fprintf(stderr, "ccompose/clay error: %.*s\n", (int)error.errorText.length,
          error.errorText.chars);
}

/* ---------- Backend: raylib ---------- */

#ifndef CCOMPOSE_NO_BACKEND

#define CC_MAX_FONTS 16
static Font cc__fonts[CC_MAX_FONTS];
static int cc__font_count = 0;
static int cc__font_global_id = 0;

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

void CC_SetBackground(Clay_Color color) { cc__background_color = color; }

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

bool CC_Running(void) { return !WindowShouldClose(); }

bool CC__MousePressedThisFrame(void) {
  return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

#else /* CCOMPOSE_NO_BACKEND */

void CC_SetWindow(int width, int height, const char *title) {
  (void)width;
  (void)height;
  (void)title;
}
void CC_SetWindowFlags(unsigned int raylib_flags) { (void)raylib_flags; }
void CC_SetBackground(Clay_Color color) { (void)color; }
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
bool CC_Running(void) { return true; }
bool CC__MousePressedThisFrame(void) { return false; }

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

void CC_Shutdown(void) {
#ifndef CCOMPOSE_NO_BACKEND
  if (cc__initialized) {
    cc__backend_shutdown();
  }
#endif
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
#ifndef CCOMPOSE_NO_BACKEND
  cc__backend_begin_frame();
#endif
  Clay_BeginLayout();
}

CC_RenderCommandArray CC_End(void) {
#ifndef CCOMPOSE_NO_BACKEND
  Clay_RenderCommandArray commands = Clay_EndLayout(GetFrameTime());
  cc__backend_end_frame(commands);
  return commands;
#else
  return Clay_EndLayout(0.0f);
#endif
}

/* ---------- Element scope ---------- */

CC_Scope CC_OpenElement(const char *id_chars, int32_t id_len,
                        Clay_LayoutDirection direction,
                        Clay_ElementDeclaration decl) {
  decl.layout.layoutDirection = direction;
  if (id_len == 0) {
    /* Anonymous element — Clay generates an internal ID. */
    Clay__OpenElement();
  } else {
    Clay_String id_string = (Clay_String){
        .isStaticallyAllocated = true,
        .length = id_len,
        .chars = id_chars,
    };
    Clay__OpenElementWithId(Clay__HashString(id_string, 0));
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
