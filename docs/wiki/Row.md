# Row

`Row("id", ...) { ... }` lays out children left-to-right.

```c
Row("Toolbar",
    .layout = {
        .sizing = { Grow(), Fixed(40) },
        .padding = PadAll(8),
        .childGap = 10,
        .childAlignment = { .y = AlignMiddle() }
    }) {
    Text("File");
    Text("Edit");
}
```

Equivalent to:

```c
Element(CLAY_LEFT_TO_RIGHT, "Toolbar", ...)
```

## Example: two-pane layout

```c
Row("Main", .layout = { .sizing = { Grow(), Grow() } }) {
    Column("Nav", .layout = { .sizing = { Fixed(240), Grow() } }) { ... }
    Column("Content", .layout = { .sizing = { Grow(), Grow() } }) { ... }
}
```
