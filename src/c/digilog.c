#include <pebble.h>
#include "utils.h"

#define DEBUG_TIME (false)
#define DEBUG_BT_OK (true)
#define BUFFER_LEN (100)
#define COL_BG                   COLOR_FALLBACK(GColorBlack,          GColorBlack)
#define COL_MIN                  COLOR_FALLBACK(GColorDarkGreen,      GColorWhite)
#define COL_FACE                 COLOR_FALLBACK(GColorWhite,          GColorBlack)
#define COL_STROKE               COLOR_FALLBACK(GColorBlack,          GColorWhite)
#define COL_SUN                  COLOR_FALLBACK(GColorYellow,         GColorWhite)
#define COL_MORNING              COLOR_FALLBACK(GColorMelon,          GColorBlack)
#define COL_DAY                  COLOR_FALLBACK(GColorVividCerulean,  GColorBlack)
#define COL_EVENING              COLOR_FALLBACK(GColorChromeYellow,   GColorBlack)
#define COL_NIGHT                COLOR_FALLBACK(GColorCobaltBlue,     GColorBlack)
#define COL_MONTH_TEXT           COLOR_FALLBACK(GColorWhite,          GColorWhite)
#define COL_HOUR_TEXT            COLOR_FALLBACK(GColorBlack,          GColorBlack)
#define COL_ERR_TEXT             COLOR_FALLBACK(GColorChromeYellow,   GColorWhite)
#define COL_WDAY_TEXT            COLOR_FALLBACK(GColorBlack,          GColorWhite)
#define COL_SELECTED_WDAY        COLOR_FALLBACK(GColorWhite,          GColorWhite)
#define COL_SELECTED_WDAY_TEXT   COLOR_FALLBACK(GColorBlack,          GColorBlack)

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];
static GPath* s_arc;
static GPath* s_arrow;

static const GPathInfo ARC_POINTS = {
  .num_points = 40,
  .points = (GPoint []) {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
  }
};

