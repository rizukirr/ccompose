#include "ccompose.h"
#include "raylib.h"
#include "raymath.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define CLAY_RECTANGLE_TO_RAYLIB_RECTANGLE(rectangle)                          \
  (Rectangle) {                                                                \
    .x = rectangle.x, .y = rectangle.y, .width = rectangle.width,              \
    .height = rectangle.height                                                 \
  }
#define CLAY_COLOR_TO_RAYLIB_COLOR(color)                                      \
  (Color) {                                                                    \
    .r = (unsigned char)roundf(color.r), .g = (unsigned char)roundf(color.g),  \
    .b = (unsigned char)roundf(color.b), .a = (unsigned char)roundf(color.a)   \
  }

Camera Raylib_camera;

typedef enum { CUSTOM_LAYOUT_ELEMENT_TYPE_3D_MODEL } CustomLayoutElementType;

typedef struct {
  Model model;
  float scale;
  Vector3 position;
  Matrix rotation;
} CustomLayoutElement_3DModel;

typedef struct {
  CustomLayoutElementType type;
  union {
    CustomLayoutElement_3DModel model;
  } customData;
} CustomLayoutElement;

const char *overlayShaderCode =
    "#version 330\n"
    "\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 overlayColor;\n"
    "\n"
    "out vec4 finalColor;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord) * fragColor;\n"
    "\n"
    "    vec3 blendedRGB = mix(texelColor.rgb, overlayColor.rgb, "
    "overlayColor.a);\n"
    "\n"
    "    finalColor = vec4(blendedRGB, texelColor.a);\n"
    "}";

Shader overlayShader;
int colorLoc;
bool overlayEnabled = false;

void InitOverlay() {
  overlayShader = LoadShaderFromMemory(0, overlayShaderCode);
  colorLoc = GetShaderLocation(overlayShader, "overlayColor");
}

void SetColorOverlay(Color color) {
  overlayEnabled = true;
  float colorFloat[4] = {
      (float)color.r / 255.0f,
      (float)color.g / 255.0f,
      (float)color.b / 255.0f,
      (float)color.a / 255.0f,
  };

  SetShaderValue(overlayShader, colorLoc, colorFloat, SHADER_UNIFORM_VEC4);
  BeginShaderMode(overlayShader);
}

void DisableColorOverlay() {
  if (overlayEnabled) {
    EndShaderMode();
    overlayEnabled = false;
  }
}

// Get a ray trace from the screen position (i.e mouse) within a specific
// section of the screen
Ray GetScreenToWorldPointWithZDistance(Vector2 position, Camera camera,
                                       int screenWidth, int screenHeight,
                                       float zDistance) {
  Ray ray = {0};

  // Calculate normalized device coordinates
  // NOTE: y value is negative
  float x = (2.0f * position.x) / (float)screenWidth - 1.0f;
  float y = 1.0f - (2.0f * position.y) / (float)screenHeight;
  float z = 1.0f;

  // Store values in a vector
  Vector3 deviceCoords = {x, y, z};

  // Calculate view matrix from camera look at
  Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

  Matrix matProj = MatrixIdentity();

  if (camera.projection == CAMERA_PERSPECTIVE) {
    // Calculate projection matrix from perspective
    matProj = MatrixPerspective(camera.fovy * DEG2RAD,
                                ((double)screenWidth / (double)screenHeight),
                                0.01f, zDistance);
  } else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
    double aspect = (double)screenWidth / (double)screenHeight;
    double top = camera.fovy / 2.0;
    double right = top * aspect;

    // Calculate projection matrix from orthographic
    matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
  }

  // Unproject far/near points
  Vector3 nearPoint = Vector3Unproject(
      (Vector3){deviceCoords.x, deviceCoords.y, 0.0f}, matProj, matView);
  Vector3 farPoint = Vector3Unproject(
      (Vector3){deviceCoords.x, deviceCoords.y, 1.0f}, matProj, matView);

  // Calculate normalized direction vector
  Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

  ray.position = farPoint;

  // Apply calculated vectors to ray
  ray.direction = direction;

  return ray;
}

