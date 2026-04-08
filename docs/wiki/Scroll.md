# Scroll / Clip

Scrolling and clipping use `.clip = (Clay_ClipElementConfig){ ... }`.

```c
Column("List",
       .layout = { .sizing = { Grow(), Fixed(300) } },
       .clip = { .vertical = true, .childOffset = { 0, state.scroll_y } }) {
    ...
}
```

## Fields

- `.horizontal`: clip/scroll horizontally
- `.vertical`: clip/scroll vertically
- `.childOffset`: child position offset (commonly used as scroll position)

ccompose does not own scroll state. Keep offset in app state and update it from input.
