include $(APPDIR)/Make.defs

PROGNAME = smart_band
PRIORITY = $(CONFIG_LVX_DEMO_SMART_BAND_BASIC_PRIORITY)
STACKSIZE = $(CONFIG_LVX_DEMO_SMART_BAND_BASIC_STACKSIZE)
MODULE = $(CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC)

CSRCS = watch_model.c sensor_bridge.c app_lvgl.c
MAINSRC = smart_band_main.c

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/packages/demos/smart_band_basic/include

include $(APPDIR)/Application.mk