static inline Clay_Dimensions Raylib_MeasureText(Clay_StringSlice text,
                                                 Clay_TextElementConfig *config,
                                                 void *userData) {
  // Measure string size for Font
  Clay_Dimensions textSize = {0};

  float maxTextWidth = 0.0f;
  float lineTextWidth = 0;
  int maxLineCharCount = 0;
  int lineCharCount = 0;

  float textHeight = config->fontSize;
  Font *fonts = (Font *)userData;
  Font fontToUse = fonts[config->fontId];
  // Font failed to load, likely the fonts are in the wrong place relative to
  // the execution dir. RayLib ships with a default font, so we can continue
  // with that built in one.
  if (!fontToUse.glyphs) {
    fontToUse = GetFontDefault();
  }

  float scaleFactor = config->fontSize / (float)fontToUse.baseSize;

  for (int i = 0; i < text.length; ++i, lineCharCount++) {
    if (text.chars[i] == '\n') {
      maxTextWidth = fmax(maxTextWidth, lineTextWidth);
      maxLineCharCount = CLAY__MAX(maxLineCharCount, lineCharCount);
      lineTextWidth = 0;
      lineCharCount = 0;
      continue;
    }
    int index = text.chars[i] - 32;
    if (fontToUse.glyphs[index].advanceX != 0)
      lineTextWidth += fontToUse.glyphs[index].advanceX;
    else
      lineTextWidth +=
          (fontToUse.recs[index].width + fontToUse.glyphs[index].offsetX);
  }

  maxTextWidth = fmax(maxTextWidth, lineTextWidth);
  maxLineCharCount = CLAY__MAX(maxLineCharCount, lineCharCount);

  textSize.width =
      maxTextWidth * scaleFactor + (lineCharCount * config->letterSpacing);
  textSize.height = textHeight;

  return textSize;
}

void Clay_Raylib_Initialize(int width, int height, const char *title,
                            unsigned int flags) {
  SetConfigFlags(flags);
  InitWindow(width, height, title);
  InitOverlay();
  //    EnableEventWaiting();
}

// A MALLOC'd buffer, that we keep modifying inorder to save from so many Malloc
// and Free Calls. Call Clay_Raylib_Close() to free
static char *temp_render_buffer = NULL;
static int temp_render_buffer_len = 0;

/* --- CC_ImageRef shape (ccompose divergence from upstream Clay) ---
 *
 * ccompose's Image() macro wraps the raw Texture2D pointer in a small
 * struct so it can carry a scale mode (FILL / FIT / CROP) alongside the
 * texture. The renderer dereferences imageData as CC_ImageRef* instead
 * of Texture2D*. When this file is #included from src/ccompose.c, the
 * type already exists from ccompose.h — the guards avoid redefinition.
 * When the renderer is built standalone by upstream consumers, these
 * definitions provide the type locally. */
#ifndef CC_IMAGE_REF_DEFINED_
#define CC_IMAGE_REF_DEFINED_ 1
typedef enum {
  CC_IMAGE_FILL = 0,
  CC_IMAGE_FIT,
  CC_IMAGE_CROP,
} CC_ImageScale;
#endif

#ifndef CC_IMAGE_REF_STRUCT_DEFINED_
#define CC_IMAGE_REF_STRUCT_DEFINED_ 1
typedef struct {
  Texture2D *texture;
  CC_ImageScale scale;
} CC_ImageRef;
#endif

/* --- Rounded image cache (ccompose divergence from upstream Clay) ---
 *
 * Clay computes cornerRadius for IMAGE render commands and forwards it via
 * Clay_ImageRenderData.cornerRadius, but upstream's raylib renderer ignores
 * that field and always draws images into a square bounding box. This block
 * adds a small RenderTexture2D cache so images with cornerRadius > 0 render
 * with rounded corners via a BLEND_MULTIPLIED mask pass:
 *
 *   1. Look up (or allocate) a RenderTexture2D matching the bounding box,
 *      keyed on the Clay element id.
 *   2. Clear the RT to transparent; draw a white DrawRectangleRounded at
 *      full size — that's the alpha mask.
 *   3. Switch to BLEND_MULTIPLIED and draw the source texture over the mask.
 *      Where the mask is white the texture survives; where it's transparent
 *      it multiplies to zero and disappears.
 *   4. Blit the RT to the screen at the image's bounding box (with the Y
 *      flip that GL framebuffers require).
 *
 * Entries are evicted if unused for >STALE_FRAMES, or when the table is
 * full (oldest wins). Render textures are unloaded on eviction and on
 * Clay_Raylib_Close(). */
#define CC_ROUNDED_IMAGE_CACHE_SIZE 32
#define CC_ROUNDED_IMAGE_STALE_FRAMES 120

