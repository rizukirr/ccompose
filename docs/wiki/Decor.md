# Decor

Visual surface fields are set directly on the element declaration.

## Fields

- `.backgroundColor`: base fill/tint color
- `.overlayColor`: color overlay applied to element and descendants
- `.cornerRadius`: rounded corners (`RadiusAll(n)` helper)
- `.image`: image payload (`.imageData` pointer for renderer)

```c
Box("Avatar",
    .layout = { .sizing = { Fixed(64), Fixed(64) } },
    .backgroundColor = Color(255, 255, 255, 255),
    .cornerRadius = RadiusAll(999),
    .image = { .imageData = avatar_texture }) {
}
```

For per-corner radii:

```c
.cornerRadius = (Clay_CornerRadius){ .topLeft = 8, .topRight = 8, .bottomLeft = 0, .bottomRight = 0 }
```
