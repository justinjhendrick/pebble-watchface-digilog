#include "fonts.h"

// BRITTLE HACK
// Assuming fonts are assigned contiguously, starting from 1
// 0 is the default icon.
#define FIRST_RESOURCE_ID (1)
// Make sure this matches package.json
#if PBL_DISPLAY_WIDTH >= 260
#define NUM_FONTS (15)
#elif PBL_DISPLAY_WIDTH >= 200
#define NUM_FONTS (11)
#else
#define NUM_FONTS (6)
#endif

#define DEBUG (false)

typedef struct FontInfo {
  GFont font;
  int height_px;
} FontInfo;

static FontInfo s_fonts[NUM_FONTS];
static const char* digits = "0123456789";
static const GRect huge = GRect(0, 0, 1000, 1000);

void init_fonts() {
  for (int resource_id = FIRST_RESOURCE_ID; resource_id <= NUM_FONTS; resource_id++) {
    int idx = resource_id - FIRST_RESOURCE_ID;
    FontInfo* font_info = s_fonts + idx;
    font_info->font = fonts_load_custom_font(resource_get_handle(resource_id));
    GSize content_size = graphics_text_layout_get_content_size(
      digits,
      font_info->font,
      huge,
      GTextOverflowModeFill,
      GTextAlignmentCenter
    );
    font_info->height_px = content_size.h;
    if (DEBUG) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "init_fonts: idx=%d, resource_id=%d, actual_size=%d", idx, resource_id, content_size.h);
    }
  }
}

GFont get_font(int desired_size) {
  for (int idx = 1; idx < NUM_FONTS; idx++) {
    // ASSUMPTION: fonts are listed in package.json in increasing order
    if (s_fonts[idx].height_px > desired_size) {
      if (DEBUG) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "get_font: desired_size=%d, actual_size=%d", desired_size, s_fonts[idx-1].height_px);
      }
      // Largest font that isn't above the desired size
      // default to smallest font when desired_size is too small
      return s_fonts[idx - 1].font;
    }
  }
  if (DEBUG) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "get_font: desired_size=%d, actual_size=%d", desired_size, s_fonts[NUM_FONTS-1].height_px);
  }
  // default to largest font when desired_size is too big
  return s_fonts[NUM_FONTS - 1].font;
}

void deinit_fonts() {
  for (int i = 0; i < NUM_FONTS; i++) {
    if (s_fonts[i].font == NULL) {
      continue;
    }
    fonts_unload_custom_font(s_fonts[i].font);
  }
}
