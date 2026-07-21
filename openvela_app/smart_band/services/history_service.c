#include "smart_band_history.h"
#include "smart_band_storage_transaction.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define HISTORY_WIRE_MAGIC UINT32_C(0x31545348)
#define HISTORY_WIRE_HEADER_SIZE 8u
#define DAILY_WIRE_SIZE 28u
#define SESSION_WIRE_SIZE 50u
#define DAILY_SHARD_CAPACITY 15u
#define SESSION_SHARD_CAPACITY 10u
#define CST_OFFSET_SECONDS INT64_C(28800)
#define SECONDS_PER_DAY INT64_C(86400)

static const smart_band_store_record_spec_t g_daily_specs[] =
{
  {{UINT32_C(0x00030000), UINT32_C(0x00030001)},
   SMART_BAND_STORAGE_RECORD_DAILY_HISTORY, 1, 0, NULL, 0, NULL, NULL},
  {{UINT32_C(0x00030002), UINT32_C(0x00030003)},
   SMART_BAND_STORAGE_RECORD_DAILY_HISTORY, 1, 0, NULL, 0, NULL, NULL}
};

static const smart_band_store_record_spec_t g_session_specs[] =
{
  {{UINT32_C(0x00040000), UINT32_C(0x00040001)},
   SMART_BAND_STORAGE_RECORD_WORKOUT_HISTORY, 1, 0, NULL, 0, NULL, NULL},
  {{UINT32_C(0x00040002), UINT32_C(0x00040003)},
   SMART_BAND_STORAGE_RECORD_WORKOUT_HISTORY, 1, 0, NULL, 0, NULL, NULL},
  {{UINT32_C(0x00040004), UINT32_C(0x00040005)},
   SMART_BAND_STORAGE_RECORD_WORKOUT_HISTORY, 1, 0, NULL, 0, NULL, NULL}
};

static uint16_t read_le16(const uint8_t *input)
{
  return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8));
}

static uint32_t read_le32(const uint8_t *input)
{
  return (uint32_t)input[0] | ((uint32_t)input[1] << 8) |
         ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
}

static uint64_t read_le64(const uint8_t *input)
{
  return (uint64_t)read_le32(input) |
         ((uint64_t)read_le32(input + 4) << 32);
}

static void write_le16(uint8_t *output, uint16_t value)
{
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *output, uint32_t value)
{
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
  output[2] = (uint8_t)(value >> 16);
  output[3] = (uint8_t)(value >> 24);
}

static void write_le64(uint8_t *output, uint64_t value)
{
  write_le32(output, (uint32_t)value);
  write_le32(output + 4, (uint32_t)(value >> 32));
}

static size_t daily_shard(int32_t day_key)
{
  int32_t modulo = day_key % (int32_t)SMART_BAND_HISTORY_DAILY_SHARDS;

  return (size_t)(modulo < 0 ? modulo +
                  (int32_t)SMART_BAND_HISTORY_DAILY_SHARDS : modulo);
}

static size_t session_shard(uint32_t id)
{
  return (size_t)((id - 1u) % SMART_BAND_HISTORY_SESSION_SHARDS);
}

static void mark_degraded(smart_band_store_result_t *result)
{
  if (result != NULL && *result >= SMART_BAND_STORE_OK)
    {
      *result = SMART_BAND_STORE_DEGRADED;
    }
}

static bool result_blocks_rewrite(smart_band_store_result_t result)
{
  return result < SMART_BAND_STORE_OK ||
         (result >= SMART_BAND_STORE_RECOVERED &&
          result != SMART_BAND_STORE_UNAVAILABLE);
}

static bool storage_is_permanently_unavailable(
  const smart_band_history_t *history)
{
  return history == NULL || history->store == NULL ||
         !history->store->initialized ||
         smart_band_storage_is_permanently_unavailable(
           &history->store->backend);
}

static bool add_u32(uint32_t left, uint32_t right, uint32_t *result)
{
  if (right > UINT32_MAX - left)
    {
      *result = UINT32_MAX;
      return false;
    }

  *result = left + right;
  return true;
}

static void insert_daily(smart_band_history_t *history,
                         const smart_band_daily_summary_t *entry);

