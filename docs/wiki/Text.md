# Text

`Text` emits a text leaf element. It is not a scoped block.

## Forms

```c
Text("literal", .fontSize = 14, .textColor = Color(255,255,255,255));

const char *name = get_name();
TextN(name, strlen(name), .fontSize = 14, .textColor = Color(220,220,220,255));
```

- `Text(...)` requires a string literal.
- `TextN(chars, len, ...)` is for runtime strings/slices.

## Style fields

Arguments map directly to `Clay_TextElementConfig`:

- `.textColor`
- `.fontId`
- `.fontSize`
- `.letterSpacing`
- `.lineHeight`
- `.wrapMode` (`CLAY_TEXT_WRAP_WORDS`, `CLAY_TEXT_WRAP_NEWLINES`, `CLAY_TEXT_WRAP_NONE`)
- `.textAlignment` (`CLAY_TEXT_ALIGN_LEFT`, `_CENTER`, `_RIGHT`)
- `.userData`

## Fonts

Use `CC_LoadFont()` after `CC_Init()` and pass the returned id as `.fontId`.
Or use `CC_LoadGlobalFont()` to set a default font for all text that omits
`.fontId`.

```c
int FONT_TITLE = CC_LoadFont("resources/Roboto-Bold.ttf", 42);
if (FONT_TITLE < 0) FONT_TITLE = 0;

Text("Dashboard",
     .fontId = FONT_TITLE,
     .fontSize = 42,
     .textColor = Color(255,255,255,255));
```

When `.fontId` is omitted (or `0`), `Text`/`TextN` uses the current global
font id from `CC_LoadGlobalFont()` (defaults to `0` if unset).
