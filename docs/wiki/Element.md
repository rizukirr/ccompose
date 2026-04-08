# Element

`Element` is the generic container primitive. `Column`, `Row`, and `Box` are wrappers over it.

## Signature

```c
Element(direction, "id", .field = value, ...) {
    /* children */
}
```

- `direction`: `CLAY_TOP_TO_BOTTOM` or `CLAY_LEFT_TO_RIGHT`
- `"id"`: string literal ID (use `""` for anonymous)
- remaining args: `Clay_ElementDeclaration` fields

## Available declaration fields

You can set any `Clay_ElementDeclaration` field directly:

- `.layout`
- `.backgroundColor`, `.overlayColor`, `.cornerRadius`
- `.image`, `.custom`
- `.floating`, `.clip`, `.aspectRatio`, `.border`
- `.userData`

Example:

```c
Element(CLAY_TOP_TO_BOTTOM, "Card",
        .layout = { .sizing = { Grow(), Fit() }, .padding = PadAll(12), .childGap = 8 },
        .backgroundColor = Color(24, 24, 28, 255),
        .cornerRadius = RadiusAll(10),
        .border = { .color = Color(60, 60, 70, 255), .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 } }) {
    Text("Title", .fontSize = 16, .textColor = Color(255, 255, 255, 255));
}
```

## ID guidance

Use stable non-empty IDs when you need hover/pointer lookups, scroll persistence, or floating anchors. Use `""` for throwaway containers.