static bool merge_daily_summary(
  smart_band_history_t *history, const smart_band_daily_summary_t *delta)
{
  smart_band_daily_summary_t *target = NULL;
  bool complete;
  bool arithmetic_complete = true;
  bool target_heart_valid;
  bool delta_heart_valid;
  size_t index;

  for (index = 0; index < history->daily_count; index++)
    {
      if (history->daily[index].day_key == delta->day_key)
        {
          target = &history->daily[index];
          break;
        }
    }

  if (target == NULL)
    {
      insert_daily(history, delta);
      for (index = 0; index < history->daily_count; index++)
        {
          if (history->daily[index].day_key == delta->day_key)
            {
              history->daily_dirty_shards =
                (uint8_t)((1u << SMART_BAND_HISTORY_DAILY_SHARDS) - 1u);
              return true;
            }
        }
      return true;
    }

  complete = (target->flags & SMART_BAND_HISTORY_DAY_COMPLETE) != 0u &&
             (delta->flags & SMART_BAND_HISTORY_DAY_COMPLETE) != 0u;
  target_heart_valid =
    (target->flags & SMART_BAND_HISTORY_DAY_HEART_VALID) != 0u;
  delta_heart_valid =
    (delta->flags & SMART_BAND_HISTORY_DAY_HEART_VALID) != 0u;
  arithmetic_complete &= add_u32(target->steps, delta->steps,
                                 &target->steps);
  arithmetic_complete &= add_u32(target->active_seconds,
                                 delta->active_seconds,
                                 &target->active_seconds);
  arithmetic_complete &= add_u32(target->calories_milli_kcal,
                                 delta->calories_milli_kcal,
                                 &target->calories_milli_kcal);
  arithmetic_complete &= add_u32(target->heart_weighted_bpm_seconds,
                                 delta->heart_weighted_bpm_seconds,
                                 &target->heart_weighted_bpm_seconds);
  arithmetic_complete &= add_u32(target->heart_duration_seconds,
                                 delta->heart_duration_seconds,
                                 &target->heart_duration_seconds);
  if (delta_heart_valid)
    {
      if (!target_heart_valid)
        {
          target->heart_min_bpm = delta->heart_min_bpm;
          target->heart_max_bpm = delta->heart_max_bpm;
        }
      else
        {
          if (delta->heart_min_bpm < target->heart_min_bpm)
            {
              target->heart_min_bpm = delta->heart_min_bpm;
            }
          if (delta->heart_max_bpm > target->heart_max_bpm)
            {
              target->heart_max_bpm = delta->heart_max_bpm;
            }
        }
    }
  target->source_flags |= delta->source_flags;
  target->flags |= delta->flags;
  if (complete && arithmetic_complete)
    {
      target->flags |= SMART_BAND_HISTORY_DAY_COMPLETE;
    }
  else
    {
      target->flags = (uint8_t)(target->flags &
                                ~SMART_BAND_HISTORY_DAY_COMPLETE);
    }
  if (!arithmetic_complete)
    {
      target->flags |= SMART_BAND_HISTORY_DAY_OVERFLOW;
    }
  history->daily_dirty_shards |=
    (uint8_t)(1u << daily_shard(delta->day_key));
  return true;
}

static void insert_daily(smart_band_history_t *history,
                         const smart_band_daily_summary_t *entry)
{
  int32_t latest_day;
  int32_t oldest_day;
  size_t remove_count = 0u;
  size_t index;

  latest_day = history->daily_count == 0u ? entry->day_key :
               history->daily[history->daily_count - 1u].day_key;
  if (entry->day_key > latest_day)
    {
      latest_day = entry->day_key;
    }
  oldest_day = latest_day -
               (int32_t)(SMART_BAND_HISTORY_DAILY_CAPACITY - 1u);
  if (entry->day_key < oldest_day)
    {
      return;
    }

  while (remove_count < history->daily_count &&
         history->daily[remove_count].day_key < oldest_day)
    {
      remove_count++;
    }
  if (remove_count != 0u)
    {
      memmove(&history->daily[0], &history->daily[remove_count],
              (history->daily_count - remove_count) *
              sizeof(history->daily[0]));
      history->daily_count -= remove_count;
    }

  for (index = 0; index < history->daily_count; index++)
    {
      if (history->daily[index].day_key == entry->day_key)
        {
          history->daily[index] = *entry;
          return;
        }

      if (history->daily[index].day_key > entry->day_key)
        {
          break;
        }
    }

  if (history->daily_count == SMART_BAND_HISTORY_DAILY_CAPACITY)
    {
      if (index == 0)
        {
          return;
        }

      memmove(&history->daily[0], &history->daily[1],
              (history->daily_count - 1u) * sizeof(history->daily[0]));
      history->daily_count--;
      index--;
    }

  memmove(&history->daily[index + 1u], &history->daily[index],
          (history->daily_count - index) * sizeof(history->daily[0]));
  history->daily[index] = *entry;
  history->daily_count++;
}

static bool insert_session(smart_band_history_t *history,
                           const smart_band_workout_session_t *entry)
{
  size_t index;

  for (index = 0; index < history->session_count; index++)
    {
      if (history->sessions[index].id == entry->id)
        {
          return smart_band_history_session_equal(&history->sessions[index],
                                                   entry);
        }

      if (history->sessions[index].id > entry->id)
        {
          break;
        }
    }

  if (history->session_count == SMART_BAND_HISTORY_SESSION_CAPACITY)
    {
      if (index == 0)
        {
          return true;
        }

      memmove(&history->sessions[0], &history->sessions[1],
              (history->session_count - 1u) * sizeof(history->sessions[0]));
      history->session_count--;
      index--;
    }

  memmove(&history->sessions[index + 1u], &history->sessions[index],
          (history->session_count - index) * sizeof(history->sessions[0]));
  history->sessions[index] = *entry;
  history->session_count++;
  return true;
}

static void encode_daily(uint8_t *output,
                         const smart_band_daily_summary_t *entry)
{
  write_le32(output, (uint32_t)entry->day_key);
  write_le32(output + 4, entry->steps);
  write_le32(output + 8, entry->active_seconds);
  write_le32(output + 12, entry->calories_milli_kcal);
  write_le32(output + 16, entry->heart_weighted_bpm_seconds);
  write_le32(output + 20, entry->heart_duration_seconds);
  output[24] = entry->heart_min_bpm;
  output[25] = entry->heart_max_bpm;
  output[26] = entry->source_flags;
  output[27] = entry->flags;
}