typedef struct {
  uint32_t id; // 0 = empty slot
  int width;
  int height;
  RenderTexture2D rt;
  uint32_t last_frame;
} CC_RoundedImageCacheEntry;

static CC_RoundedImageCacheEntry
    cc__rounded_image_cache[CC_ROUNDED_IMAGE_CACHE_SIZE];
static uint32_t cc__rounded_image_frame = 0;

static void cc__rounded_image_cache_free_all(void) {
  for (int i = 0; i < CC_ROUNDED_IMAGE_CACHE_SIZE; ++i) {
    if (cc__rounded_image_cache[i].id != 0) {
      UnloadRenderTexture(cc__rounded_image_cache[i].rt);
      cc__rounded_image_cache[i] = (CC_RoundedImageCacheEntry){0};
    }
  }
}

static void cc__rounded_image_cache_sweep(void) {
  uint32_t now = cc__rounded_image_frame;
  for (int i = 0; i < CC_ROUNDED_IMAGE_CACHE_SIZE; ++i) {
    CC_RoundedImageCacheEntry *e = &cc__rounded_image_cache[i];
    if (e->id != 0 && (now - e->last_frame) > CC_ROUNDED_IMAGE_STALE_FRAMES) {
      UnloadRenderTexture(e->rt);
      *e = (CC_RoundedImageCacheEntry){0};
    }
  }
}

static CC_RoundedImageCacheEntry *cc__rounded_image_cache_get(uint32_t id,
                                                              int w, int h) {
  if (id == 0)
    return NULL; // Can't cache anonymous elements stably.
  int free_slot = -1;
  int oldest_slot = 0;
  uint32_t oldest_frame = UINT32_MAX;
  for (int i = 0; i < CC_ROUNDED_IMAGE_CACHE_SIZE; ++i) {
    CC_RoundedImageCacheEntry *e = &cc__rounded_image_cache[i];
    if (e->id == id) {
      if (e->width != w || e->height != h) {
        UnloadRenderTexture(e->rt);
        e->rt = LoadRenderTexture(w, h);
        e->width = w;
        e->height = h;
      }
      e->last_frame = cc__rounded_image_frame;
      return e;
    }
    if (e->id == 0 && free_slot < 0)
      free_slot = i;
    if (e->last_frame < oldest_frame) {
      oldest_frame = e->last_frame;
      oldest_slot = i;
    }
  }
  int slot = (free_slot >= 0) ? free_slot : oldest_slot;
  CC_RoundedImageCacheEntry *e = &cc__rounded_image_cache[slot];
  if (e->id != 0)
    UnloadRenderTexture(e->rt);
  e->id = id;
  e->width = w;
  e->height = h;
  e->rt = LoadRenderTexture(w, h);
  e->last_frame = cc__rounded_image_frame;
  return e;
}

// Call after closing the window to clean up the render buffer
void Clay_Raylib_Close() {
  if (temp_render_buffer)
    free(temp_render_buffer);
  temp_render_buffer_len = 0;
  cc__rounded_image_cache_free_all();

  CloseWindow();
}

/* ccompose draw-slot forward decl - must match CC_DrawSlot in ccompose.h */
#define CC_DRAW_SLOT_TAG (-1)
typedef struct {
  int _tag;
  void (*fn)(Clay_BoundingBox bb, void *user);
  void *user;
} CC_DrawSlotFwd;

static bool cc__is_ccompose_draw_slot(const Clay_RenderCommand *renderCommand,
                                      CC_DrawSlotFwd **out_slot) {
  if (!renderCommand ||
      renderCommand->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) {
    return false;
  }

  Clay_CustomRenderData *config = (Clay_CustomRenderData *)&renderCommand->renderData.custom;
  CustomLayoutElement *data = (CustomLayoutElement *)config->customData;
  if (!data) {
    return false;
  }

  CC_DrawSlotFwd *slot = (CC_DrawSlotFwd *)data;
  if (slot->_tag != CC_DRAW_SLOT_TAG) {
    return false;
  }

  if (out_slot) {
    *out_slot = slot;
  }
  return true;
}

