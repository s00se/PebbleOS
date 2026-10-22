/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/analytics/analytics.h>
#include <pbl/services/notifications/notification_storage.h>
#include <pbl/services/notifications/notifications.h>
#include <pbl/services/vibes/vibe_intensity.h>
#include <pbl/util/bitops.h>

#include <applib/event_service_client.h>
#include <pbl/services/regular_timer.h>
#include <kernel/events.h>
#include <kernel/pbl_malloc.h>

static void prv_notification_migration_iterator_callback(TimelineItem *notification,
                                                         SerializedTimelineItemHeader *header,
                                                         void *data) {
  header->common.timestamp -= *((int *)data);
  notification->header.timestamp = header->common.timestamp;
}

void notifications_handle_notification_action_result(
    PebbleSysNotificationActionResult *action_result) {
  PebbleEvent launcher_event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification = {
      .type = NotificationActionResult,
      .action_result = action_result,
    }
  };
  // event loop will free memory of action_result
  event_put(&launcher_event);
}

void notifications_handle_notification_removed(Uuid *notification_id) {
  Uuid *removed_id = kernel_malloc_check(sizeof(Uuid));
  *removed_id = *notification_id;
  PebbleEvent launcher_event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification = {
      .type = NotificationRemoved,
      .notification_id = removed_id,
    }
  };
  event_put(&launcher_event);
}

void notifications_handle_notification_added(Uuid *notification_id) {
  PebbleEvent launcher_event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification = {.type = NotificationAdded, .notification_id = notification_id}
  };
  event_put(&launcher_event);
  PBL_ANALYTICS_ADD(notification_received_count, 1);
}

void notifications_handle_notification_acted_upon(Uuid *notification_id) {
  PebbleEvent launcher_event = {
    .type = PEBBLE_SYS_NOTIFICATION_EVENT,
    .sys_notification = {.type = NotificationActedUpon, .notification_id = notification_id}
  };
  event_put(&launcher_event);
}

void notifications_migrate_timezone(const int tz_diff) {
  notification_storage_rewrite(prv_notification_migration_iterator_callback, (void *)&tz_diff);
}

void notification_storage_init(void);
void vibe_intensity_init(void);

#define NOTIFICATION_HISTORY_WIPE_MINUTES 30

static RegularTimerInfo s_wipe_timer_info;
static EventServiceInfo s_comm_session_event_info;

static void prv_wipe_history(void) {
  notification_storage_reset_and_init();
}

static void prv_wipe_timer_callback(void *data) {
  prv_wipe_history();
}

static void prv_comm_session_event_handler(PebbleEvent *e, void *context) {
  if (!e->bluetooth.comm_session_event.is_open && e->bluetooth.comm_session_event.is_system) {
    prv_wipe_history();
  }
}

void notifications_init(void) {
  notification_storage_init();

  s_wipe_timer_info = (RegularTimerInfo){.cb = prv_wipe_timer_callback};
  regular_timer_add_multiminute_callback(&s_wipe_timer_info, NOTIFICATION_HISTORY_WIPE_MINUTES);

  s_comm_session_event_info = (EventServiceInfo){
    .type = PEBBLE_COMM_SESSION_EVENT,
    .handler = prv_comm_session_event_handler,
  };
  event_service_client_subscribe(&s_comm_session_event_info);
}

void notifications_add_notification(TimelineItem *notification) {
  notification_storage_store(notification);

  Uuid *uuid = kernel_malloc_check(sizeof(Uuid));
  *uuid = notification->header.id;
  notifications_handle_notification_added(uuid);
}
