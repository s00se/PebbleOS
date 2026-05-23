/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/action_bar_layer.h"
#include "applib/ui/window.h"
#include "pbl/services/notifications/alerts_preferences_private.h"

#include <stdbool.h>
#include <stdint.h>

//! Called when the user confirms all three PIN digits via the select button.
//! @param digits  Array of NOTIF_PIN_LEN digits, each in the range [0, 9].
//! @param context Caller-supplied context pointer.
typedef void (*NotifPinEntryCallback)(const uint8_t *digits, void *context);

typedef struct NotifPinEntryData {
  //! Must remain the first member so callers can cast to Window *.
  Window window;
  ActionBarLayer action_bar;
  const char *label;
  uint8_t digits[NOTIF_PIN_LEN];
  uint8_t current_pos;
  //! Set to true when the user confirms the last digit; false if they pressed Back.
  bool pin_confirmed;
  NotifPinEntryCallback callback;
  void *callback_context;
  //! Optional hook called from the window's unload handler after action-bar cleanup.
  void (*on_unload)(struct NotifPinEntryData *data);
} NotifPinEntryData;

//! Initialise a PIN entry window.
//! The window is not pushed to the stack; call app_window_stack_push() separately.
void notif_pin_entry_init(NotifPinEntryData *data, const char *label,
                          NotifPinEntryCallback callback, void *context);

//! Deinitialise: call after the window has been removed from the stack.
void notif_pin_entry_deinit(NotifPinEntryData *data);

//! Returns the root window so it can be pushed to the app window stack.
Window *notif_pin_entry_get_window(NotifPinEntryData *data);
