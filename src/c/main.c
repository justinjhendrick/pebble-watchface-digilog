#include <pebble.h>
#include "utils.h"

#define DEBUG_TIME (false)
#define DEBUG_BBOX (false)
#define BUFFER_LEN (40)

#define COL_SUN           (GColorYellow)
#define COL_MORNING       (GColorMelon)
#define COL_DAY           (GColorVividCerulean)
#define COL_EVENING       (GColorChromeYellow)
#define COL_NIGHT         (GColorCobaltBlue)

#define COL_STROKE        (GColorBlack)
#define COL_FACE          (GColorWhite)
#define COL_TIME_TEXT     (GColorBlack)
#define COL_DATE_BG       (GColorBlack)
#define COL_DATE_TEXT     (GColorWhite)

static Window* s_window;
static Layer* s_layer;
static GPath* s_arc;
static int s_sunrise_minute_since_midnight = 60 * 6;
static int s_sunset_minute_since_midnight = 60 * (6 + 12);
static GFont s_font_lg = NULL;

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

static void draw_arc(GContext* ctx, GPoint center, int begin, int arc, int inner, int outer) {
  int half_points = ARC_POINTS.num_points / 2;
  int step = arc / (half_points - 1);
  for (int i = 0; i < half_points; i++) {
    int a = begin + step * i;
    ARC_POINTS.points[i] = cartesian_from_polar(center, outer, a);
  }
  for (int j = 0; j < half_points; j++) {
    int a = begin + arc - step * j;
    ARC_POINTS.points[half_points + j] = cartesian_from_polar(center, inner, a);
  }
  gpath_draw_filled(ctx, s_arc);
  gpath_draw_outline(ctx, s_arc);
}

static void draw_sun(GContext* ctx, struct tm* now, GPoint center, int radius, int sun_radius) {
  int total_mins = 24 * 60;
  int current_mins = now->tm_hour * 60 + now->tm_min;
  int hour_angle = current_mins * TRIG_MAX_ANGLE / total_mins + DEG_TO_TRIGANGLE(180);
  GPoint mpoint = cartesian_from_polar(center, radius, hour_angle);
  graphics_context_set_fill_color(ctx, COL_SUN);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  graphics_fill_circle(ctx, mpoint, sun_radius);
  graphics_draw_circle(ctx, mpoint, sun_radius);
}

static void draw_time(GContext* ctx, struct tm* now, GPoint center, int radius) {
  graphics_context_set_text_color(ctx, COL_TIME_TEXT);
  GRect full = rect_from_midpoint(center, GSize(radius * 2, radius * 2));
  int x = full.origin.x;
  int y = full.origin.y;
  int w = full.size.w / 2;
  int h = full.size.h / 2;
  GRect tl = GRect(x    , y,     w, h);
  GRect tr = GRect(x + w, y,     w, h);
  GRect bl = GRect(x    , y + h, w, h);
  GRect br = GRect(x + w, y + h, w, h);
  debug_bbox(ctx, tl);
  debug_bbox(ctx, tr);
  debug_bbox(ctx, bl);
  debug_bbox(ctx, br);

  char t[BUFFER_LEN];

  // hours on top
  int hours_shift_up = 7;
  int hour = now->tm_hour;
  if (clock_is_24h_style()) {
    snprintf(t, BUFFER_LEN, "%d", hour / 10);
    draw_text(ctx, t, s_font_lg, tl, GTextAlignmentRight, hours_shift_up);
    snprintf(t, BUFFER_LEN, "%d", hour % 10);
    draw_text(ctx, t, s_font_lg, tr, GTextAlignmentLeft, hours_shift_up);
  } else {
    hour = hour % 12;
    if (hour == 0) {
      hour = 12;
    }
    if (hour / 10 != 0) {
      snprintf(t, BUFFER_LEN, "%d", hour / 10);
      draw_text(ctx, t, s_font_lg, tl, GTextAlignmentRight, hours_shift_up);
    }
    snprintf(t, BUFFER_LEN, "%d", hour % 10);
    draw_text(ctx, t, s_font_lg, tr, GTextAlignmentLeft, hours_shift_up);
  }

  // minutes on bottom
  int minutes_shift_up = 15;
  int minute = now->tm_min;
  snprintf(t, BUFFER_LEN, "%d", minute / 10);
  draw_text(ctx, t, s_font_lg, bl, GTextAlignmentRight, minutes_shift_up);
  snprintf(t, BUFFER_LEN, "%d", minute % 10);
  draw_text(ctx, t, s_font_lg, br, GTextAlignmentLeft, minutes_shift_up);
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
  draw_arc(
    ctx,
    center,
    flip + sunrise_start * TRIG_MAX_ANGLE / minutes_per_day,
    twilight_minutes * TRIG_MAX_ANGLE / minutes_per_day,
    inner_radius,
    outer_radius
  );

  // day
  graphics_context_set_fill_color(ctx, COL_DAY);
  draw_arc(
    ctx,
    center,
    flip + sunrise_end * TRIG_MAX_ANGLE / minutes_per_day,
    (sunset_start - sunrise_end) * TRIG_MAX_ANGLE / minutes_per_day,
    inner_radius,
    outer_radius
  );

  // evening
  graphics_context_set_fill_color(ctx, COL_EVENING);
  draw_arc(ctx,
    center,
    flip + sunset_start * TRIG_MAX_ANGLE / minutes_per_day,
    twilight_minutes * TRIG_MAX_ANGLE / minutes_per_day,
    inner_radius,
    outer_radius
  );

  // night
  graphics_context_set_fill_color(ctx, COL_NIGHT);
  draw_arc(
    ctx,
    center,
    flip + sunset_end * TRIG_MAX_ANGLE / minutes_per_day,
    (minutes_per_day - sunset_end + sunrise_start) * TRIG_MAX_ANGLE / minutes_per_day,
    inner_radius,
    outer_radius
  );
}

