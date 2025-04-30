#include <pebble.h>

static GPoint cartesian_from_polar(GPoint center, int radius, int angle_deg) {
  GPoint ret = {
    .x = (int16_t)(sin_lookup(DEG_TO_TRIGANGLE(angle_deg)) * radius / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(DEG_TO_TRIGANGLE(angle_deg)) * radius / TRIG_MAX_RATIO) + center.y,
  };
  return ret;
}

static GRect rect_from_midpoint(GPoint midpoint, GSize size) {
  GRect ret;
  ret.origin.x = midpoint.x - size.w / 2;
  ret.origin.y = midpoint.y - size.h / 2;
  ret.size = size;
  return ret;
}

static int min(int a, int b) {
  if (a < b) {
    return a;
  }
  return b;
}

static int max(int a, int b) {
  if (a > b) {
    return a;
  }
  return b;
}

static void fast_forward_time(struct tm* now) {
  now->tm_min = now->tm_sec;           /* Minutes. [0-59] */
  now->tm_hour = now->tm_sec % 24;     /* Hours.  [0-23] */
  now->tm_mday = now->tm_sec % 31 + 1; /* Day. [1-31] */
  now->tm_mon = now->tm_sec % 12;      /* Month. [0-11] */
  now->tm_wday = now->tm_sec % 7;      /* Day of week. [0-6] */
}