static bool decode_daily(const uint8_t *input, size_t shard,
                         smart_band_daily_summary_t *entry)
{
  memset(entry, 0, sizeof(*entry));
  entry->day_key = (int32_t)read_le32(input);
  entry->steps = read_le32(input + 4);
  entry->active_seconds = read_le32(input + 8);
  entry->calories_milli_kcal = read_le32(input + 12);
  entry->heart_weighted_bpm_seconds = read_le32(input + 16);
  entry->heart_duration_seconds = read_le32(input + 20);
  entry->heart_min_bpm = input[24];
  entry->heart_max_bpm = input[25];
  entry->source_flags = input[26];
  entry->flags = input[27];
  return daily_shard(entry->day_key) == shard &&
         entry->heart_min_bpm <= entry->heart_max_bpm &&
         ((entry->flags & SMART_BAND_HISTORY_DAY_HEART_VALID) != 0u ||
          (entry->heart_min_bpm == 0u && entry->heart_max_bpm == 0u &&
           entry->heart_duration_seconds == 0u &&
           entry->heart_weighted_bpm_seconds == 0u));
}

static void encode_session(uint8_t *output,
                           const smart_band_workout_session_t *entry)
{
  write_le32(output, entry->id);
  write_le64(output + 4, (uint64_t)entry->start_wall_time);
  write_le64(output + 12, (uint64_t)entry->end_wall_time);
  write_le64(output + 20, entry->active_duration_ms);
  write_le32(output + 28, entry->steps);
  write_le32(output + 32, entry->distance_mm);
  write_le32(output + 36, entry->calories_milli_kcal);
  output[40] = entry->heart_current_bpm;
  output[41] = entry->heart_min_bpm;
  output[42] = entry->heart_max_bpm;
  output[43] = entry->heart_average_bpm;
  write_le16(output + 44, entry->pause_count);
  output[46] = entry->mode;
  output[47] = entry->status;
  output[48] = entry->source_flags;
  output[49] = entry->flags;
}

static bool decode_session(const uint8_t *input, size_t shard,
                           smart_band_workout_session_t *entry)
{
  memset(entry, 0, sizeof(*entry));
  entry->id = read_le32(input);
  entry->start_wall_time = (int64_t)read_le64(input + 4);
  entry->end_wall_time = (int64_t)read_le64(input + 12);
  entry->active_duration_ms = read_le64(input + 20);
  entry->steps = read_le32(input + 28);
  entry->distance_mm = read_le32(input + 32);
  entry->calories_milli_kcal = read_le32(input + 36);
  entry->heart_current_bpm = input[40];
  entry->heart_min_bpm = input[41];
  entry->heart_max_bpm = input[42];
  entry->heart_average_bpm = input[43];
  entry->pause_count = read_le16(input + 44);
  entry->mode = input[46];
  entry->status = input[47];
  entry->source_flags = input[48];
  entry->flags = input[49];
  return entry->id != 0u && session_shard(entry->id) == shard &&
         entry->mode < SMART_BAND_WORKOUT_MODE_COUNT &&
         entry->status <= SMART_BAND_HISTORY_STATUS_ABORTED &&
         entry->heart_min_bpm <= entry->heart_max_bpm;
}

static bool decode_header(const uint8_t *payload, size_t payload_size,
                          size_t expected_shard, size_t entry_size,
                          size_t capacity, size_t *count)
{
  if (payload_size < HISTORY_WIRE_HEADER_SIZE ||
      read_le32(payload) != HISTORY_WIRE_MAGIC ||
      payload[4] != expected_shard || payload[5] != entry_size ||
      payload[6] > capacity || payload[7] != 0u ||
      payload_size != HISTORY_WIRE_HEADER_SIZE +
                      (size_t)payload[6] * entry_size)
    {
      return false;
    }

  *count = payload[6];
  return true;
}

static void encode_header(uint8_t *payload, size_t shard,
                          size_t entry_size, size_t count)
{
  write_le32(payload, HISTORY_WIRE_MAGIC);
  payload[4] = (uint8_t)shard;
  payload[5] = (uint8_t)entry_size;
  payload[6] = (uint8_t)count;
  payload[7] = 0u;
}

static void load_daily_shard(smart_band_history_t *history, size_t shard)
{
  uint8_t payload[SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE];
  size_t payload_size = 0;
  size_t count;
  size_t index;
  smart_band_store_result_t result;

  result = smart_band_store_load(history->store, &g_daily_specs[shard],
                                 payload, sizeof(payload), &payload_size,
                                 NULL);
  if (result < SMART_BAND_STORE_OK)
    {
      history->last_daily_result = result;
      return;
    }

  if (result > history->last_daily_result)
    {
      history->last_daily_result = result;
    }

  if (payload_size == 0u)
    {
      return;
    }

  if (!decode_header(payload, payload_size, shard, DAILY_WIRE_SIZE,
                     DAILY_SHARD_CAPACITY, &count))
    {
      mark_degraded(&history->last_daily_result);
      return;
    }

  for (index = 0; index < count; index++)
    {
      smart_band_daily_summary_t entry;
      const uint8_t *wire = payload + HISTORY_WIRE_HEADER_SIZE +
                            index * DAILY_WIRE_SIZE;

      if (!decode_daily(wire, shard, &entry))
        {
          mark_degraded(&history->last_daily_result);
          continue;
        }

      insert_daily(history, &entry);
    }
}

