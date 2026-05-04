#include <pebble.h>

// ============================================================================
// CONFIG
// ============================================================================
#define WEATHER_KEY_TEMP        0
#define WEATHER_KEY_DESC        1
#define CONFIG_KEY_MILITARY     2
#define CONFIG_KEY_FAHRENHEIT   3

#define PERSIST_KEY_MILITARY    0
#define PERSIST_KEY_FAHRENHEIT  1

#define TEMP_INVALID 999

// ============================================================================
// 🔥 FIXED PEPPLE TIME HIGH-CONTRAST COLORS
// ============================================================================
#ifdef PBL_COLOR
  #define COLOR_BG        GColorBlack
  #define COLOR_PRIMARY   GColorWhite
  #define COLOR_DIM       GColorLightGray
  #define COLOR_COMMENT   GColorBlueMoon
  #define COLOR_KEYWORD   GColorCyan
  #define COLOR_NUMBER    GColorGreen
#else
  #define COLOR_BG        GColorBlack
  #define COLOR_PRIMARY   GColorWhite
  #define COLOR_DIM       GColorLightGray
  #define COLOR_COMMENT   GColorLightGray
  #define COLOR_KEYWORD   GColorWhite
  #define COLOR_NUMBER    GColorWhite
#endif

// Layout
#define LINE_HEIGHT_SMALL 14
#define LINE_HEIGHT_MED   18
#define PADDING_X         4

// ============================================================================
// GLOBAL STATE
// ============================================================================
static Window *s_window;
static Layer  *s_canvas;

static struct {
  int hours, minutes;
  int day, month, year;
  int battery;
  bool charging;
  int steps;

  int temp;
  char desc[24];

  bool military;
  bool fahrenheit;
} s_state;

static GFont s_font_small;
static GFont s_font_medium;

// ============================================================================
// DRAW HELPERS
// ============================================================================

static void draw_line(GContext *ctx, const char *text, GFont font,
                      GColor color, int y) {
  graphics_context_set_text_color(ctx, color);

  graphics_draw_text(ctx, text, font,
    GRect(PADDING_X, y, 144 - (PADDING_X * 3), 30),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft,
    NULL);
}

static void draw_separator(GContext *ctx, int y, int width) {
  graphics_context_set_stroke_color(ctx, COLOR_DIM);
  for (int x = 2; x < width - 2; x += 5) {
    graphics_draw_line(ctx, GPoint(x, y), GPoint(x + 2, y));
  }
}

// ============================================================================
// RENDER SECTIONS
// ============================================================================

static int render_header(GContext *ctx, int y) {
  draw_line(ctx, "#include <pebble.h>", s_font_small, COLOR_PRIMARY, y);
  y += LINE_HEIGHT_SMALL;

  draw_line(ctx, "class Clock {", s_font_small, COLOR_KEYWORD, y);
  return y + LINE_HEIGHT_SMALL + 4;
}

static int render_time(GContext *ctx, int y) {
  int display_hours = s_state.military
    ? s_state.hours
    : (s_state.hours % 12 == 0 ? 12 : s_state.hours % 12);

  char line1[24];
  char line2[24];

  snprintf(line1, sizeof(line1), "int hr  = %02d;", display_hours);
  snprintf(line2, sizeof(line2), "int min = %02d;", s_state.minutes);

  y -= 2;

  draw_line(ctx, line1, s_font_medium, COLOR_NUMBER, y);
  y += LINE_HEIGHT_MED;

  draw_line(ctx, line2, s_font_medium, COLOR_NUMBER, y);
  return y + LINE_HEIGHT_MED;
}

static int render_date(GContext *ctx, int y) {
  static const char *months[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };

  const char *m = (s_state.month >= 1 && s_state.month <= 12)
    ? months[s_state.month - 1] : "???";

  char buf[32];
  snprintf(buf, sizeof(buf), "// %s %02d, %04d",
           m, s_state.day, s_state.year);

  draw_line(ctx, buf, s_font_medium, COLOR_COMMENT, y);
  return y + LINE_HEIGHT_MED;
}

