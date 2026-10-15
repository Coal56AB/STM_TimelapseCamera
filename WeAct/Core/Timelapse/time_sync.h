#ifndef TIME_SYNC_H
#define TIME_SYNC_H
#include "clock.h"
/* Обрабатывается основным циклом; ответ не длиннее 31 символа + NUL. */
void TimeSync_Command(Clock_t *clock, const char *request, char response[32], uint32_t now);
#endif
