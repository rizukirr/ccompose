#ifndef MUSLIMTIFY_DASHBOARD_CONTENT_H
#define MUSLIMTIFY_DASHBOARD_CONTENT_H

#include <stdint.h>
void DashboardContent(void);

#ifdef MUSLIMTIFY_DASHBOARD_CONTENT_IMPLEMENTATION

#include "../../../../include/ccompose.h"
#include "../themes/colors.h"

static void PrayerCard(int16_t manropeFontId, int16_t boldFontId) {
  Column("PrayerCard",
         .layout = {.sizing = {.height = Fixed(400), .width = Grow()},
                    .padding = PadAll(48),
                    .childGap = 16},
         .backgroundColor = COLOR_PRIMARY) {
    Text("UPCOMING PRAYER", .textColor = COLOR_ON_PRIMARY, .fontSize = 12);
    Text("Duhr", .textColor = COLOR_ON_PRIMARY, .fontSize = 60,
         .fontId = manropeFontId);
    Text("in 45 minutes", .textColor = COLOR_ON_PRIMARY, .fontSize = 20);
    VSpacer();
    Row("PrayerCardButtons") {
      Column("startAt") {
        Text("Starts at", .textColor = COLOR_ON_PRIMARY, .fontSize = 12);
        Text("12:00 AM", .textColor = COLOR_ON_PRIMARY, .fontSize = 30,
             .fontId = boldFontId);
      }
      Box("Divider",
          .layout = {.sizing = {.width = Fixed(1), .height = Fixed(48)}});
    }
  }
}

#endif

#endif