static const GPathInfo ARROW_POINTS = {
  .num_points = 4,
  .points = (GPoint []) {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
};

static void draw_arc_trigangle(GContext* ctx, GPoint center, int begin, int arc, int inner, int outer) {
  int half_points = ARC_POINTS.num_points / 2;
  int step = arc / (half_points - 1);
  for (int i = 0; i < half_points; i++) {
    int a = begin + step * i;
    ARC_POINTS.points[i] = cartesian_from_polar_trigangle(center, outer, a);
  }
  for (int j = 0; j < half_points; j++) {
    int a = begin + arc - step * j;
    ARC_POINTS.points[half_points + j] = cartesian_from_polar_trigangle(center, inner, a);
  }
  gpath_draw_filled(ctx, s_arc);
  gpath_draw_outline(ctx, s_arc);
}

static void draw_arc(GContext* ctx, GPoint center, int begin_deg, int arc_degs, int inner, int outer) {
  draw_arc_trigangle(ctx, center, DEG_TO_TRIGANGLE(begin_deg), DEG_TO_TRIGANGLE(arc_degs), inner, outer);
}

static void draw_arrow(GContext* ctx, GPoint center, int angle_deg, int inner, int outer) {
  ARROW_POINTS.points[0] = cartesian_from_polar(center, inner + 3, angle_deg);
  ARROW_POINTS.points[1] = cartesian_from_polar(center, inner, angle_deg + 7);
  ARROW_POINTS.points[2] = cartesian_from_polar(center, outer, angle_deg);
  ARROW_POINTS.points[3] = cartesian_from_polar(center, inner, angle_deg - 7);
  gpath_draw_filled(ctx, s_arrow);
  gpath_draw_outline(ctx, s_arrow);
}

static void draw_hour(GContext* ctx, struct tm* now, GPoint center, int radius, int sun_radius) {
  int hour_angle_deg = 360 * now->tm_hour / 24 + 180;
  GSize bbox_size = GSize(sun_radius * 2, sun_radius * 2);
  GPoint bbox_mpoint = cartesian_from_polar(center, radius, hour_angle_deg);
  GRect bbox = rect_from_midpoint(bbox_mpoint, bbox_size);
  graphics_context_set_fill_color(ctx, COL_SUN);
  graphics_context_set_text_color(ctx, COL_HOUR_TEXT);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  graphics_fill_circle(ctx, bbox_mpoint, sun_radius);
  graphics_draw_circle(ctx, bbox_mpoint, sun_radius);
  format_hour(s_buffer, BUFFER_LEN, now, true);
  draw_text_midalign(ctx, s_buffer, bbox, GTextAlignmentCenter, true);
}

static void draw_minute(GContext* ctx, struct tm* now, GPoint center, int radius, int outer_radius, int sun_radius) {
  int min_angle_deg = deg_from_mins(now->tm_min);
  GSize bbox_size = GSize(sun_radius * 2, sun_radius * 2);
  GPoint bbox_mpoint = cartesian_from_polar(center, radius, min_angle_deg);
  GRect bbox = rect_from_midpoint(bbox_mpoint, bbox_size);
  graphics_context_set_text_color(ctx, COL_MIN);
  if (now->tm_min < 10) {
    snprintf(s_buffer, BUFFER_LEN, "0%d", now->tm_min);
  } else {
    snprintf(s_buffer, BUFFER_LEN, "%d", now->tm_min);
  }
  draw_text_midalign(ctx, s_buffer, bbox, GTextAlignmentCenter, true);
  graphics_context_set_fill_color(ctx, COL_MIN);
  graphics_context_set_stroke_color(ctx, COL_MIN);
  graphics_context_set_stroke_width(ctx, 1);
  draw_arrow(ctx, center, min_angle_deg, outer_radius - sun_radius, outer_radius - 1);
}

static void draw_ticks(GContext* ctx, GPoint center, int radius, int tick_length, int from, int to, int by) {
  for (int i = from; i < to; i += by) {
    int angle_deg = i * 360 / (to - from);
    graphics_draw_line(
      ctx, 
      cartesian_from_polar(center, radius, angle_deg),
      cartesian_from_polar(center, radius + tick_length, angle_deg)
    );
  }
}

static void draw_sunlight_background(GContext* ctx, GPoint center, int outer_radius) {
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  int inner_radius = 0;

  // morning
  graphics_context_set_fill_color(ctx, COL_MORNING);
  draw_arc(ctx, center, deg_from_mins(42), deg_from_mins(6), inner_radius, outer_radius);

  // day
  graphics_context_set_fill_color(ctx, COL_DAY);
  draw_arc(ctx, center, deg_from_mins(48), deg_from_mins(24), inner_radius, outer_radius);

  // evening
  graphics_context_set_fill_color(ctx, COL_EVENING);
  draw_arc(ctx, center, deg_from_mins(12), deg_from_mins(6), inner_radius, outer_radius);

  // night
  graphics_context_set_fill_color(ctx, COL_NIGHT);
  draw_arc(ctx, center, deg_from_mins(18), deg_from_mins(24), inner_radius, outer_radius);
}

static void draw_week(GContext* ctx, struct tm* now, GRect bounds, GPoint center, int vcr) {
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  graphics_context_set_stroke_width(ctx, 3);
  static const char WEEK[8] = "SMTWTFS";
  uint32_t trigangle_start = trigangle_from_mins(51);
  uint32_t trigangle_step = trigangle_from_mins(18) / 7;
  int inner_radius = vcr + 2;
  int outer_radius = bounds.size.h - vcr;
  int width = outer_radius - inner_radius;
  for (int d = 0; d < 7; d++) {
    if (now->tm_wday == d) {
      graphics_context_set_fill_color(ctx, COL_SELECTED_WDAY);
      graphics_context_set_text_color(ctx, COL_SELECTED_WDAY_TEXT);
    } else {
      graphics_context_set_fill_color(ctx, COL_DAY);
      graphics_context_set_text_color(ctx, COL_WDAY_TEXT);
    }
    uint32_t trigangle_arc_begin = trigangle_start + trigangle_step * d;
    draw_arc_trigangle(ctx, center, trigangle_arc_begin, trigangle_step, inner_radius, outer_radius);
    uint32_t trigangle_arc_midpoint = trigangle_arc_begin + trigangle_step / 2;
    GPoint text_center = cartesian_from_polar_trigangle(center, inner_radius + width / 2, trigangle_arc_midpoint);
    GRect bbox = rect_from_midpoint(text_center, GSize(width, width));
    s_buffer[0] = WEEK[d];
    s_buffer[1] = '\0';
    draw_text_midalign(ctx, s_buffer, bbox, GTextAlignmentCenter, true);
  }
}

static void draw_month(GContext* ctx, struct tm* now, GRect bounds, int vcr) {
  int height = (bounds.size.h - 2 * vcr) * 3 / 2;
  GRect l_bbox = GRect(
    bounds.origin.x + 2,
    bounds.origin.y + bounds.size.h - height - 1,
    bounds.size.w / 2 - 2,
    height - 4
  );
  GRect r_bbox = GRect(
    bounds.origin.x + bounds.size.w / 2,
    bounds.origin.y + bounds.size.h - height - 1,
    bounds.size.w / 2 - 2,
    height - 4
  );
  graphics_context_set_text_color(ctx, COL_MONTH_TEXT);
  format_day(s_buffer, BUFFER_LEN, now);
  draw_text_botalign(ctx, s_buffer, r_bbox, GTextAlignmentRight, false);
  format_short_month(s_buffer, BUFFER_LEN, now);
  draw_text_botalign(ctx, s_buffer, l_bbox, GTextAlignmentLeft, false);
}

static void draw_bluetooth(GContext* ctx, GRect bounds) {
  bool bt_ok = DEBUG_BT_OK && connection_service_peek_pebble_app_connection();
  GRect bbox = GRect(bounds.origin.x + 2, bounds.origin.y + 2, 8, 16);
  if (bounds.size.w > 180) {
    bbox = GRect(bounds.origin.x + 2, bounds.origin.y + 2, 12, 24);
  }
  int w = bbox.size.w / 2;
  int h = bbox.size.h / 4;
  int top = bbox.origin.y;
  int upper = bbox.origin.y + h;
  int lower = bbox.origin.y + 3 * h;
  int bottom = bbox.origin.y + 4 * h;
  int left = bbox.origin.x;
  int mid = bbox.origin.x + w;
  int right = bbox.origin.x + 2 * w;
  if (!bt_ok) {
    graphics_context_set_stroke_width(ctx, 3);
    graphics_context_set_stroke_color(ctx, COL_ERR_TEXT);
    graphics_draw_line(ctx, GPoint(left, lower), GPoint(right, upper));
    graphics_draw_line(ctx, GPoint(right, lower), GPoint(left, upper));
  } else {
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, COL_WDAY_TEXT);
    graphics_draw_line(ctx, GPoint(left, lower), GPoint(right, upper));
    graphics_draw_line(ctx, GPoint(right, lower), GPoint(left, upper));
  }
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, COL_WDAY_TEXT);
  graphics_draw_line(ctx, GPoint(right, upper), GPoint(mid, top));
  graphics_draw_line(ctx, GPoint(mid, top), GPoint(mid, bottom));
  graphics_draw_line(ctx, GPoint(mid, bottom), GPoint(right, lower));
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);
  if (DEBUG_TIME) {
    fast_forward_time(now);
  }

  GRect bounds = layer_get_bounds(layer);
  int vcr = min(bounds.size.h, bounds.size.w) / 2;
  GPoint center = GPoint(vcr, bounds.origin.y + bounds.size.h - vcr);
  int sun_radius = bounds.size.w / 12 + 1;
  int between = vcr - sun_radius * 2;
  draw_sunlight_background(ctx, center, bounds.size.h);
  if (PBL_IF_RECT_ELSE(true, false)) {
    draw_month(ctx, now, bounds, vcr);
    draw_week(ctx, now, bounds, center, vcr);
    draw_bluetooth(ctx, bounds);
  }
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  graphics_context_set_fill_color(ctx, COL_FACE);
  graphics_fill_circle(ctx, center, between);
  graphics_draw_circle(ctx, center, between);

  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  draw_ticks(ctx, center, between, -8, 0, 60, 5);

  int hour_radius = between + sun_radius;
  draw_hour(ctx, now, center, hour_radius, sun_radius);
  int min_radius = between - 2 * sun_radius;
  draw_minute(ctx, now, center, min_radius, between, sun_radius);
}

static void window_load(Window* window) {
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(s_window, COL_BG);
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update_layer);
  layer_add_child(window_layer, s_layer);
}

static void window_unload(Window* window) {
  layer_destroy(s_layer);
}

static void tick_handler(struct tm* now, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  s_arc = gpath_create(&ARC_POINTS);
  s_arrow = gpath_create(&ARROW_POINTS);
  tick_timer_service_subscribe(DEBUG_TIME ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}