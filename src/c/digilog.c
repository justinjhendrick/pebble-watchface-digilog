#include <pebble.h>
#include "utils.h"

#define DEBUG_TIME (false)
#define DEG_PER_MIN (360 / 60)
#define BUFFER_LEN (100)

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];
static GPath* s_arc;

static const GPathInfo ARC_POINTS = {
  .num_points = 40,
  .points = (GPoint []) {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
  }
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

static void draw_hour_text(GContext* ctx, struct tm* now, GPoint center, int radius, int sun_radius) {
  int hour_angle_deg = 360 * now->tm_hour / 24 + 180;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GSize bbox_size = GSize(sun_radius * 2, sun_radius * 2);
  GPoint bbox_mpoint = cartesian_from_polar(center, radius, hour_angle_deg);
  GRect bbox = rect_from_midpoint(bbox_mpoint, bbox_size);
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, bbox_mpoint, sun_radius);
  graphics_draw_circle(ctx, bbox_mpoint, sun_radius);
  format_hour(s_buffer, BUFFER_LEN, now, true);
  graphics_draw_text(ctx, s_buffer, font, vcenter(bbox), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_min_text(GContext* ctx, struct tm* now, GPoint center, int radius, int outer_radius, int sun_radius) {
  int min_angle_deg = now->tm_min * DEG_PER_MIN;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GSize bbox_size = GSize(sun_radius * 2, sun_radius * 2);
  GPoint bbox_mpoint = cartesian_from_polar(center, radius, min_angle_deg);
  GRect bbox = rect_from_midpoint(bbox_mpoint, bbox_size);
  graphics_context_set_text_color(ctx, GColorBlack);
  if (now->tm_min < 10) {
    snprintf(s_buffer, BUFFER_LEN, "0%d", now->tm_min);
  } else {
    snprintf(s_buffer, BUFFER_LEN, "%d", now->tm_min);
  }
  graphics_context_set_fill_color(ctx, GColorDarkGreen);
  graphics_fill_circle(ctx, cartesian_from_polar(center, outer_radius - 5, min_angle_deg), 4);
  graphics_draw_text(ctx, s_buffer, font, vcenter(bbox), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
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
  graphics_context_set_stroke_color(ctx, GColorBlack);
  int inner_radius = 0;

  // morning
  graphics_context_set_fill_color(ctx, GColorMelon);
  draw_arc(ctx, center, deg_from_mins(42), deg_from_mins(6), inner_radius, outer_radius);

  // day
  graphics_context_set_fill_color(ctx, GColorVividCerulean);
  draw_arc(ctx, center, deg_from_mins(48), deg_from_mins(24), inner_radius, outer_radius);

  // evening
  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  draw_arc(ctx, center, deg_from_mins(12), deg_from_mins(6), inner_radius, outer_radius);

  // night
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  draw_arc(ctx, center, deg_from_mins(18), deg_from_mins(24), inner_radius, outer_radius);
}

static void draw_week(GContext* ctx, struct tm* now, GRect bounds, GPoint center, int vcr) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_text_color(ctx, GColorBlack);
  static const char WEEK[8] = "SMTWTFS";
  uint32_t trigangle_start = trigangle_from_mins(51);
  uint32_t trigangle_step = trigangle_from_mins(18) / 7;
  int inner_radius = vcr + 2;
  int outer_radius = bounds.size.h - vcr;
  int width = outer_radius - inner_radius;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  for (int d = 0; d < 7; d++) {
    if (now->tm_wday == d) {
      graphics_context_set_fill_color(ctx, GColorWhite);
    } else {
      graphics_context_set_fill_color(ctx, GColorVividCerulean);
    }
    uint32_t trigangle_arc_begin = trigangle_start + trigangle_step * d;
    draw_arc_trigangle(ctx, center, trigangle_arc_begin, trigangle_step, inner_radius, outer_radius);
    uint32_t trigangle_arc_midpoint = trigangle_arc_begin + trigangle_step / 2;
    GPoint text_center = cartesian_from_polar_trigangle(center, inner_radius + width / 2, trigangle_arc_midpoint);
    GRect bbox = rect_from_midpoint(text_center, GSize(width, width));
    s_buffer[0] = WEEK[d];
    s_buffer[1] = '\0';
    graphics_draw_text(ctx, s_buffer, font, vcenter(bbox), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

static void draw_month(GContext* ctx, struct tm* now, GPoint center, int outer_radius) {
  GRect full_bbox = rect_from_midpoint(center, GSize(outer_radius * 2, outer_radius * 2));
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  format_day_and_month(s_buffer, BUFFER_LEN, now);
  graphics_draw_text(ctx, s_buffer, font, vcenter(full_bbox), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
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
  int sun_radius = 17;
  int between = vcr - sun_radius * 2;
  draw_sunlight_background(ctx, center, bounds.size.h);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, between);
  graphics_draw_circle(ctx, center, between);

  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  draw_ticks(ctx, center, between, -8, 0, 60, 5);

  int hour_radius = between + sun_radius;
  draw_hour_text(ctx, now, center, hour_radius, sun_radius);
  int min_radius = between - sun_radius * 3 / 2;
  draw_min_text(ctx, now, center, min_radius, between, sun_radius);
  draw_month(ctx, now, center, min_radius - sun_radius);
  draw_week(ctx, now, bounds, center, vcr);
}

static void window_load(Window* window) {
  Layer* window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(s_window, GColorBlack);
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