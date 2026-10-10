#ifndef INC_UI_H_
#define INC_UI_H_
void ui_init(void);
void ui_set_readings(float v, float a);
void ui_poll(void);   /* call every main-loop pass */
#endif
