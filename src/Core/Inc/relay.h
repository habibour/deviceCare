#ifndef INC_RELAY_H_
#define INC_RELAY_H_
#include <stdint.h>
void    relay_init(void);
void    relay_connect(void);
void    relay_disconnect(void);
uint8_t relay_is_connected(void);
#endif
