/* Headless smoke test — compiles with -DCCOMPOSE_NO_BACKEND.
 *
 * Exercises: CC_Init -> CC_Begin -> Column/Row/Text/ColumnId -> CC_End,
 * then inspects the returned render command list to make sure Clay
 * actually emitted a rectangle (for the background-colored Column) and
 * a text command (for the Text call).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "ccompose.h"

static bool saw_error = false;

static void smoke_error_handler(CC_ErrorData err) {
    saw_error = true;
    fprintf(stderr, "unexpected clay error: %.*s\n",
            (int)err.errorText.length, err.errorText.chars);
}

int main(void) {
    CC_SetViewport(400.0f, 300.0f);
    CC_SetErrorHandler(smoke_error_handler);
    CC_Init();

    CC_Begin();
    Column("Root",
           .layout = { .sizing   = { Grow(), Grow() },
                       .padding  = PadAll(8),
                       .childGap = 4 },
           .backgroundColor = Color(24, 24, 24, 255),
           .cornerRadius    = RadiusAll(6)) {
        Row("Header", .layout = { .childGap = 4 }) {
            Text("Hello",
                 .textColor = Color(255, 255, 255, 255),
                 .fontSize  = 14);
            Text("World",
                 .textColor = Color(180, 180, 180, 255),
                 .fontSize  = 14);
        }
        /* Anonymous footer container — empty ID. */
        Row("", .layout = { .childGap = 0 }) {
            Text("(footer)",
                 .textColor = Color(120, 120, 120, 255),
                 .fontSize  = 10);
        }
        /* Box used as a decoration container — single child. */
        Box("Badge",
            .layout = { .sizing = { Fixed(20), Fixed(20) } },
            .backgroundColor = Color(255, 0, 0, 255),
            .cornerRadius    = RadiusAll(999)) {
        }
    }
    CC_RenderCommandArray commands = CC_End();

    int rect_count = 0;
    int text_count = 0;
    for (int32_t i = 0; i < commands.length; ++i) {
        CC_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
        if (!cmd) continue;
        if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) rect_count++;
        if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT)      text_count++;
    }

    assert(!saw_error && "clay reported an error during layout");
    assert(rect_count >= 1 && "expected at least one RECTANGLE render command");
    assert(text_count == 3 && "expected exactly three TEXT render commands");

    CC_Shutdown();

    printf("smoke_test ok: %d rectangles, %d text commands\n",
           rect_count, text_count);
    return 0;
}