static void load_session_shard(smart_band_history_t *history, size_t shard)
{
  uint8_t payload[SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE];
  size_t payload_size = 0;
  size_t count;
  size_t index;
  smart_band_store_result_t result;

  result = smart_band_store_load(history->store, &g_session_specs[shard],
                                 payload, sizeof(payload), &payload_size,
                                 NULL);
  if (result < SMART_BAND_STORE_OK)
    {
      history->last_session_result = result;
      return;
    }

  if (result > history->last_session_result)
    {
      history->last_session_result = result;
    }

  if (payload_size == 0u)
    {
      return;
    }

  if (!decode_header(payload, payload_size, shard, SESSION_WIRE_SIZE,
                     SESSION_SHARD_CAPACITY, &count))
    {
      mark_degraded(&history->last_session_result);
      return;
    }

  for (index = 0; index < count; index++)
    {
      smart_band_workout_session_t entry;
      const uint8_t *wire = payload + HISTORY_WIRE_HEADER_SIZE +
                            index * SESSION_WIRE_SIZE;

      if (!decode_session(wire, shard, &entry) ||
          !insert_session(history, &entry))
        {
          mark_degraded(&history->last_session_result);
        }
    }
}

static smart_band_store_result_t commit_daily_shard(
  smart_band_history_t *history, size_t shard)
{
  uint8_t payload[HISTORY_WIRE_HEADER_SIZE +
                  DAILY_SHARD_CAPACITY * DAILY_WIRE_SIZE];
  size_t count = 0;
  size_t index;

  for (index = 0; index < history->daily_count; index++)
    {
      if (daily_shard(history->daily[index].day_key) == shard)
        {
          encode_daily(payload + HISTORY_WIRE_HEADER_SIZE +
                       count * DAILY_WIRE_SIZE, &history->daily[index]);
          count++;
        }
    }

  encode_header(payload, shard, DAILY_WIRE_SIZE, count);
  return smart_band_store_commit(
    history->store, &g_daily_specs[shard], payload,
    HISTORY_WIRE_HEADER_SIZE + count * DAILY_WIRE_SIZE, NULL);
}

static smart_band_store_result_t commit_session_shard(
  smart_band_history_t *history, size_t shard)
{
  uint8_t payload[HISTORY_WIRE_HEADER_SIZE +
                  SESSION_SHARD_CAPACITY * SESSION_WIRE_SIZE];
  size_t count = 0;
  size_t index;

  for (index = 0; index < history->session_count; index++)
    {
      if (session_shard(history->sessions[index].id) == shard)
        {
          encode_session(payload + HISTORY_WIRE_HEADER_SIZE +
                         count * SESSION_WIRE_SIZE,
                         &history->sessions[index]);
          count++;
        }
    }

  encode_header(payload, shard, SESSION_WIRE_SIZE, count);
  return smart_band_store_commit(
    history->store, &g_session_specs[shard], payload,
    HISTORY_WIRE_HEADER_SIZE + count * SESSION_WIRE_SIZE, NULL);
}

int smart_band_history_init(smart_band_history_t *history,
                            smart_band_store_t *store)
{
  smart_band_store_result_t transaction_result = SMART_BAND_STORE_UNAVAILABLE;
  size_t shard;

  if (history == NULL)
    {
      return -1;
    }

  memset(history, 0, sizeof(*history));
  history->store = store;
  history->next_session_id = 1u;
  history->last_daily_result = SMART_BAND_STORE_UNAVAILABLE;
  history->last_session_result = SMART_BAND_STORE_UNAVAILABLE;
  history->last_transaction_result = SMART_BAND_STORE_UNAVAILABLE;
  if (store != NULL && store->initialized)
    {
      transaction_result = smart_band_storage_transaction_recover(store);
      history->last_transaction_result = transaction_result;
      history->last_daily_result = SMART_BAND_STORE_OK;
      history->last_session_result = SMART_BAND_STORE_OK;
      for (shard = 0; shard < SMART_BAND_HISTORY_DAILY_SHARDS; shard++)
        {
          load_daily_shard(history, shard);
        }
      for (shard = 0; shard < SMART_BAND_HISTORY_SESSION_SHARDS; shard++)
        {
          load_session_shard(history, shard);
        }
      history->daily_writes_blocked =
        result_blocks_rewrite(history->last_daily_result);
      history->session_writes_blocked =
        result_blocks_rewrite(history->last_session_result);
      if (transaction_result != SMART_BAND_STORE_OK &&
          transaction_result != SMART_BAND_STORE_UNAVAILABLE)
        {
          history->last_daily_result = transaction_result;
          history->last_session_result = transaction_result;
          history->daily_writes_blocked = true;
          history->session_writes_blocked = true;
        }

      history->storage_reload_pending =
        !storage_is_permanently_unavailable(history) &&
        (transaction_result == SMART_BAND_STORE_UNAVAILABLE ||
         history->last_daily_result == SMART_BAND_STORE_UNAVAILABLE ||
         history->last_session_result == SMART_BAND_STORE_UNAVAILABLE);
      if (history->storage_reload_pending)
        {
          /* A partial read is not a baseline. Keep only deltas collected
           * after init, then merge them after a complete retry. */
          history->daily_count = 0u;
          history->session_count = 0u;
          history->next_session_id = 1u;
          history->daily_dirty_shards = 0u;
          history->daily_writes_blocked = false;
          history->session_writes_blocked = false;
        }
    }

  if (history->session_count != 0u)
    {
      uint32_t latest = history->sessions[history->session_count - 1u].id;

      history->next_session_id = latest == UINT32_MAX ? 0u : latest + 1u;
    }

  history->initialized = true;
  return 0;
}

