#ifndef SMART_BAND_UI_LVGL_WATCH_PAGES_H
#define SMART_BAND_UI_LVGL_WATCH_PAGES_H

#include "components.h"

typedef struct
{
  lv_obj_t *heart_page;
  lv_obj_t *heart_date;
  lv_obj_t *heart_value;
  lv_obj_t *heart_progress;
  lv_obj_t *heart_status;
  lv_obj_t *heart_battery;
  lv_obj_t *heart_source;
  lv_obj_t *heart_stress;
  lv_obj_t *steps_page;
  lv_obj_t *steps_date;
  lv_obj_t *steps_value;
  lv_obj_t *steps_progress;
  lv_obj_t *steps_goal;
  lv_obj_t *steps_percent;
  lv_obj_t *steps_source;
  lv_obj_t *steps_weather;
  lv_obj_t *steps_goal_down;
  lv_obj_t *steps_goal_up;
} smart_band_watch_pages_t;

int smart_band_watch_page_build_header(lv_obj_t *parent,
                                       const smart_band_ui_components_t *ui,
                                       lv_obj_t **date_label,
                                       lv_coord_t leaf_y,
                                       lv_coord_t date_y);
int smart_band_watch_pages_build_heart(smart_band_watch_pages_t *pages,
                                       lv_obj_t *parent,
                                       const smart_band_ui_components_t *ui);
int smart_band_watch_pages_build_steps(smart_band_watch_pages_t *pages,
                                       lv_obj_t *parent,
                                       const smart_band_ui_components_t *ui,
                                       lv_event_cb_t step_goal_cb);
void smart_band_watch_pages_render_heart(smart_band_watch_pages_t *pages,
                                         const smart_band_state_t *model);
void smart_band_watch_pages_render_steps(smart_band_watch_pages_t *pages,
                                         const smart_band_state_t *model);

#endif
