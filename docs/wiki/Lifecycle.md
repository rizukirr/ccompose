# Lifecycle

ccompose can own the full Clay + raylib lifecycle for you.

## Minimal loop

```c
CC_SetWindow(1280, 720, "app");              // optional, before init
CC_SetWindowFlags(FLAG_WINDOW_RESIZABLE);      // optional, before init
CC_SetBackground(Color(16, 16, 18, 255));      // optional
CC_SetErrorHandler(my_error_handler);           // optional, before init

CC_Init();
while (CC_Running()) {
    CC_Begin();
    // build UI with Column/Row/Box/Text
    CC_End();
}
CC_Shutdown();
```

## Font loading

- `int CC_LoadFont(const char *path, int base_size)` returns `fontId`.
- Use that id in `Text(..., .fontId = id, .fontSize = n)`.
- Font slot `0` is always the default raylib font.
- `int CC_LoadGlobalFont(const char *path, int base_size)` loads a font and
  sets it as the default for `Text()` / `TextN()` calls that omit `.fontId`.

```c
int FONT_UI = CC_LoadFont("resources/Inter-Regular.ttf", 16);
if (FONT_UI < 0) FONT_UI = 0;

Text("Status", .fontId = FONT_UI, .fontSize = 16,
     .textColor = Color(220, 220, 220, 255));
```

```c
int FONT_GLOBAL = CC_LoadGlobalFont("resources/Inter-Regular.ttf", 16);
if (FONT_GLOBAL < 0) FONT_GLOBAL = 0;
int FONT_MONO = CC_LoadFont("resources/RobotoMono-Medium.ttf", 14);

Text("Uses global font", .fontSize = 16, .textColor = Color(220, 220, 220, 255));
Text("Manual override", .fontId = FONT_MONO, .fontSize = 14, .textColor = Color(220, 220, 220, 255));
```

## Headless mode

Build with `-DCCOMPOSE_BACKEND_RAYLIB=OFF` for CI/tests:

- no window/rendering,
- `CC_LoadFont()` returns `-1`,
- use `CC_SetViewport(w, h)` to define layout size,
- `CC_Running()` always returns `true` (your code must break loops explicitly).