smart_band_store_result_t smart_band_history_recover_storage(
  smart_band_history_t *history)
{
  smart_band_history_t persisted;
  smart_band_store_result_t result;
  size_t index;

  if (history == NULL || !history->initialized)
    {
      return SMART_BAND_STORE_INVALID;
    }
  if (!history->storage_reload_pending)
    {
      return SMART_BAND_STORE_OK;
    }
  if (history->store == NULL || !history->store->initialized ||
      storage_is_permanently_unavailable(history))
    {
      return SMART_BAND_STORE_UNAVAILABLE;
    }

  if (smart_band_history_init(&persisted, history->store) != 0)
    {
      return SMART_BAND_STORE_INVALID;
    }
  if (persisted.storage_reload_pending)
    {
      return SMART_BAND_STORE_UNAVAILABLE;
    }
  if (persisted.daily_writes_blocked || persisted.session_writes_blocked)
    {
      result = persisted.daily_writes_blocked ?
               persisted.last_daily_result : persisted.last_session_result;
      history->last_transaction_result = persisted.last_transaction_result;
      history->last_daily_result = persisted.last_daily_result;
      history->last_session_result = persisted.last_session_result;
      history->daily_writes_blocked = persisted.daily_writes_blocked;
      history->session_writes_blocked = persisted.session_writes_blocked;
      history->storage_reload_pending = false;
      return result;
    }

  for (index = 0; index < history->daily_count; index++)
    {
      if (!merge_daily_summary(&persisted, &history->daily[index]))
        {
          return SMART_BAND_STORE_INVALID;
        }
    }
  for (index = 0; index < history->session_count; index++)
    {
      if (!insert_session(&persisted, &history->sessions[index]))
        {
          return SMART_BAND_STORE_INVALID;
        }
    }

  if (persisted.session_count != 0u)
    {
      uint32_t latest =
        persisted.sessions[persisted.session_count - 1u].id;

      persisted.next_session_id = latest == UINT32_MAX ? 0u : latest + 1u;
    }
  persisted.storage_reload_pending = false;
  *history = persisted;
  return SMART_BAND_STORE_OK;
}

void smart_band_history_reset(smart_band_history_t *history)
{
  if (history != NULL)
    {
      memset(history, 0, sizeof(*history));
    }
}

bool smart_band_history_day_key(time_t wall_time, int32_t *day_key)
{
  int64_t seconds = (int64_t)wall_time;
  int64_t days;

  if (day_key == NULL || seconds < 0 ||
      seconds > INT64_MAX - CST_OFFSET_SECONDS)
    {
      return false;
    }

  days = (seconds + CST_OFFSET_SECONDS) / SECONDS_PER_DAY;
  if (days > INT32_MAX)
    {
      return false;
    }

  *day_key = (int32_t)days;
  return true;
}

bool smart_band_history_format_day(int32_t day_key, char *buffer,
                                   size_t size)
{
  int64_t z;
  int64_t era;
  unsigned int doe;
  unsigned int yoe;
  int year;
  unsigned int doy;
  unsigned int mp;
  unsigned int day;
  unsigned int month;

  if (buffer == NULL || size == 0u)
    {
      return false;
    }

  z = (int64_t)day_key + 719468;
  era = (z >= 0 ? z : z - 146096) / 146097;
  doe = (unsigned int)(z - era * 146097);
  yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
  year = (int)yoe + (int)(era * 400);
  doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
  mp = (5u * doy + 2u) / 153u;
  day = doy - (153u * mp + 2u) / 5u + 1u;
  month = mp < 10u ? mp + 3u : mp - 9u;
  year += month <= 2u;
  return snprintf(buffer, size, "%04d-%02u-%02u", year, month, day) > 0;
}

