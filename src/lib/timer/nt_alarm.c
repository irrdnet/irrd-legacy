#ifndef MRT_WIN_NT_ALARM_H_
#define MRT_WIN_NT_ALARM_H_
#define _MT        // neeed in win nt for multithreaded 


#include <process.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <windows.h>
#include <tchar.h>

/*******************************************************************
Windows does not support SIGALRM.  SIGTERM exists but is never used
therefore we typedef SIGTERM to be SIGALRM and then raise a SIGALRM
whenever the timer fires, signal.c has an callback routine that is
called when we get a SIGALRM signal
*******************************************************************/

#ifndef SIGALRM
#define SIGALRM		SIGTERM
#endif

static UINT Timer_result;
static BOOL alarm_on = FALSE;

VOID CALLBACK Alarm_callback__(HWND hWnd, UINT uMsg, UINT idEven, DWORD dwTime)
{

		int n;
	alarm_interrupt ();
	//return;

	//if (alarm_on == TRUE)
		//raise(SIGALRM);
	if ((n = KillTimer(NULL, Timer_result)) == 0) {
		printf("KillTimer failed in mrt_win_NT_alarm.h  (%d) %d  %s\n", Timer_result, GetLastError(), strerror("fail"));
		//exit(0);
	}

	return;
}

unsigned int alarm(unsigned int seconds)
{
	UINT idEvent = 0;
	DWORD ret = 0;
	
	//assert(seconds > 0);
	
	if (seconds <= 0)
	{
		alarm_on = FALSE;
		return ret;
	}
	
	alarm_on = TRUE;

	Timer_result = SetTimer(NULL, 0, seconds * 1000 /*# of mSec*/, (TIMERPROC )Alarm_callback__);
	
	// Timer creation has failed
	assert (0 != Timer_result);
	//{
	//	printf("Timer creation has failed\n");
		//exit(0);
	//}

	return ret;

}

#endif
