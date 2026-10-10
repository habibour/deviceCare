#ifndef INC_FSM_H_
#define INC_FSM_H_
#include <stdint.h>
typedef enum { STATE_MONITORING=0, STATE_DISCONNECTED, STATE_COOLDOWN } FSM_State;
typedef enum { FAULT_NONE=0, FAULT_OVERVOLTAGE, FAULT_UNDERVOLTAGE, FAULT_OVERCURRENT } Fault_Cause;
void        fsm_init(void);
void        fsm_update(float v, float a, float ov, float uv, float oc, uint8_t reset_req);
FSM_State   fsm_get_state(void);
Fault_Cause fsm_get_fault(void);
const char *fsm_state_str(void);
const char *fsm_fault_str(void);
#endif
