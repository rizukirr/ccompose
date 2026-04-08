# Layout

Layout is configured through `.layout = (Clay_LayoutConfig){ ... }` on containers.

## Common fields

```c
Column("Root",
       .layout = {
           .sizing = { Grow(), Grow() },
           .padding = PadAll(12),
           .childGap = 8,
           .childAlignment = { .x = AlignStart(), .y = AlignTop() }
       }) { ... }
```

- `.sizing.width`, `.sizing.height`: use `Fit()`, `Grow()`, `Fixed(px)`, `Percent(0.0f..1.0f)`
- `.padding`: `PadAll(n)` or `Pad(left, right, top, bottom)`
- `.childGap`: spacing between children on the main axis
- `.childAlignment`: cross-axis alignment (`.x`, `.y`)

## Sizing helpers

```c
Fit() Grow() Fixed(240) Percent(0.5f)
```

Use `Percent` only in `[0.0f, 1.0f]`.
