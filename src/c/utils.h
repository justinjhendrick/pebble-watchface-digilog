#include <pebble.h>

static GPoint cartesian_from_polar_trigangle(GPoint center, int radius, int trigangle) {
  GPoint ret = {
    .x = (int16_t)(sin_lookup(trigangle) * radius / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(trigangle) * radius / TRIG_MAX_RATIO) + center.y,
  };
  return ret;
}

static GPoint cartesian_from_polar(GPoint center, int radius, int angle_deg) {
  return cartesian_from_polar_trigangle(center, radius, DEG_TO_TRIGANGLE(angle_deg));
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

static int deg_from_mins(int mins) {
  return mins * 360 / 60;
}

static uint32_t trigangle_from_mins(int mins) {
  return mins * TRIG_MAX_ANGLE / 60;
}

static void format_hour(char* buffer, int buffer_len, struct tm* now, bool force_12h) {
  int hour = now->tm_hour;
  if (force_12h || !clock_is_24h_style()) {
    hour = now->tm_hour % 12;
    if (hour == 0) {
      hour = 12;
    }
  }
  snprintf(buffer, buffer_len, "%d", hour);
}

static void format_day_of_week(char* buffer, int buffer_len, struct tm* now) {
  strftime(buffer, buffer_len, "%a", now);
}

static void format_day_th(char* buffer, int buffer_len, struct tm* now) {
  if (now->tm_mday / 10 == 1) {
    strftime(buffer, buffer_len, "%eth", now);
  } else if (now->tm_mday % 10 == 1) {
    strftime(buffer, buffer_len, "%est", now);
  } else if (now->tm_mday % 10 == 2) {
    strftime(buffer, buffer_len, "%end", now);
  } else if (now->tm_mday % 10 == 3) {
    strftime(buffer, buffer_len, "%erd", now);
  } else {
    strftime(buffer, buffer_len, "%eth", now);
  }
}

static void format_day(char* buffer, int buffer_len, struct tm* now) {
  strftime(buffer, buffer_len, "%e", now);
}

static void format_short_month(char* buffer, int buffer_len, struct tm* now) {
  strftime(buffer, buffer_len, "%b", now);
}

static GRect vcenter(GRect bbox) {
  return GRect(bbox.origin.x + 1, bbox.origin.y - 2, bbox.size.w, bbox.size.h);
}