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
  set(CSRCS watch_model.c sensor_bridge.c app_lvgl.c)

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