bool smart_band_history_add_daily(smart_band_history_t *history,
                                  int32_t day_key, uint32_t step_delta,
                                  uint32_t active_seconds,
                                  uint32_t calories_milli_kcal,
                                  uint16_t heart_rate_bpm,
                                  bool heart_rate_valid,
                                  uint32_t heart_sample_seconds,
                                  uint8_t source_flags,
                                  uint8_t additional_flags)
{
  smart_band_daily_summary_t entry;
  smart_band_daily_summary_t *target = NULL;
  uint32_t weighted = 0u;
  bool complete = true;
  bool inserted = false;
  size_t index;

  if (history == NULL || !history->initialized || day_key < 0 ||
      (heart_rate_valid && (heart_rate_bpm == 0u ||
                            heart_rate_bpm > UINT8_MAX)))
    {
      return false;
    }

  for (index = 0; index < history->daily_count; index++)
    {
      if (history->daily[index].day_key == day_key)
        {
          target = &history->daily[index];
          break;
        }
    }

  if (target == NULL)
    {
      memset(&entry, 0, sizeof(entry));
      entry.day_key = day_key;
      entry.flags = SMART_BAND_HISTORY_DAY_COMPLETE;
      insert_daily(history, &entry);
      inserted = true;
      for (index = 0; index < history->daily_count; index++)
        {
          if (history->daily[index].day_key == day_key)
            {
              target = &history->daily[index];
              break;
            }
        }
    }

  if (target == NULL)
    {
      return true;
    }

  complete &= add_u32(target->steps, step_delta, &target->steps);
  complete &= add_u32(target->active_seconds, active_seconds,
                      &target->active_seconds);
  complete &= add_u32(target->calories_milli_kcal, calories_milli_kcal,
                      &target->calories_milli_kcal);
  if (heart_rate_valid && heart_sample_seconds != 0u)
    {
      if (heart_sample_seconds > UINT32_MAX / heart_rate_bpm)
        {
          weighted = UINT32_MAX;
          complete = false;
        }
      else
        {
          weighted = heart_sample_seconds * heart_rate_bpm;
        }

      complete &= add_u32(target->heart_weighted_bpm_seconds, weighted,
                          &target->heart_weighted_bpm_seconds);
      complete &= add_u32(target->heart_duration_seconds,
                          heart_sample_seconds,
                          &target->heart_duration_seconds);
      if ((target->flags & SMART_BAND_HISTORY_DAY_HEART_VALID) == 0u)
        {
          target->heart_min_bpm = (uint8_t)heart_rate_bpm;
          target->heart_max_bpm = (uint8_t)heart_rate_bpm;
        }
      else
        {
          if (heart_rate_bpm < target->heart_min_bpm)
            {
              target->heart_min_bpm = (uint8_t)heart_rate_bpm;
            }
          if (heart_rate_bpm > target->heart_max_bpm)
            {
              target->heart_max_bpm = (uint8_t)heart_rate_bpm;
            }
        }
      target->flags |= SMART_BAND_HISTORY_DAY_HEART_VALID;
    }

  target->source_flags |= source_flags;
  if (!complete)
    {
      target->flags = (uint8_t)(target->flags & UINT8_C(0xfe));
      target->flags |= SMART_BAND_HISTORY_DAY_OVERFLOW;
    }
  if ((additional_flags & SMART_BAND_HISTORY_DAY_CLOCK_ROLLBACK) != 0u)
    {
      target->flags = (uint8_t)(target->flags & UINT8_C(0xfe));
      target->flags |= SMART_BAND_HISTORY_DAY_CLOCK_ROLLBACK;
    }
  if ((additional_flags & SMART_BAND_HISTORY_DAY_RECOVERY_GAP) != 0u)
    {
      target->flags = (uint8_t)(target->flags & UINT8_C(0xfe));
      target->flags |= SMART_BAND_HISTORY_DAY_RECOVERY_GAP;
    }
  if ((additional_flags & SMART_BAND_HISTORY_DAY_OVERFLOW) != 0u)
    {
      target->flags = (uint8_t)(target->flags & UINT8_C(0xfe));
      target->flags |= SMART_BAND_HISTORY_DAY_OVERFLOW;
    }

  history->daily_dirty_shards |= inserted ?
    (uint8_t)((1u << SMART_BAND_HISTORY_DAILY_SHARDS) - 1u) :
    (uint8_t)(1u << daily_shard(day_key));
  return true;
}

smart_band_store_result_t
smart_band_history_flush_daily(smart_band_history_t *history)
{
  smart_band_store_result_t aggregate = SMART_BAND_STORE_OK;
  smart_band_store_result_t recovery;
  size_t shard;

  if (history == NULL || !history->initialized)
    {
      return SMART_BAND_STORE_INVALID;
    }
  recovery = smart_band_history_recover_storage(history);
  if (recovery != SMART_BAND_STORE_OK)
    {
      return recovery;
    }
  if (history->daily_dirty_shards == 0u)
    {
      return history->last_daily_result == SMART_BAND_STORE_UNAVAILABLE ?
             SMART_BAND_STORE_UNAVAILABLE : SMART_BAND_STORE_OK;
    }
  if (history->daily_writes_blocked)
    {
      return history->last_daily_result;
    }
  if (history->store == NULL || !history->store->initialized ||
      (history->last_daily_result == SMART_BAND_STORE_UNAVAILABLE &&
       storage_is_permanently_unavailable(history)))
    {
      history->last_daily_result = SMART_BAND_STORE_UNAVAILABLE;
      return history->last_daily_result;
    }

  for (shard = 0; shard < SMART_BAND_HISTORY_DAILY_SHARDS; shard++)
    {
      smart_band_store_result_t result;

      if ((history->daily_dirty_shards & (1u << shard)) == 0u)
        {
          continue;
        }
      result = commit_daily_shard(history, shard);
      if (result != SMART_BAND_STORE_OK)
        {
          history->last_daily_result = result;
          return result;
        }
      history->daily_dirty_shards &= (uint8_t)~(1u << shard);
      if (result > aggregate)
        {
          aggregate = result;
        }
    }

  history->last_daily_result = aggregate;
  return aggregate;
}

