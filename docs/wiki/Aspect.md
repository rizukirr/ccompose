# Aspect Ratio

Aspect ratio is configured with `.aspectRatio = (Clay_AspectRatioElementConfig){ ... }`.

```c
Box("Video",
    .layout = { .sizing = { Grow(), Fit() } },
    .aspectRatio = { .aspectRatio = 16.0f / 9.0f },
    .backgroundColor = Color(0, 0, 0, 255)) {
}
```

## Notes

- `aspectRatio` is width / height.
- Common values: `1.0f`, `16.0f/9.0f`, `4.0f/3.0f`, `9.0f/16.0f`.
- Pair with explicit width or height sizing so layout has a stable constraint.
