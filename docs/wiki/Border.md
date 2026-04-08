# Border

Borders are configured with `.border = (Clay_BorderElementConfig){ ... }`.

```c
Column("Panel",
       .border = {
           .color = Color(80, 80, 90, 255),
           .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 }
       }) {
    ...
}
```

## Border width fields

- `.left`, `.right`, `.top`, `.bottom`
- `.betweenChildren` (divider lines between children based on layout direction)

Example:

```c
Column("List",
       .layout = { .childGap = 0 },
       .border = {
           .color = Color(50, 50, 50, 255),
           .width = { .betweenChildren = 1 }
       }) { ... }
```
