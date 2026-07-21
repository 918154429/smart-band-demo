#include "fake_lvgl/fake_lvgl_test.h"
#include "smart_band_watch_face.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
  do                                                                         \
    {                                                                        \
      if (!(condition))                                                      \
        {                                                                    \
          fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                  #condition);                                               \
          return 1;                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

static int resources_are_zero(void)
{
  return fake_lvgl_live_object_count() == 0 &&
         fake_lvgl_live_event_count() == 0 &&
         fake_lvgl_live_timer_count() == 0;
}

typedef struct
{
  smart_band_watch_face_id_t id;
  const char *name;
  const char *key_texts[4];
  size_t key_text_count;
} watch_face_expectation_t;

static const watch_face_expectation_t g_expected_faces[] =
{
  {
    SMART_BAND_WATCH_FACE_LOTUS,
    "Lotus",
    {"Sleep", "Heart Rate", "Stress", "Weather"},
    4
  },
  {
    SMART_BAND_WATCH_FACE_ACTIVITY,
    "Activity Rings",
    {"ACTIVITY", "MOVE", "STEPS", "HEART"},
    4
  },
  {
    SMART_BAND_WATCH_FACE_MINIMAL,
    "Minimal Digital",
    {"MINIMAL", "BATTERY", NULL, NULL},
    2
  }
};

static int test_registry_contract(void)
{
  const smart_band_watch_face_descriptor_t *descriptor;
  size_t count = smart_band_watch_face_registry_count();
  size_t index;
  size_t left;
  size_t right;

  CHECK(count == sizeof(g_expected_faces) / sizeof(g_expected_faces[0]));
  descriptor = smart_band_watch_face_registry_default();
  CHECK(descriptor != NULL);
  CHECK(descriptor == smart_band_watch_face_registry_at(0));
  CHECK(descriptor ==
        smart_band_watch_face_registry_find(SMART_BAND_WATCH_FACE_LOTUS));
  CHECK(smart_band_watch_face_registry_at(count) == NULL);
  CHECK(smart_band_watch_face_registry_at((size_t)-1) == NULL);
  CHECK(smart_band_watch_face_registry_find(
          (smart_band_watch_face_id_t)99) == NULL);

  for (index = 0; index < count; index++)
    {
      descriptor = smart_band_watch_face_registry_at(index);
      CHECK(descriptor != NULL);
      CHECK(descriptor ==
            smart_band_watch_face_registry_find(g_expected_faces[index].id));
      CHECK(descriptor->id == g_expected_faces[index].id);
      CHECK(strcmp(descriptor->name, g_expected_faces[index].name) == 0);
      CHECK(descriptor->context_size > 0);
      CHECK(descriptor->context_size <=
            SMART_BAND_WATCH_FACE_CONTEXT_CAPACITY);
      CHECK(descriptor->ops != NULL);
      CHECK(descriptor->ops->mount != NULL);
      CHECK(descriptor->ops->render != NULL);
      CHECK(descriptor->ops->set_visible != NULL);
      CHECK(descriptor->ops->root != NULL);
      CHECK(descriptor->ops->unmount != NULL);
    }

  for (left = 0; left < count; left++)
    {
      for (right = left + 1; right < count; right++)
        {
          CHECK(smart_band_watch_face_registry_at(left)->id !=
                smart_band_watch_face_registry_at(right)->id);
          CHECK(strcmp(smart_band_watch_face_registry_at(left)->name,
                       smart_band_watch_face_registry_at(right)->name) != 0);
          CHECK(smart_band_watch_face_registry_at(left)->ops !=
                smart_band_watch_face_registry_at(right)->ops);
        }
    }

  return 0;
}

static int test_lifecycle_guards(void)
{
  smart_band_watch_face_instance_t instance;
  smart_band_watch_face_config_t config = {320, 480, true};
  smart_band_watch_face_config_t invalid_config = {0, 480, true};
  const smart_band_watch_face_descriptor_t *descriptor =
    smart_band_watch_face_registry_default();

  memset(&instance, 0, sizeof(instance));
  CHECK(smart_band_watch_face_mount(NULL, descriptor, lv_scr_act(),
                                    &config) != 0);
  CHECK(smart_band_watch_face_mount(&instance, NULL, lv_scr_act(),
                                    &config) != 0);
  CHECK(smart_band_watch_face_mount(&instance, descriptor, NULL,
                                    &config) != 0);
  CHECK(smart_band_watch_face_mount(&instance, descriptor, lv_scr_act(),
                                    NULL) != 0);
  CHECK(smart_band_watch_face_mount(&instance, descriptor, lv_scr_act(),
                                    &invalid_config) != 0);
  CHECK(descriptor->ops->mount(NULL, lv_scr_act(), &config) != 0);
  CHECK(descriptor->ops->mount(instance.storage.bytes, NULL, &config) != 0);
  CHECK(descriptor->ops->mount(instance.storage.bytes, lv_scr_act(), NULL) !=
        0);
  descriptor->ops->render(NULL, NULL);
  descriptor->ops->set_visible(NULL, false);
  CHECK(descriptor->ops->root(NULL) == NULL);
  descriptor->ops->unmount(NULL);
  smart_band_watch_face_render(NULL, NULL);
  smart_band_watch_face_render(&instance, NULL);
  smart_band_watch_face_set_visible(NULL, false);
  smart_band_watch_face_set_visible(&instance, false);
  CHECK(smart_band_watch_face_root(NULL) == NULL);
  CHECK(smart_band_watch_face_root(&instance) == NULL);
  smart_band_watch_face_unmount(NULL);
  smart_band_watch_face_unmount(&instance);
  CHECK(resources_are_zero());
  return 0;
}

static int run_face_failure_sweep(const watch_face_expectation_t *expected,
                                  bool compact_band)
{
  smart_band_watch_face_instance_t instance;
  smart_band_watch_face_config_t config =
    {compact_band ? 320 : 345, compact_band ? 480 : 649, compact_band};
  const smart_band_watch_face_descriptor_t *descriptor =
    smart_band_watch_face_registry_find(expected->id);
  smart_band_state_t model;
  size_t create_count;
  size_t failure;
  size_t text_index;
  lv_obj_t *page;

  fake_lvgl_reset();
  memset(&instance, 0, sizeof(instance));
  CHECK(descriptor != NULL);
  lv_obj_set_size(lv_scr_act(), config.screen_width, config.screen_height);
  CHECK(smart_band_watch_face_mount(&instance, descriptor, lv_scr_act(),
                                    &config) == 0);
  create_count = fake_lvgl_object_create_attempts();
  CHECK(create_count > 0);
  CHECK(fake_lvgl_live_object_count() == create_count);
  page = lv_obj_get_child(lv_scr_act(), 0);
  CHECK(page != NULL);
  CHECK(smart_band_watch_face_root(&instance) == page);
  smart_band_watch_face_set_visible(&instance, false);
  CHECK(fake_lvgl_obj_has_flag(page, LV_OBJ_FLAG_HIDDEN));
  smart_band_watch_face_set_visible(&instance, true);
  CHECK(!fake_lvgl_obj_has_flag(page, LV_OBJ_FLAG_HIDDEN));
  smart_band_state_init(&model, 0);
  smart_band_watch_face_render(&instance, &model);
  for (text_index = 0; text_index < expected->key_text_count; text_index++)
    {
      CHECK(fake_lvgl_find_text(page, expected->key_texts[text_index], 0) !=
            NULL);
    }

  smart_band_watch_face_unmount(&instance);
  CHECK(!instance.mounted && instance.descriptor == NULL);
  CHECK(smart_band_watch_face_root(&instance) == NULL);
  CHECK(resources_are_zero());

  for (failure = 1; failure <= create_count; failure++)
    {
      fake_lvgl_fail_object_create_at(failure);
      CHECK(smart_band_watch_face_mount(&instance, descriptor, lv_scr_act(),
                                        &config) != 0);
      CHECK(resources_are_zero());
      CHECK(!instance.mounted && instance.descriptor == NULL);

      fake_lvgl_fail_object_create_at(0);
      CHECK(smart_band_watch_face_mount(&instance, descriptor, lv_scr_act(),
                                        &config) == 0);
      smart_band_watch_face_render(&instance, &model);
      CHECK(smart_band_watch_face_root(&instance) != NULL);
      smart_band_watch_face_unmount(&instance);
      CHECK(resources_are_zero());
    }

  return 0;
}

int main(void)
{
  size_t index;

  CHECK(test_registry_contract() == 0);
  CHECK(test_lifecycle_guards() == 0);
  for (index = 0;
       index < sizeof(g_expected_faces) / sizeof(g_expected_faces[0]);
       index++)
    {
      CHECK(run_face_failure_sweep(&g_expected_faces[index], true) == 0);
      CHECK(run_face_failure_sweep(&g_expected_faces[index], false) == 0);
    }

  return 0;
}
