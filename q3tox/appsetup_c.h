#ifndef APPSETUP_C_H
#define APPSETUP_C_H

#ifdef __cplusplus
extern "C" {
#endif

void qtapp_onExit(void (*callback)());
int  qtapp_addTimer(unsigned int intervalMs, void (*callback)());
void qtapp_removeTimer(int timerId);

#ifdef __cplusplus
}
#endif

#endif
