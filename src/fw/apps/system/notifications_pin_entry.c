/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "notifications_pin_entry.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "kernel/ui/system_icons.h"
#include "system/passert.h"

#include <stddef.h>

// Layout constants
#define PIN_BOX_WIDTH    28
#define PIN_BOX_HEIGHT   38
#define PIN_BOX_GAP       8
#define PIN_DIGIT_Y_RECT 60
#define PIN_DIGIT_Y_RUND 70
#define PIN_LABEL_Y_RECT 14
#define PIN_LABEL_Y_RUND 30

static void prv_update_proc(Layer *layer, GContext *ctx) {
  _Static_assert(offsetof(Window, layer) == 0,
                 "Window.layer must be the first member");
  _Static_assert(offsetof(NotifPinEntryData, window) == 0,
                 "NotifPinEntryData.window must be the first member");
  NotifPinEntryData *data = (NotifPinEntryData *)layer;

  const GRect bounds = layer->bounds;
  const int16_t x_margin = 5;
  const GEdgeInsets insets =
      PBL_IF_ROUND_ELSE(GEdgeInsets(ACTION_BAR_WIDTH + x_margin),
                        GEdgeInsets(0, ACTION_BAR_WIDTH + x_margin, 0, x_margin));
  const GRect content_rect = grect_inset(bounds, insets);

  // White background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &bounds);

  // Label
  graphics_context_set_text_color(ctx, GColorBlack);
  GRect title_rect = content_rect;
  title_rect.origin.y = PBL_IF_ROUND_ELSE(PIN_LABEL_Y_RUND, PIN_LABEL_Y_RECT);
  title_rect.size.h = 30;
  graphics_draw_text(ctx, data->label,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     title_rect,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Three digit boxes
  const int16_t total_w = NOTIF_PIN_LEN * PIN_BOX_WIDTH + (NOTIF_PIN_LEN - 1) * PIN_BOX_GAP;
  const int16_t start_x = content_rect.origin.x + (content_rect.size.w - total_w) / 2;
  const int16_t start_y = PBL_IF_ROUND_ELSE(PIN_DIGIT_Y_RUND, PIN_DIGIT_Y_RECT);

  for (int i = 0; i < NOTIF_PIN_LEN; i++) {
    const GRect box = GRect(start_x + i * (PIN_BOX_WIDTH + PIN_BOX_GAP),
                            start_y, PIN_BOX_WIDTH, PIN_BOX_HEIGHT);
    const bool is_current = (i == (int)data->current_pos);

    if (is_current) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, &box);
      graphics_context_set_text_color(ctx, GColorWhite);
    } else {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, &box);
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_rect(ctx, &box);
      graphics_context_set_text_color(ctx, GColorBlack);
    }

    char digit_str[2] = {'0' + data->digits[i], '\0'};
    GRect digit_rect = box;
    digit_rect.origin.y += 3;
    graphics_draw_text(ctx, digit_str,
                       fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK),
                       digit_rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Reset text colour to avoid leaking state
  graphics_context_set_text_color(ctx, GColorBlack);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, NotifPinEntryData *data) {
  data->digits[data->current_pos] = (data->digits[data->current_pos] + 1) % 10;
  layer_mark_dirty(&data->window.layer);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, NotifPinEntryData *data) {
  // Subtract 1 mod 10 by adding 9 (avoids unsigned underflow)
  data->digits[data->current_pos] = (data->digits[data->current_pos] + 9) % 10;
  layer_mark_dirty(&data->window.layer);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, NotifPinEntryData *data) {
  if (data->current_pos < NOTIF_PIN_LEN - 1) {
    data->current_pos++;
    layer_mark_dirty(&data->window.layer);
  } else {
    // Last digit confirmed — invoke callback
    data->pin_confirmed = true;
    if (data->callback) {
      data->callback(data->digits, data->callback_context);
    }
  }
}

static void prv_click_config_provider(NotifPinEntryData *data) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 80,
      (ClickHandler)prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 80,
      (ClickHandler)prv_down_click_handler);
  // Brief delay (25 ms) keeps the window responsive while giving the action-bar
  // check icon time to render visibly before the window transitions.
  window_multi_click_subscribe(BUTTON_ID_SELECT, 1, 2, 25, true,
      (ClickHandler)prv_select_click_handler);
}

static void prv_window_load(Window *window) {
  NotifPinEntryData *data = window_get_user_data(window);
  ActionBarLayer *action_bar = &data->action_bar;
  action_bar_layer_set_context(action_bar, data);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, &s_bar_icon_up_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, &s_bar_icon_down_bitmap);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_SELECT, &s_bar_icon_check_bitmap);
  action_bar_layer_add_to_window(action_bar, window);
  action_bar_layer_set_click_config_provider(action_bar,
      (ClickConfigProvider)prv_click_config_provider);
}

static void prv_window_unload(Window *window) {
  NotifPinEntryData *data = window_get_user_data(window);
  action_bar_layer_remove_from_window(&data->action_bar);
  action_bar_layer_deinit(&data->action_bar);
  if (data->on_unload) {
    data->on_unload(data);
  }
}

void notif_pin_entry_init(NotifPinEntryData *data, const char *label,
                          NotifPinEntryCallback callback, void *context) {
  *data = (NotifPinEntryData){
    .label = label,
    .callback = callback,
    .callback_context = context,
  };
  window_init(&data->window, WINDOW_NAME("PIN Entry"));
  window_set_user_data(&data->window, data);
  window_set_window_handlers(&data->window, &(WindowHandlers){
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  layer_set_update_proc(&data->window.layer, prv_update_proc);
  action_bar_layer_init(&data->action_bar);
}

void notif_pin_entry_deinit(NotifPinEntryData *data) {
  window_deinit(&data->window);
}

Window *notif_pin_entry_get_window(NotifPinEntryData *data) {
  return &data->window;
}
