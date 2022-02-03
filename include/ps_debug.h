/*****************************************************
 *
 *	LED matrix driver for Raspberry RP2040
 *	(c) Peter Schulten, Mülheim, Germany
 *	peter_(at)_pitschu.de
 *
 *  Unmodified reproduction and distribution of this entire
 *  source code in any form is permitted provided the above
 *  notice is preserved.
 *  I make this source code available free of charge and therefore
 *  offer neither support nor guarantee for its functionality.
 *  Furthermore, I assume no liability for the consequences of
 *  its use.
 *  The source code may only be used and modified for private,
 *  non-commercial purposes. Any further use requires my consent.
 *
 *	History
 *	25.01.2022	pitschu		Start of work
 */

#ifndef INCLUDE_PS_DEBUG_H_
#define INCLUDE_PS_DEBUG_H_
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "FreeRTOS.h"

// color codes from:
#define _ANSI_NONE_
#define _ANSI_RED_		"\x1b[0;31m"
#define _ANSI_GREEN_	"\x1b[0;32m"
#define _ANSI_YELLOW_	"\x1b[0;33m"
#define _ANSI_BLUE_		"\x1b[0;34m"
#define _ANSI_MAGENTA_	"\x1b[0;35m"
#define _ANSI_CYAN_		"\x1b[0;36m"
#define _ANSI_WHITE_	"\x1b[0;37m"

#define _ANSI_BLACK_	"\x1b[1;30m"
#define _ANSI_LRED_		"\x1b[1;31m"
#define _ANSI_LGREEN_	"\x1b[1;32m"
#define _ANSI_BROWN_	"\x1b[1;33m"
#define _ANSI_LBLUE_	"\x1b[1;34m"
#define _ANSI_LPURPLE_	"\x1b[1;35m"
#define _ANSI_LCYAN_	"\x1b[1;36m"
#define _ANSI_GREY_		"\x1b[1;37m"

#define _ALERT_RED_		"\x1b[1;37;41m"
#define _ALERT_OFF_		"\x1b[0m"

extern char				logTimeBuf[32];

#define		PRT_LEVEL	1
#define		DEB_LEVEL	2
#define		TRC_LEVEL	3


#ifndef NO_DEBUG
#define NO_DEBUG 0
#endif

#if NO_DEBUG == 1
#undef DEBUG
#define DEBUG 		1
#endif

#ifndef DEBUG
#define DEBUG		2
#endif
#ifndef DEBUG_COLOR
#define DEBUG_COLOR	_ANSI_GREEN_
#endif


extern int dummyPrintf(char *frm, ...);

#if DEBUG > 0
#define PRT_DEBUG(a,...)		printf(DEBUG_COLOR"%s [%d]:%5d:%5d ", logTimeBuf, get_core_num(), xPortGetFreeHeapSize(), (int)uxTaskGetStackHighWaterMark(NULL)); printf((a), ##__VA_ARGS__)
#define ALERT_DEBUG(a,...)		printf(_ALERT_RED_"%s [%d]:%5d:%5d "_ALERT_OFF_ DEBUG_COLOR, logTimeBuf, get_core_num(), xPortGetFreeHeapSize(), (int)uxTaskGetStackHighWaterMark(NULL)); printf((a), ##__VA_ARGS__)
#define PRT_POS()				printf(DEBUG_COLOR"--> line %4d\n", (int)__LINE__)
#else
#define PRT_DEBUG(a,...)
#define ALERT_DEBUG(a,...)
#endif

#if DEBUG > 1
#define LOG_DEBUG(a,...)		{printf(DEBUG_COLOR"%s [%d]:%5d:%5d ", logTimeBuf, get_core_num(), xPortGetFreeHeapSize(), (int)uxTaskGetStackHighWaterMark(NULL)); printf((a), ##__VA_ARGS__);}
#define LOG_ASSERT(e)       ((e) ? (void)0 : LOG_DEBUG("%s()%d): %s\n", __FILE__, __LINE__, #e))
#else
#define LOG_DEBUG(a,...)		// {dummyPrintf((a), ##__VA_ARGS__);}
#define LOG_ASSERT(e)
#endif

#if DEBUG > 2
#define TRC_DEBUG(a,...)		printf(DEBUG_COLOR"%s %5d:%5d ", logTimeBuf, xPortGetFreeHeapSize(), (int)uxTaskGetStackHighWaterMark(NULL)); printf((a), ##__VA_ARGS__)
#else
#define TRC_DEBUG(a,...)		// {dummyPrintf((a), ##__VA_ARGS__);}
#endif



#endif /* INCLUDE_PS_DEBUG_H_ */