smart_band_store_result_t smart_band_history_append_session(
  smart_band_history_t *history,
  const smart_band_workout_session_t *session)
{
  smart_band_workout_session_t
    before[SMART_BAND_HISTORY_SESSION_CAPACITY];
  size_t before_count;
  size_t index;
  size_t shard;
  smart_band_store_result_t result;

  if (history == NULL || session == NULL || !history->initialized ||
      session->id == 0u || session->mode >= SMART_BAND_WORKOUT_MODE_COUNT ||
      session->status > SMART_BAND_HISTORY_STATUS_ABORTED)
    {
      return SMART_BAND_STORE_INVALID;
    }

  result = smart_band_history_recover_storage(history);
  if (result != SMART_BAND_STORE_OK)
    {
      return result;
    }

  for (index = 0; index < history->session_count; index++)
    {
      if (history->sessions[index].id == session->id)
        {
          return smart_band_history_session_equal(&history->sessions[index],
                                                   session) ?
                 SMART_BAND_STORE_OK : SMART_BAND_STORE_INVALID;
        }
    }

  if (history->session_writes_blocked)
    {
      return history->last_session_result;
    }

  if (session->id != history->next_session_id)
    {
      return SMART_BAND_STORE_INVALID;
    }

  before_count = history->session_count;
  memcpy(before, history->sessions,
         before_count * sizeof(history->sessions[0]));
  if (!insert_session(history, session))
    {
      history->last_session_result = SMART_BAND_STORE_INVALID;
      return history->last_session_result;
    }

  if (history->store == NULL || !history->store->initialized ||
      (history->last_session_result == SMART_BAND_STORE_UNAVAILABLE &&
       storage_is_permanently_unavailable(history)))
    {
      history->next_session_id = session->id == UINT32_MAX ?
                                 0u : session->id + 1u;
      history->last_session_result = SMART_BAND_STORE_UNAVAILABLE;
      return history->last_session_result;
    }

  shard = session_shard(session->id);
  result = commit_session_shard(history, shard);
  if (result != SMART_BAND_STORE_OK)
    {
      memcpy(history->sessions, before,
             before_count * sizeof(history->sessions[0]));
      history->session_count = before_count;
    }
  else if (session->id >= history->next_session_id)
    {
      history->next_session_id = session->id == UINT32_MAX ?
                                 0u : session->id + 1u;
    }

  history->last_session_result = result;
  return result;
}

smart_band_store_result_t smart_band_history_commit_workout(
  smart_band_history_t *history,
  const smart_band_store_record_spec_t *checkpoint_spec,
  const void *checkpoint_payload, size_t checkpoint_payload_size,
  const smart_band_workout_session_t *session)
{
  smart_band_storage_transaction_participant_t participants[
    SMART_BAND_STORAGE_TRANSACTION_MAX_PARTICIPANTS];
  uint8_t daily_payloads[SMART_BAND_HISTORY_DAILY_SHARDS]
                        [HISTORY_WIRE_HEADER_SIZE +
                         DAILY_SHARD_CAPACITY * DAILY_WIRE_SIZE];
  uint8_t session_payload[HISTORY_WIRE_HEADER_SIZE +
                          SESSION_SHARD_CAPACITY * SESSION_WIRE_SIZE];
  smart_band_workout_session_t
    previous_sessions[SMART_BAND_HISTORY_SESSION_CAPACITY];
  size_t previous_session_count = 0u;
  uint32_t previous_next_session_id = 0u;
  size_t participant_count = 0u;
  size_t shard;
  bool session_already_present = false;
  bool session_staged = false;
  smart_band_store_result_t result;

  if (history == NULL || !history->initialized || checkpoint_spec == NULL ||
      (checkpoint_payload == NULL && checkpoint_payload_size != 0u) ||
      checkpoint_payload_size > SMART_BAND_STORAGE_CODEC_MAX_PAYLOAD_SIZE ||
      history->store == NULL || !history->store->initialized)
    {
      return SMART_BAND_STORE_INVALID;
    }
  if (storage_is_permanently_unavailable(history))
    {
      history->last_transaction_result = SMART_BAND_STORE_UNAVAILABLE;
      return history->last_transaction_result;
    }
  result = smart_band_history_recover_storage(history);
  if (result != SMART_BAND_STORE_OK)
    {
      history->last_transaction_result = result;
      return result;
    }
  if (history->daily_writes_blocked ||
      (session != NULL && history->session_writes_blocked))
    {
      result = history->daily_writes_blocked ? history->last_daily_result :
               history->last_session_result;
      history->last_transaction_result = result;
      return result;
    }

  memset(participants, 0, sizeof(participants));
  for (shard = 0; shard < SMART_BAND_HISTORY_DAILY_SHARDS; shard++)
    {
      size_t count = 0u;
      size_t index;

      if ((history->daily_dirty_shards & (1u << shard)) == 0u)
        {
          continue;
        }
      for (index = 0; index < history->daily_count; index++)
        {
          if (daily_shard(history->daily[index].day_key) == shard)
            {
              encode_daily(daily_payloads[shard] + HISTORY_WIRE_HEADER_SIZE +
                           count * DAILY_WIRE_SIZE, &history->daily[index]);
              count++;
            }
        }
      encode_header(daily_payloads[shard], shard, DAILY_WIRE_SIZE, count);
      participants[participant_count].target = g_daily_specs[shard];
      participants[participant_count].payload = daily_payloads[shard];
      participants[participant_count].payload_size =
        HISTORY_WIRE_HEADER_SIZE + count * DAILY_WIRE_SIZE;
      participant_count++;
    }

  if (session != NULL)
    {
      size_t index;

      if (session->id == 0u ||
          session->mode >= SMART_BAND_WORKOUT_MODE_COUNT ||
          session->status > SMART_BAND_HISTORY_STATUS_ABORTED)
        {
          return SMART_BAND_STORE_INVALID;
        }
      for (index = 0; index < history->session_count; index++)
        {
          if (history->sessions[index].id == session->id)
            {
              if (!smart_band_history_session_equal(&history->sessions[index],
                                                     session))
                {
                  return SMART_BAND_STORE_INVALID;
                }
              session_already_present = true;
              break;
            }
        }
      if (!session_already_present)
        {
          size_t count = 0u;

          if (session->id != history->next_session_id)
            {
              return SMART_BAND_STORE_INVALID;
            }
          previous_session_count = history->session_count;
          previous_next_session_id = history->next_session_id;
          memcpy(previous_sessions, history->sessions,
                 previous_session_count * sizeof(previous_sessions[0]));
          if (!insert_session(history, session))
            {
              return SMART_BAND_STORE_INVALID;
            }
          shard = session_shard(session->id);
          for (index = 0; index < history->session_count; index++)
            {
              if (session_shard(history->sessions[index].id) == shard)
                {
                  encode_session(session_payload + HISTORY_WIRE_HEADER_SIZE +
                                 count * SESSION_WIRE_SIZE,
                                 &history->sessions[index]);
                  count++;
                }
            }
          encode_header(session_payload, shard, SESSION_WIRE_SIZE, count);
          memcpy(history->sessions, previous_sessions,
                 previous_session_count * sizeof(previous_sessions[0]));
          history->session_count = previous_session_count;
          history->next_session_id = previous_next_session_id;
          participants[participant_count].target = g_session_specs[shard];
          participants[participant_count].payload = session_payload;
          participants[participant_count].payload_size =
            HISTORY_WIRE_HEADER_SIZE + count * SESSION_WIRE_SIZE;
          participant_count++;
          session_staged = true;
        }
    }

  if (participant_count >=
      SMART_BAND_STORAGE_TRANSACTION_MAX_PARTICIPANTS)
    {
      return SMART_BAND_STORE_INVALID;
    }
  participants[participant_count].target = *checkpoint_spec;
  participants[participant_count].payload = checkpoint_payload;
  participants[participant_count].payload_size = checkpoint_payload_size;
  participant_count++;

  result = smart_band_storage_transaction_commit(
    history->store, participants, participant_count);
  history->last_transaction_result = result;
  if (result != SMART_BAND_STORE_OK)
    {
      if (result == SMART_BAND_STORE_UNAVAILABLE)
        {
          history->last_daily_result = result;
          if (session != NULL)
            {
              history->last_session_result = result;
            }
        }
      return result;
    }

  history->daily_dirty_shards = 0u;
  history->last_daily_result = SMART_BAND_STORE_OK;
  if (session_staged)
    {
      if (!insert_session(history, session))
        {
          history->last_session_result = SMART_BAND_STORE_DEGRADED;
          history->session_writes_blocked = true;
          return history->last_session_result;
        }
      history->next_session_id = session->id == UINT32_MAX ?
                                 0u : session->id + 1u;
    }
  if (session != NULL)
    {
      history->last_session_result = SMART_BAND_STORE_OK;
    }
  return SMART_BAND_STORE_OK;
}

