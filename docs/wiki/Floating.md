# Floating

Floating overlays are configured with `.floating = (Clay_FloatingElementConfig){ ... }`.

```c
Box("Tooltip",
    .layout = { .padding = PadAll(6) },
    .backgroundColor = Color(40, 40, 40, 240),
    .cornerRadius = RadiusAll(4),
    .floating = {
        .attachTo = CLAY_ATTACH_TO_PARENT,
        .offset = { 0, 24 },
        .zIndex = 100
    }) {
    Text("Hello", .fontSize = 12, .textColor = Color(255,255,255,255));
}
```

## Common fields

- `.attachTo`: `CLAY_ATTACH_TO_PARENT`, `CLAY_ATTACH_TO_ELEMENT_WITH_ID`, `CLAY_ATTACH_TO_ROOT`
- `.parentId`: used with `CLAY_ATTACH_TO_ELEMENT_WITH_ID`
- `.attachPoints`: anchor points for element and parent
- `.offset`, `.expand`, `.zIndex`
- `.pointerCaptureMode`
- `.clipTo`

Floating is active when `.attachTo` is not `CLAY_ATTACH_TO_NONE`.