static int render_weather(GContext *ctx, int y) {
  char buf[48];

  if (s_state.temp == TEMP_INVALID) {
    snprintf(buf, sizeof(buf), "#define WTHR %s", s_state.desc);
  } else {
    int t = s_state.fahrenheit
      ? (s_state.temp * 9 / 5 + 32)
      : s_state.temp;

    char unit = s_state.fahrenheit ? 'F' : 'C';

    snprintf(buf, sizeof(buf),
             "#define WTHR %d%c %s", t, unit, s_state.desc);
  }

  draw_line(ctx, buf, s_font_small, COLOR_NUMBER, y);
  return y + LINE_HEIGHT_SMALL;
}

static int render_battery(GContext *ctx, int y) {
  char buf[32];

  if (s_state.charging) {
    snprintf(buf, sizeof(buf),
             "#define BATT %d%% CHG", s_state.battery);
  } else {
    snprintf(buf, sizeof(buf),
             "#define BATT %d%%", s_state.battery);
  }

  draw_line(ctx, buf, s_font_small, COLOR_NUMBER, y);
  return y + LINE_HEIGHT_SMALL;
}

static int render_steps(GContext *ctx, int y) {
  char buf[32];
  snprintf(buf, sizeof(buf), "#define STEPS %d", s_state.steps);

  draw_line(ctx, buf, s_font_small, COLOR_NUMBER, y);
  return y + LINE_HEIGHT_SMALL;
}

static void render_footer(GContext *ctx, int y) {
  draw_line(ctx, "}; // end", s_font_small, COLOR_PRIMARY, y);
}

// ============================================================================
// MAIN DRAW
// ============================================================================

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, COLOR_BG);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int y = 2;

  y = render_header(ctx, y);

  draw_separator(ctx, y, bounds.size.w);
  y += 4;

  y = render_time(ctx, y);

  y += 4;
  draw_separator(ctx, y, bounds.size.w);
  y += 4;

  y = render_date(ctx, y);

  y += 4;

  y = render_weather(ctx, y);
  y = render_battery(ctx, y);
  y = render_steps(ctx, y);

  int footer_y = bounds.size.h - LINE_HEIGHT_SMALL - 2;
  render_footer(ctx, footer_y);
}

// ============================================================================
// SYSTEM (UNCHANGED)
// ============================================================================

static void update_steps(void) {
#if defined(PBL_HEALTH)
  s_state.steps = (int)health_service_sum_today(HealthMetricStepCount);
#endif
}

static void tick_handler(struct tm *t, TimeUnits units) {
  s_state.hours   = t->tm_hour;
  s_state.minutes = t->tm_min;
  s_state.day     = t->tm_mday;
  s_state.month   = t->tm_mon + 1;
  s_state.year    = t->tm_year + 1900;

  if (t->tm_min % 30 == 0) {
    update_steps();
  }

  layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
  s_state.battery  = state.charge_percent;
  s_state.charging = state.is_charging;
  layer_mark_dirty(s_canvas);
}

// ============================================================================
// APP / WINDOW (UNCHANGED LOGIC)
// ============================================================================

static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;

  if ((t = dict_find(iter, WEATHER_KEY_TEMP))) {
    s_state.temp = (int)t->value->int32;
  }

  if ((t = dict_find(iter, WEATHER_KEY_DESC))) {
    snprintf(s_state.desc, sizeof(s_state.desc), "%s",
             t->value->cstring);
  }

  if ((t = dict_find(iter, CONFIG_KEY_MILITARY))) {
    s_state.military = t->value->int32;
    persist_write_bool(PERSIST_KEY_MILITARY, s_state.military);
  }

  if ((t = dict_find(iter, CONFIG_KEY_FAHRENHEIT))) {
    s_state.fahrenheit = t->value->int32;
    persist_write_bool(PERSIST_KEY_FAHRENHEIT, s_state.fahrenheit);
  }

  layer_mark_dirty(s_canvas);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_font_small  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_font_medium = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  tick_handler(t, MINUTE_UNIT);
  update_steps();
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

static void init(void) {
  s_state.temp = TEMP_INVALID;

  s_state.military =
    persist_exists(PERSIST_KEY_MILITARY)
      ? persist_read_bool(PERSIST_KEY_MILITARY)
      : false;

  s_state.fahrenheit =
    persist_exists(PERSIST_KEY_FAHRENHEIT)
      ? persist_read_bool(PERSIST_KEY_FAHRENHEIT)
      : false;

  battery_state_service_subscribe(battery_handler);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 64);

  s_window = window_create();
  window_set_background_color(s_window, COLOR_BG);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  window_stack_push(s_window, true);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}