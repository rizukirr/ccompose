#define MUSLIMTIFY_NAVIGATION_IMPLEMENTATION
#define MUSLIMTIFY_DASHBOARD_CONTENT_IMPLEMENTATION
#include "../../../include/ccompose.h"
#include "components/dashboard_content.h"
#include "components/navigation.h"
#include "themes/colors.h"
#include "themes/fonts.h"
#include <stdbool.h>

void App(void) {
  int LOGO_FONT = CC_LoadFont(FONT_MANROPE_BOLD, 48);
  int BOLD_FONT = CC_LoadFont(FONT_INTER_BOLD, 48);

  NavigationLoadImage();

  while (CC_Running()) {
    CC_Begin();

    Row("Container",
        .layout = {.sizing = {.height = Grow(), .width = Grow()}}) {
      SideNavigation(LOGO_FONT, BOLD_FONT);
      PrayerCard(LOGO_FONT, BOLD_FONT);
    }

    CC_End();
  }

  NavigationUnloadImage();
}

int main(void) {
  CC_SetWindow(960, 640, "Muslimtify");
  CC_SetBackground(COLOR_BG);
  CC_Init();
  CC_LoadGlobalFont(FONT_INTER_REGULAR, 48);

  App();

  CC_Shutdown();

  return 0;
}