static void draw_date(GContext* ctx, GRect bounds, int date_height, struct tm* now) {
  GRect date_bbox = GRect(
    bounds.origin.x,
    bounds.origin.y + bounds.size.h - date_height,
    bounds.size.w,
    date_height
  );
  graphics_context_set_fill_color(ctx, COL_DATE_BG);
  graphics_fill_rect(ctx, date_bbox, 0, GCornerNone);
  char t[BUFFER_LEN];
  strftime(t, BUFFER_LEN, "%a %d %b", now);
  graphics_context_set_text_color(ctx, COL_DATE_TEXT);
  draw_text(ctx, t, fonts_get_system_font(FONT_KEY_GOTHIC_28), date_bbox, GTextAlignmentCenter, 6);
}

static void update_layer(Layer* layer, GContext* ctx) {
  time_t temp = time(NULL);
  struct tm* now = localtime(&temp);
  if (DEBUG_TIME) {
    fast_forward_time(now);
  }

  int date_height = 28;
  GRect bounds = layer_get_bounds(layer);

  GRect main = GRect(
    bounds.origin.x,
    bounds.origin.y,
    bounds.size.w,
    bounds.size.h - date_height
  );
  int vcr = min(main.size.h, main.size.w) / 2;
  GPoint center = grect_center_point(&main);
  int sun_radius = main.size.w * 3 / 40;
  int between = vcr - sun_radius * 2;
  draw_sunlight_background(ctx, center, main.size.h);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, COL_STROKE);
  graphics_context_set_fill_color(ctx, COL_FACE);
  graphics_fill_circle(ctx, center, between);
  graphics_draw_circle(ctx, center, between);

  draw_sun(ctx, now, center, between + sun_radius + 1, sun_radius);
  draw_time(ctx, now, center, between * 18 / 20);

  draw_date(ctx, bounds, date_height, now);
}

static int minutes_since_midnight(time_t ts) {
  struct tm* local = localtime(&ts);
  return local->tm_hour * 60 + local->tm_min;
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple* t;

  t = dict_find(iter, MESSAGE_KEY_sunrise);
  if (t && t->value->int32 != 0) { s_sunrise_minute_since_midnight = minutes_since_midnight(t->value->int32); }

  t = dict_find(iter, MESSAGE_KEY_sunset);
  if (t && t->value->int32 != 0) { s_sunset_minute_since_midnight = minutes_since_midnight(t->value->int32); }

  layer_mark_dirty(s_layer);
}

static void maybe_request_sun() {
  static time_t s_last_request_sent = 0;
  time_t now = time(NULL);

  if (now >= s_last_request_sent + 24 * 60 * 60) {
    // send an empty message. that means "give me sun!"
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    dict_write_uint8(iter, 0, 0);
    app_message_outbox_send();
    s_last_request_sent = now;
  }
}

static void window_load(Window* window) {
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, update_layer);
  layer_add_child(window_layer, s_layer);
}

static void window_unload(Window* window) {
  layer_destroy(s_layer);
}

static void tick_handler(struct tm* now, TimeUnits units_changed) {
  layer_mark_dirty(s_layer);
  maybe_request_sun();
}

static void init(void) {
  s_font_lg = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_68));
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
  fonts_unload_custom_font(s_font_lg);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
