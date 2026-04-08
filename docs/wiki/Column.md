# Column

`Column("id", ...) { ... }` lays out children top-to-bottom.

```c
Column("Sidebar",
       .layout = { .sizing = { Fixed(220), Grow() }, .padding = PadAll(12), .childGap = 8 }) {
    Text("Home");
    Text("Settings");
}
```

Equivalent to:

```c
Element(CLAY_TOP_TO_BOTTOM, "Sidebar", ...)
```

## Common patterns

```c
// Fill screen and center children
Column("Root",
       .layout = {
           .sizing = { Grow(), Grow() },
           .childAlignment = { .x = AlignCenter(), .y = AlignMiddle() }
       }) { ... }

// Scrollable list
Column("List",
       .layout = { .sizing = { Grow(), Fixed(280) }, .childGap = 4 },
       .clip = { .vertical = true }) { ... }
```
