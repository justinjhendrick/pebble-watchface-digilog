#include <pebble.h>
#include "utils.h"
#include "fonts.h"

#define DEBUG_TIME (false)
#define DEBUG_BBOX (false)
#define BUFFER_LEN (40)
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
#define COL_ERR_TEXT             COLOR_FALLBACK(GColorBlack,          GColorWhite)
#define COL_WDAY_TEXT            COLOR_FALLBACK(GColorBlack,          GColorWhite)
#define COL_SELECTED_WDAY        COLOR_FALLBACK(GColorWhite,          GColorWhite)
#define COL_SELECTED_WDAY_TEXT   COLOR_FALLBACK(GColorBlack,          GColorBlack)

static Window* s_window;
static Layer* s_layer;
static GPath* s_arc;
static int s_sunrise_minute_since_midnight = 60 * 6;
static int s_sunset_minute_since_midnight = 60 * (6 + 12);

static const GPathInfo ARC_POINTS = {
  .num_points = 40,
  .points = (GPoint []) {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
  }
};

static void debug_bbox(GContext* ctx, GRect bbox) {
  if (!DEBUG_BBOX) {
    return;
  }
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_rect(ctx, bbox);
}

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

static void draw_sun(GContext* ctx, struct tm* now, GPoint center, int radius, int sun_radius) {
  int hour_angle_deg = 360 * now->tm_hour / 24 + 180;
  GPoint mpoint = cartesian_from_polar(center, radius, hour_angle_deg);
  graphics_context_set_fill_color(ctx, COL_SUN);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  graphics_fill_circle(ctx, mpoint, sun_radius);
  graphics_draw_circle(ctx, mpoint, sun_radius);
}

static void draw_time(GContext* ctx, struct tm* now, GPoint center, int radius) {
  graphics_context_set_text_color(ctx, COL_HOUR_TEXT);
  GRect full = rect_from_midpoint(center, GSize(radius * 2, radius * 2));
  int x = full.origin.x;
  int y = full.origin.y;
  int w = full.size.w;
  int h = full.size.h;
  int big_h = h * 17 / 40;
  GRect hour = GRect(x, y, w, big_h);
  GRect hour_left = GRect(x, y, w / 2, hour.size.h);
  GRect hour_right = GRect(x + w / 2, y, w / 2, hour.size.h);
  y += hour.size.h;
  GRect date = GRect(x, y, w, h - 2 * big_h);
  y += date.size.h;
  GRect minute = GRect(x, y, w, big_h);
  GFont big = get_font(hour.size.h);
  debug_bbox(ctx, hour_left);
  debug_bbox(ctx, hour_right);
  debug_bbox(ctx, date);
  debug_bbox(ctx, minute);

  char hour_tens[BUFFER_LEN];
  char hour_ones[BUFFER_LEN];
  char t[BUFFER_LEN];

  // hours on top
  format_hour(hour_tens, hour_ones, BUFFER_LEN, now);
  draw_text(ctx, hour_tens, big, hour_left, GTextAlignmentRight, hour.size.h * 3 / 20);
  draw_text(ctx, hour_ones, big, hour_right, GTextAlignmentLeft, hour.size.h * 3 / 20);

  // date in the middle
  GFont small = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  strftime(t, BUFFER_LEN, "%a %d %b", now);
  draw_text(ctx, t, small, date, GTextAlignmentCenter, date.size.h / 5);

  // minutes on bottom
  strftime(t, BUFFER_LEN, "%M", now);
  draw_text(ctx, t, big, minute, GTextAlignmentCenter, minute.size.h * 3 / 20);
}

static void draw_sunlight_background(GContext* ctx, GPoint center, int outer_radius) {
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  int inner_radius = 0;
  int twilight_minutes = 60;
  int minutes_per_day = 60 * 24;
  int sunrise_end = s_sunrise_minute_since_midnight;
  int sunrise_start = sunrise_end - twilight_minutes;
  int sunset_start = s_sunset_minute_since_midnight;
  int sunset_end = sunset_start + twilight_minutes;
  int flip = TRIG_MAX_ANGLE / 2;

  // morning
  graphics_context_set_fill_color(ctx, COL_MORNING);
  draw_arc_trigangle(
    ctx,
    center,
    flip + sunrise_start * TRIG_MAX_ANGLE / minutes_per_day,
    twilight_minutes * TRIG_MAX_ANGLE / minutes_per_day,
    inner_radius,
    outer_radius
  );

  // day
  graphics_context_set_fill_color(ctx, COL_DAY);
  draw_arc_trigangle(
    ctx,
    center,
    flip + sunrise_end * TRIG_MAX_ANGLE / minutes_per_day,
    (sunset_start - sunrise_end) * TRIG_MAX_ANGLE / minutes_per_day,
    inner_radius,
    outer_radius
  );

  // evening
  graphics_context_set_fill_color(ctx, COL_EVENING);
  draw_arc_trigangle(ctx,
    center,
    flip + sunset_start * TRIG_MAX_ANGLE / minutes_per_day,
    twilight_minutes * TRIG_MAX_ANGLE / minutes_per_day,
    inner_radius,
    outer_radius
  );

  // night
  graphics_context_set_fill_color(ctx, COL_NIGHT);
  draw_arc_trigangle(
    ctx,
    center,
    flip + sunset_end * TRIG_MAX_ANGLE / minutes_per_day,
    (minutes_per_day - sunset_end + sunrise_start) * TRIG_MAX_ANGLE / minutes_per_day,
    inner_radius,
    outer_radius
  );
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);
  if (DEBUG_TIME) {
    fast_forward_time(now);
  }

  GRect bounds = layer_get_bounds(layer);
  int vcr = min(bounds.size.h, bounds.size.w) / 2;
  GPoint center = grect_center_point(&bounds);
  int sun_radius = bounds.size.w * 1 / 20;
  int between = vcr - sun_radius * 2;
  draw_sunlight_background(ctx, center, bounds.size.h);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  graphics_context_set_fill_color(ctx, COL_FACE);
  graphics_fill_circle(ctx, center, between);
  graphics_draw_circle(ctx, center, between);

  draw_sun(ctx, now, center, between + sun_radius, sun_radius);
  int time_radius = between * 19 / 20;
  draw_time(ctx, now, center, time_radius);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple* t;

  t = dict_find(iter, MESSAGE_KEY_sunriseMinuteSinceMidnight);
  if (t) { s_sunrise_minute_since_midnight = t->value->int32; }

  t = dict_find(iter, MESSAGE_KEY_sunsetMinuteSinceMidnight);
  if (t) { s_sunset_minute_since_midnight = t->value->int32; }
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
  layer_mark_dirty(s_layer);
}

static void init(void) {
  init_fonts();
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  app_message_open(/*inbox_size*/64, /*outbox_size=*/256);
  app_message_register_inbox_received(inbox_received_handler);
  s_arc = gpath_create(&ARC_POINTS);
  tick_timer_service_subscribe(DEBUG_TIME ? SECOND_UNIT : MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  window_destroy(s_window);
  deinit_fonts();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
