include $(APPDIR)/Make.defs

PROGNAME = smart_band
PRIORITY = $(CONFIG_LVX_DEMO_SMART_BAND_BASIC_PRIORITY)
STACKSIZE = $(CONFIG_LVX_DEMO_SMART_BAND_BASIC_STACKSIZE)
MODULE = $(CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC)

CSRCS = watch_model.c sensor_bridge.c app_lvgl.c icon_assets.c smart_band_apps.c
CSRCS += services/event_queue.c services/event_inbox.c services/clock.c
CSRCS += services/capabilities.c services/runtime.c platform/platform_noop.c
CSRCS += platform/event_mutex.c
CSRCS += services/storage_codec.c services/store.c services/storage_transaction.c
CSRCS += services/history_service.c services/workout_service.c
CSRCS += services/notification_service.c
CSRCS += services/power_manager.c
CSRCS += services/sync_protocol.c
CSRCS += platform/loopback/sync_loopback.c
CSRCS += platform/storage/storage_memory.c platform/storage/storage_file.c
CSRCS += platform/storage/storage_fault.c
CSRCS += logic/calculator_model.c logic/game_2048_model.c logic/mines_model.c
CSRCS += logic/step_normalizer.c logic/workout_model.c
CSRCS += logic/notification_model.c logic/notification_demo.c
CSRCS += logic/power_policy.c
CSRCS += logic/watch_face_settings.c
CSRCS += ui/lvgl/components.c ui/lvgl/watch_face_picker.c
CSRCS += ui/lvgl/watch_pages.c
CSRCS += ui/lvgl/workout_view.c ui/lvgl/history_view.c
CSRCS += ui/lvgl/notification_view.c
CSRCS += ui/lvgl/faces/activity_face.c ui/lvgl/faces/lotus_face.c
CSRCS += ui/lvgl/faces/minimal_face.c ui/lvgl/faces/watch_face_registry.c
CSRCS += apps/weather_app.c apps/calculator_app.c apps/timer_app.c
CSRCS += apps/game_2048_app.c apps/stopwatch_app.c
CSRCS += apps/mines_app.c apps/tetris_app.c
CSRCS += apps/wooden_fish_app.c
MAINSRC = smart_band_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/smart_band_basic/include

include $(APPDIR)/Application.mk
