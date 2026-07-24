# ##############################################################################
# packages/demos/smart_band_basic/CMakeLists.txt
#
# Build entry for the smart band basic LVGL demo.
# ##############################################################################

if(CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
  set(PROGNAME smart_band)
  set(PRIORITY ${CONFIG_LVX_DEMO_SMART_BAND_BASIC_PRIORITY})
  set(STACKSIZE ${CONFIG_LVX_DEMO_SMART_BAND_BASIC_STACKSIZE})
  set(MODULE ${CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC})
  set(MAINSRC smart_band_main.c)
  set(CSRCS
    watch_model.c
    logic/calculator_model.c
    logic/game_2048_model.c
    logic/mines_model.c
    logic/step_normalizer.c
    logic/workout_model.c
    logic/notification_model.c
    logic/notification_demo.c
    logic/power_policy.c
    logic/watch_face_settings.c
    sensor_bridge.c
    services/event_queue.c
    services/event_inbox.c
    services/clock.c
    services/capabilities.c
    services/runtime.c
    services/storage_codec.c
    services/store.c
    services/storage_transaction.c
    services/history_service.c
    services/workout_service.c
    services/notification_service.c
    services/sync_protocol.c
    platform/platform_noop.c
    platform/event_mutex.c
    platform/loopback/sync_loopback.c
    platform/storage/storage_memory.c
    platform/storage/storage_file.c
    platform/storage/storage_fault.c
    app_lvgl.c
    ui/lvgl/components.c
    ui/lvgl/watch_face_picker.c
    ui/lvgl/faces/activity_face.c
    ui/lvgl/faces/lotus_face.c
    ui/lvgl/faces/minimal_face.c
    ui/lvgl/faces/watch_face_registry.c
    ui/lvgl/watch_pages.c
    ui/lvgl/workout_view.c
    ui/lvgl/history_view.c
    ui/lvgl/notification_view.c
    icon_assets.c
    smart_band_apps.c
    apps/weather_app.c
    apps/calculator_app.c
    apps/timer_app.c
    apps/game_2048_app.c
    apps/stopwatch_app.c
    apps/mines_app.c
    apps/tetris_app.c
    apps/wooden_fish_app.c)

  nuttx_add_application(
    NAME
    ${PROGNAME}
    PRIORITY
    ${PRIORITY}
    STACKSIZE
    ${STACKSIZE}
    MODULE
    ${MODULE}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    SRCS
    ${MAINSRC}
    ${CSRCS}
  )
endif()