size_t smart_band_history_latest_days(const smart_band_history_t *history,
                                      smart_band_daily_summary_t *output,
                                      size_t capacity)
{
  size_t count;

  if (history == NULL || output == NULL || capacity == 0u)
    {
      return 0u;
    }
  count = history->daily_count < capacity ? history->daily_count : capacity;
  memcpy(output, &history->daily[history->daily_count - count],
         count * sizeof(output[0]));
  return count;
}

size_t smart_band_history_latest_sessions(
  const smart_band_history_t *history,
  smart_band_workout_session_t *output, size_t capacity)
{
  size_t count;

  if (history == NULL || output == NULL || capacity == 0u)
    {
      return 0u;
    }
  count = history->session_count < capacity ?
          history->session_count : capacity;
  memcpy(output, &history->sessions[history->session_count - count],
         count * sizeof(output[0]));
  return count;
}

uint32_t smart_band_history_daily_average_heart_rate(
  const smart_band_daily_summary_t *summary)
{
  return summary == NULL || summary->heart_duration_seconds == 0u ? 0u :
         summary->heart_weighted_bpm_seconds /
         summary->heart_duration_seconds;
}

bool smart_band_history_session_equal(
  const smart_band_workout_session_t *left,
  const smart_band_workout_session_t *right)
{
  return left != NULL && right != NULL &&
         left->id == right->id &&
         left->start_wall_time == right->start_wall_time &&
         left->end_wall_time == right->end_wall_time &&
         left->active_duration_ms == right->active_duration_ms &&
         left->steps == right->steps &&
         left->distance_mm == right->distance_mm &&
         left->calories_milli_kcal == right->calories_milli_kcal &&
         left->heart_current_bpm == right->heart_current_bpm &&
         left->heart_min_bpm == right->heart_min_bpm &&
         left->heart_max_bpm == right->heart_max_bpm &&
         left->heart_average_bpm == right->heart_average_bpm &&
         left->pause_count == right->pause_count &&
         left->mode == right->mode && left->status == right->status &&
         left->source_flags == right->source_flags &&
         left->flags == right->flags;
}
