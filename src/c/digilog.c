#include <pebble.h>
#include "utils.h"

#define DEBUG_TIME (true)
#define DEG_PER_MIN (360 / 60)
#define BUFFER_LEN (100)

static Window* s_window;
static Layer* s_layer;
static char s_buffer[BUFFER_LEN];
static GPath* s_arc;

static const GPathInfo ARC_POINTS = {
  .num_points = 20,
  .points = (GPoint []) {
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}
  }
};

static void draw_arc(GContext* ctx, GPoint center, int begin_deg, int arc_degs, int inner, int outer) {
  int half_points = ARC_POINTS.num_points / 2;
  int step = arc_degs / (half_points - 1);
  int end_deg = begin_deg + arc_degs;
  for (int i = 0; i < half_points; i++) {
    int a = begin_deg + step * i;
    if (i + 1 == half_points) {
      a = end_deg;
    }
    ARC_POINTS.points[i] = cartesian_from_polar(GPoint(0, 0), outer, a);
  }
  for (int j = 0; j < half_points; j++) {
    int a = begin_deg + arc_degs - step * j;
    if (j + 1 == half_points) {
      a = begin_deg;
    }
    ARC_POINTS.points[half_points + j] = cartesian_from_polar(GPoint(0, 0), inner, a);
  }
  gpath_move_to(s_arc, center);
  gpath_draw_outline(ctx, s_arc);
  gpath_draw_filled(ctx, s_arc);
}

static void draw_hour_text(GContext* ctx, struct tm* now, GPoint center, int radius, int size) {
  // int minutes_per_hour_hand_rev = 60 * 24;
  // int minutes_elapsed = now->tm_hour * 60 + now->tm_min;
  // int hour_angle_deg = 360 * minutes_elapsed / minutes_per_hour_hand_rev + 180;
  int hour_angle_deg = 360 * now->tm_hour / 24 + 180;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GSize bbox_size = GSize(size, size);
  GPoint bbox_mpoint = cartesian_from_polar(center, radius, hour_angle_deg);
  GRect bbox = rect_from_midpoint(bbox_mpoint, bbox_size);
  graphics_context_set_fill_color(ctx, GColorYellow);
  graphics_context_set_text_color(ctx, GColorBlack);
  //graphics_fill_circle(ctx, bbox_mpoint, size / 2);
  snprintf(s_buffer, BUFFER_LEN, "%d", now->tm_hour);
  int arc_degs = 360 / 24;
  draw_arc(ctx, center, hour_angle_deg - arc_degs / 2, arc_degs, radius - size / 2, radius + size / 2);
  graphics_draw_text(ctx, s_buffer, font, bbox, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_min_text(GContext* ctx, struct tm* now, GPoint center, int radius, int size) {
  int min_angle_deg = now->tm_min * DEG_PER_MIN;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GSize bbox_size = GSize(size, size);
  GPoint bbox_mpoint = cartesian_from_polar(center, radius, min_angle_deg);
  GRect bbox = rect_from_midpoint(bbox_mpoint, bbox_size);
  graphics_context_set_text_color(ctx, GColorBlack);
  snprintf(s_buffer, BUFFER_LEN, "%d", now->tm_min);
  graphics_draw_text(ctx, s_buffer, font, bbox, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_ticks(GContext* ctx, int now, GPoint center, int radius, int tick_length, int from, int to, int by) {
  for (int i = from; i < to; i += by) {
    int angle_deg = i * 360 / (to - from);
    graphics_draw_line(
      ctx, 
      cartesian_from_polar(center, radius, angle_deg),
      cartesian_from_polar(center, radius + tick_length, angle_deg)
    );
  }
}

static void draw_sunlight_background(GContext* ctx, GRect bounds) {
  GRect day = GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h * 4 / 10);
  GRect twilight = GRect(bounds.origin.x, bounds.origin.y + day.size.h, bounds.size.w, bounds.size.h * 2 / 10);
  GRect night = GRect(bounds.origin.x, twilight.origin.y + twilight.size.h, bounds.size.w, bounds.size.h * 4 / 10);
  graphics_context_set_fill_color(ctx, GColorVividCerulean);
  graphics_fill_rect(ctx, day, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_rect(ctx, night, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorChromeYellow);
  graphics_fill_rect(ctx, twilight, 0, GCornerNone);

  // graphics_context_set_stroke_width(ctx, 3);
  // graphics_context_set_stroke_color(ctx, GColorBlack);
  // graphics_draw_rect(ctx, twilight);
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
  int size = 34;
  int between = vcr - size + 4;
  draw_sunlight_background(ctx, bounds);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, between);

  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  draw_ticks(ctx, now->tm_hour, center, between, 100, 1, 49, 2);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  draw_ticks(ctx, now->tm_min, center, between, -8, 0, 60, 5);

  draw_hour_text(ctx, now, center, between + size / 2 + 1, size);
  draw_min_text(ctx, now, center, between - size / 2 - 1, size);
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