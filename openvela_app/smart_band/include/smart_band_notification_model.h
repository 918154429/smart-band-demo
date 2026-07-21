#ifndef SMART_BAND_NOTIFICATION_MODEL_H
#define SMART_BAND_NOTIFICATION_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMART_BAND_NOTIFICATION_CAPACITY 16
#define SMART_BAND_NOTIFICATION_SOURCE_CAPACITY 24
#define SMART_BAND_NOTIFICATION_TITLE_CAPACITY 48
#define SMART_BAND_NOTIFICATION_BODY_CAPACITY 128

typedef enum
{
  SMART_BAND_NOTIFICATION_TYPE_CALL = 0,
  SMART_BAND_NOTIFICATION_TYPE_SMS,
  SMART_BAND_NOTIFICATION_TYPE_APP,
  SMART_BAND_NOTIFICATION_TYPE_SYSTEM
} smart_band_notification_type_t;

typedef enum
{
  SMART_BAND_NOTIFICATION_PRIORITY_LOW = 0,
  SMART_BAND_NOTIFICATION_PRIORITY_NORMAL,
  SMART_BAND_NOTIFICATION_PRIORITY_HIGH,
  SMART_BAND_NOTIFICATION_PRIORITY_CRITICAL
} smart_band_notification_priority_t;

typedef enum
{
  SMART_BAND_NOTIFICATION_ACTION_NONE = 0,
  SMART_BAND_NOTIFICATION_ACTION_DISMISSED,
  SMART_BAND_NOTIFICATION_ACTION_ACCEPTED,
  SMART_BAND_NOTIFICATION_ACTION_REJECTED
} smart_band_notification_action_state_t;

typedef enum
{
  SMART_BAND_NOTIFICATION_COMMAND_READ = 0,
  SMART_BAND_NOTIFICATION_COMMAND_DISMISS,
  SMART_BAND_NOTIFICATION_COMMAND_ACCEPT,
  SMART_BAND_NOTIFICATION_COMMAND_REJECT
} smart_band_notification_command_t;

typedef enum
{
  SMART_BAND_NOTIFICATION_PUT_INVALID = 0,
  SMART_BAND_NOTIFICATION_PUT_ADDED,
  SMART_BAND_NOTIFICATION_PUT_UPDATED,
  SMART_BAND_NOTIFICATION_PUT_FULL,
  SMART_BAND_NOTIFICATION_PUT_PROTECTED
} smart_band_notification_put_result_t;

typedef enum
{
  SMART_BAND_NOTIFICATION_ACTION_INVALID = 0,
  SMART_BAND_NOTIFICATION_ACTION_APPLIED,
  SMART_BAND_NOTIFICATION_ACTION_NO_CHANGE,
  SMART_BAND_NOTIFICATION_ACTION_NOT_FOUND
} smart_band_notification_action_result_t;

typedef enum
{
  SMART_BAND_NOTIFICATION_HAPTIC_NONE = 0,
  SMART_BAND_NOTIFICATION_HAPTIC_SUBTLE,
  SMART_BAND_NOTIFICATION_HAPTIC_NORMAL,
  SMART_BAND_NOTIFICATION_HAPTIC_URGENT
} smart_band_notification_haptic_t;

typedef struct
{
  uint32_t id;
  smart_band_notification_type_t type;
  smart_band_notification_priority_t priority;
  const char *source;
  const char *title;
  const char *body;
  uint64_t wall_timestamp;
} smart_band_notification_input_t;

typedef struct
{
  uint32_t id;
  smart_band_notification_type_t type;
  smart_band_notification_priority_t priority;
  char source[SMART_BAND_NOTIFICATION_SOURCE_CAPACITY];
  char title[SMART_BAND_NOTIFICATION_TITLE_CAPACITY];
  char body[SMART_BAND_NOTIFICATION_BODY_CAPACITY];
  uint64_t wall_timestamp;
  bool read;
  bool dismissed;
  smart_band_notification_action_state_t action_state;
} smart_band_notification_t;

typedef struct
{
  smart_band_notification_t items[SMART_BAND_NOTIFICATION_CAPACITY];
  size_t count;
} smart_band_notification_model_t;

typedef struct
{
  bool dnd_enabled;
  bool workout_active;
} smart_band_notification_policy_t;

typedef struct
{
  bool center_only;
  bool overlay;
  bool full_screen;
  smart_band_notification_haptic_t haptic;
  bool wake_request;
} smart_band_notification_presentation_t;

void smart_band_notification_model_init(smart_band_notification_model_t *model);
size_t smart_band_notification_count(
  const smart_band_notification_model_t *model);
const smart_band_notification_t *smart_band_notification_at(
  const smart_band_notification_model_t *model, size_t index);
const smart_band_notification_t *smart_band_notification_find(
  const smart_band_notification_model_t *model, uint32_t id);
smart_band_notification_put_result_t smart_band_notification_put(
  smart_band_notification_model_t *model,
  const smart_band_notification_input_t *input);
smart_band_notification_action_result_t smart_band_notification_apply(
  smart_band_notification_model_t *model, uint32_t id,
  smart_band_notification_command_t command);
bool smart_band_notification_remove(smart_band_notification_model_t *model,
                                    uint32_t id);
bool smart_band_notification_decide_presentation(
  const smart_band_notification_t *notification,
  const smart_band_notification_policy_t *policy,
  smart_band_notification_presentation_t *presentation);

#ifdef __cplusplus
}
#endif

#endif
