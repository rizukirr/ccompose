#include "../include/ccompose.h"

static const CC_Color COLOR_BG = {248, 250, 250, 100};

int main(void) {
  CC_SetWindow(960, 640, "Muslimtify");
  CC_SetBackground(COLOR_BG);
  CC_Init();

  while (CC_Running()) {
    CC_Begin();

    CC_End();
  }

  CC_Shutdown();

  return 0;
}
