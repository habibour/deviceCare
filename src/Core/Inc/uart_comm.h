#ifndef INC_UART_COMM_H_
#define INC_UART_COMM_H_
#include <stdint.h>
void    uart_comm_init(void);
void    uart_comm_process(void);
void    uart_comm_rx_isr(void);
void    uart_comm_update_readings(float v, float a);
uint8_t uart_reset_requested(void);
void    uart_clear_reset(void);
#endif
