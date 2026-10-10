#ifndef INC_SETTINGS_H_
#define INC_SETTINGS_H_
/* Protection thresholds shared by the UART interface and the on-device UI.
   Setters return 1 if accepted, 0 if rejected (out of range / UV >= OV). */
float settings_get_ov(void);
float settings_get_uv(void);
float settings_get_oc(void);
int   settings_set_ov(float v);
int   settings_set_uv(float v);
int   settings_set_oc(float v);
#endif
