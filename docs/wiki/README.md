# ccompose wiki

ccompose is a thin wrapper over Clay with a Compose-like block syntax in plain C.
The public API is in `include/ccompose.h`.

## Quick start

```c
#include "ccompose.h"

int main(void) {
    CC_SetWindow(960, 640, "demo");
    CC_Init();

    int font_title = CC_LoadFont("resources/Roboto-Bold.ttf", 32);
    if (font_title < 0) font_title = 0; // default raylib font

    while (CC_Running()) {
        CC_Begin();
        Column("Root",
               .layout = { .sizing = { Grow(), Grow() }, .padding = PadAll(16), .childGap = 8 }) {
            Text("Hello ccompose", .fontId = font_title, .fontSize = 32,
                 .textColor = Color(255, 255, 255, 255));
        }
        CC_End();
    }

    CC_Shutdown();
    return 0;
}
```

## Core model

- Containers are scoped blocks: `Column("id", ...) { ... }`, `Row("id", ...) { ... }`, `Box("id", ...) { ... }`.
- `Element(direction, "id", ...)` is the generic form.
- Text is a leaf statement: `Text("literal", ...)` or `TextN(chars, len, ...)`.
- Container arguments are `Clay_ElementDeclaration` designated initializers (no `CC_Props` abstraction).

## API docs

- [Lifecycle](Lifecycle.md) - `CC_Init`, frame loop, fonts, headless notes
- [Element](Element.md) - generic container contract and IDs
- [Column](Column.md), [Row](Row.md)
- [Text](Text.md)

## Declaration field reference

- [Layout](Layout.md) - `.layout`
- [Decor](Decor.md) - `.backgroundColor`, `.overlayColor`, `.cornerRadius`, `.image`
- [Border](Border.md) - `.border`
- [Scroll](Scroll.md) - `.clip`
- [Floating](Floating.md) - `.floating`
- [Aspect](Aspect.md) - `.aspectRatio`

## Important notes

- IDs must be string literals (`"Root"`, `""`), not runtime strings.
- `CC_LoadFont()` must be called after `CC_Init()`.
- In headless mode (`CCOMPOSE_BACKEND_RAYLIB=OFF`), `CC_LoadFont()` returns `-1` and `CC_Running()` always returns `true`.