static bool cc__next_command_is_same_id_background(
    const Clay_RenderCommandArray *renderCommands, int current_index) {
  int next_index = current_index + 1;
  if (!renderCommands || next_index >= renderCommands->length) {
    return false;
  }

  Clay_RenderCommand *current =
      Clay_RenderCommandArray_Get((Clay_RenderCommandArray *)renderCommands,
                                  current_index);
  Clay_RenderCommand *next =
      Clay_RenderCommandArray_Get((Clay_RenderCommandArray *)renderCommands,
                                  next_index);

  if (!current || !next) {
    return false;
  }

  return next->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE &&
         next->id == current->id;
}

void Clay_Raylib_Render(Clay_RenderCommandArray renderCommands, Font *fonts) {
  cc__rounded_image_frame++;
  cc__rounded_image_cache_sweep();

  int skip_index = -1;
  for (int j = 0; j < renderCommands.length; j++) {
    if (j == skip_index) {
      continue;
    }

    Clay_RenderCommand *renderCommand =
        Clay_RenderCommandArray_Get(&renderCommands, j);
    Clay_BoundingBox boundingBox = {
        renderCommand->boundingBox.x, renderCommand->boundingBox.y,
        renderCommand->boundingBox.width, renderCommand->boundingBox.height};
    switch (renderCommand->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextRenderData *textData = &renderCommand->renderData.text;
      Font fontToUse = fonts[textData->fontId];

      int strlen = textData->stringContents.length + 1;

      if (strlen > temp_render_buffer_len) {
        // Grow the temp buffer if we need a larger string
        if (temp_render_buffer)
          free(temp_render_buffer);
        temp_render_buffer = (char *)malloc(strlen);
        temp_render_buffer_len = strlen;
      }

      // Raylib uses standard C strings so isn't compatible with cheap slices,
      // we need to clone the string to append null terminator
      memcpy(temp_render_buffer, textData->stringContents.chars,
             textData->stringContents.length);
      temp_render_buffer[textData->stringContents.length] = '\0';
      DrawTextEx(fontToUse, temp_render_buffer,
                 (Vector2){boundingBox.x, boundingBox.y},
                 (float)textData->fontSize, (float)textData->letterSpacing,
                 CLAY_COLOR_TO_RAYLIB_COLOR(textData->textColor));

      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
      Clay_ImageRenderData *imgData = &renderCommand->renderData.image;
      CC_ImageRef *ref = (CC_ImageRef *)imgData->imageData;
      if (!ref || !ref->texture)
        break;
      Texture2D imageTexture = *ref->texture;
      Clay_Color tintColor = imgData->backgroundColor;
      if (tintColor.r == 0 && tintColor.g == 0 && tintColor.b == 0 &&
          tintColor.a == 0) {
        tintColor = (Clay_Color){255, 255, 255, 255};
      }

      /* Compute src/dst rectangles per scale mode. */
      float texW = (float)imageTexture.width;
      float texH = (float)imageTexture.height;
      float boxW = boundingBox.width;
      float boxH = boundingBox.height;
      Rectangle srcRect = {0, 0, texW, texH};
      Rectangle dstRect = {boundingBox.x, boundingBox.y, boxW, boxH};
      if (texW > 0 && texH > 0 && boxW > 0 && boxH > 0) {
        float texAspect = texW / texH;
        float boxAspect = boxW / boxH;
        switch (ref->scale) {
        case CC_IMAGE_FIT: {
          if (texAspect > boxAspect) {
            /* Image wider than box → fit width, letterbox top/bottom. */
            float newH = boxW / texAspect;
            dstRect =
                (Rectangle){boundingBox.x, boundingBox.y + (boxH - newH) * 0.5f,
                            boxW, newH};
          } else {
            /* Image taller than box → fit height, pillarbox sides. */
            float newW = boxH * texAspect;
            dstRect = (Rectangle){boundingBox.x + (boxW - newW) * 0.5f,
                                  boundingBox.y, newW, boxH};
          }
          break;
        }
        case CC_IMAGE_CROP: {
          if (texAspect > boxAspect) {
            /* Image wider than box → crop sides. */
            float effW = texH * boxAspect;
            srcRect = (Rectangle){(texW - effW) * 0.5f, 0, effW, texH};
          } else {
            /* Image taller than box → crop top/bottom. */
            float effH = texW / boxAspect;
            srcRect = (Rectangle){0, (texH - effH) * 0.5f, texW, effH};
          }
          break;
        }
        case CC_IMAGE_FILL:
        default:
          break;
        }
      }

      /* Rounded path: render through a cached RT with a rounded-rect alpha
       * mask. */
      if (imgData->cornerRadius.topLeft > 0) {
        int w = (int)roundf(boundingBox.width);
        int h = (int)roundf(boundingBox.height);
        if (w > 0 && h > 0) {
          CC_RoundedImageCacheEntry *e =
              cc__rounded_image_cache_get(renderCommand->id, w, h);
          if (e) {
            float shorter = (w < h) ? (float)w : (float)h;
            float roundness = (imgData->cornerRadius.topLeft * 2.0f) / shorter;
            if (roundness > 1.0f)
              roundness = 1.0f;

            BeginTextureMode(e->rt);
            ClearBackground(BLANK);
            DrawRectangleRounded((Rectangle){0, 0, (float)w, (float)h},
                                 roundness, 16, WHITE);
            BeginBlendMode(BLEND_MULTIPLIED);
            DrawTexturePro(imageTexture, srcRect,
                           (Rectangle){0, 0, (float)w, (float)h}, (Vector2){0},
                           0, CLAY_COLOR_TO_RAYLIB_COLOR(tintColor));
            EndBlendMode();
            EndTextureMode();

            /* GL RTs are Y-flipped — negative src height un-flips. */
            Rectangle rtSrc = {0, 0, (float)w, -(float)h};
            DrawTexturePro(e->rt.texture, rtSrc, dstRect, (Vector2){0}, 0,
                           WHITE);
            break;
          }
        }
      }

      /* Fast path: no corner radius, direct draw. */
      DrawTexturePro(imageTexture, srcRect, dstRect, (Vector2){0}, 0,
                     CLAY_COLOR_TO_RAYLIB_COLOR(tintColor));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
      BeginScissorMode((int)roundf(boundingBox.x), (int)roundf(boundingBox.y),
                       (int)roundf(boundingBox.width),
                       (int)roundf(boundingBox.height));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
      EndScissorMode();
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START: {
      SetColorOverlay(CLAY_COLOR_TO_RAYLIB_COLOR(
          renderCommand->renderData.overlayColor.color));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END: {
      DisableColorOverlay();
    }
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;
      if (config->cornerRadius.topLeft > 0) {
        float radius = (config->cornerRadius.topLeft * 2) /
                       (float)((boundingBox.width > boundingBox.height)
                                   ? boundingBox.height
                                   : boundingBox.width);
        DrawRectangleRounded(
            (Rectangle){boundingBox.x, boundingBox.y, boundingBox.width,
                        boundingBox.height},
            radius, 8, CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
      } else {
        DrawRectangle(boundingBox.x, boundingBox.y, boundingBox.width,
                      boundingBox.height,
                      CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
      }
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
      Clay_BorderRenderData *config = &renderCommand->renderData.border;
      // Left border
      if (config->width.left > 0) {
        DrawRectangleV(
            (Vector2){boundingBox.x,
                      boundingBox.y + config->cornerRadius.topLeft},
            (Vector2){config->width.left, boundingBox.height -
                                              config->cornerRadius.topLeft -
                                              config->cornerRadius.bottomLeft},
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      // Right border
      if (config->width.right > 0) {
        DrawRectangleV(
            (Vector2){boundingBox.x + boundingBox.width - config->width.right,
                      boundingBox.y + config->cornerRadius.topRight},
            (Vector2){config->width.right,
                      boundingBox.height - config->cornerRadius.topRight -
                          config->cornerRadius.bottomRight},
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      // Top border
      if (config->width.top > 0) {
        DrawRectangleV((Vector2){boundingBox.x + config->cornerRadius.topLeft,
                                 boundingBox.y},
                       (Vector2){boundingBox.width -
                                     config->cornerRadius.topLeft -
                                     config->cornerRadius.topRight,
                                 (int)config->width.top},
                       CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      // Bottom border
      if (config->width.bottom > 0) {
        DrawRectangleV(
            (Vector2){boundingBox.x + config->cornerRadius.bottomLeft,
                      boundingBox.y + boundingBox.height -
                          config->width.bottom},
            (Vector2){boundingBox.width - config->cornerRadius.bottomLeft -
                          config->cornerRadius.bottomRight,
                      (int)config->width.bottom},
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      if (config->cornerRadius.topLeft > 0) {
        DrawRing(
            (Vector2){roundf(boundingBox.x + config->cornerRadius.topLeft),
                      roundf(boundingBox.y + config->cornerRadius.topLeft)},
            roundf(config->cornerRadius.topLeft - config->width.top),
            config->cornerRadius.topLeft, 180, 270, 10,
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      if (config->cornerRadius.topRight > 0) {
        DrawRing(
            (Vector2){roundf(boundingBox.x + boundingBox.width -
                             config->cornerRadius.topRight),
                      roundf(boundingBox.y + config->cornerRadius.topRight)},
            roundf(config->cornerRadius.topRight - config->width.top),
            config->cornerRadius.topRight, 270, 360, 10,
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      if (config->cornerRadius.bottomLeft > 0) {
        DrawRing(
            (Vector2){roundf(boundingBox.x + config->cornerRadius.bottomLeft),
                      roundf(boundingBox.y + boundingBox.height -
                             config->cornerRadius.bottomLeft)},
            roundf(config->cornerRadius.bottomLeft - config->width.bottom),
            config->cornerRadius.bottomLeft, 90, 180, 10,
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      if (config->cornerRadius.bottomRight > 0) {
        DrawRing(
            (Vector2){roundf(boundingBox.x + boundingBox.width -
                             config->cornerRadius.bottomRight),
                      roundf(boundingBox.y + boundingBox.height -
                             config->cornerRadius.bottomRight)},
            roundf(config->cornerRadius.bottomRight - config->width.bottom),
            config->cornerRadius.bottomRight, 0.1, 90, 10,
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
      Clay_CustomRenderData *config = &renderCommand->renderData.custom;
      CustomLayoutElement *data = (CustomLayoutElement *)config->customData;
      if (!data)
        continue;

      CC_DrawSlotFwd *slot = NULL;
      if (cc__is_ccompose_draw_slot(renderCommand, &slot)) {
        if (cc__next_command_is_same_id_background(&renderCommands, j)) {
          Clay_RenderCommand *bg = Clay_RenderCommandArray_Get(&renderCommands, j + 1);
          if (bg) {
            Clay_RectangleRenderData *bg_cfg = &bg->renderData.rectangle;
            Clay_BoundingBox bg_bb = bg->boundingBox;
            if (bg_cfg->cornerRadius.topLeft > 0) {
              float radius = (bg_cfg->cornerRadius.topLeft * 2) /
                             (float)((bg_bb.width > bg_bb.height)
                                         ? bg_bb.height
                                         : bg_bb.width);
              DrawRectangleRounded(
                  (Rectangle){bg_bb.x, bg_bb.y, bg_bb.width, bg_bb.height},
                  radius, 8,
                  CLAY_COLOR_TO_RAYLIB_COLOR(bg_cfg->backgroundColor));
            } else {
              DrawRectangle(bg_bb.x, bg_bb.y, bg_bb.width, bg_bb.height,
                            CLAY_COLOR_TO_RAYLIB_COLOR(bg_cfg->backgroundColor));
            }
            skip_index = j + 1;
          }
        }

        Clay_BoundingBox bb = renderCommand->boundingBox;
        BeginScissorMode((int)bb.x, (int)bb.y, (int)bb.width, (int)bb.height);
        if (slot->fn)
          slot->fn(bb, slot->user);
        EndScissorMode();
        break;
      }

      CustomLayoutElement *customElement = (CustomLayoutElement *)data;

      switch (customElement->type) {
      case CUSTOM_LAYOUT_ELEMENT_TYPE_3D_MODEL: {
        Clay_BoundingBox rootBox = renderCommands.internalArray[0].boundingBox;
        float scaleValue = CLAY__MIN(CLAY__MIN(1, 768 / rootBox.height) *
                                         CLAY__MAX(1, rootBox.width / 1024),
                                     1.5f);
        Ray positionRay = GetScreenToWorldPointWithZDistance(
            (Vector2){renderCommand->boundingBox.x +
                          renderCommand->boundingBox.width / 2,
                      renderCommand->boundingBox.y +
                          (renderCommand->boundingBox.height / 2) + 20},
            Raylib_camera, (int)roundf(rootBox.width),
            (int)roundf(rootBox.height), 140);
        BeginMode3D(Raylib_camera);
        DrawModel(customElement->customData.model.model, positionRay.position,
                  customElement->customData.model.scale * scaleValue,
                  WHITE); // Draw 3d model with texture
        EndMode3D();
        break;
      }
      default:
        break;
      }
      break;
    }
    default: {
      printf("Error: unhandled render command.");
      exit(1);
    }
    }
  }
}
