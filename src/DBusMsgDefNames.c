/****************************************************************************************
 *   FileName    : DBusMsgDefNames.c
 *   Description : Telechips DBUS Message Define Names
 ****************************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited 
to re-distribution in source or binary form is strictly prohibited.
This source code is provided ¡°AS IS¡± and nothing contained in this source code 
shall constitute any express or implied warranty of any kind, including without limitation, 
any warranty of merchantability, fitness for a particular purpose or non-infringement of any patent, 
copyright or other third party intellectual property right. 
No warranty is made, express or implied, regarding the information¡¯s accuracy, 
completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability arising from, 
out of or in connection with this source code or the use in the source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement 
between Telechips and Company.
*
****************************************************************************************/
#include "DBusMsgDef.h"

const char *g_signalMediaPlaybackEventNames[TotalSignalMediaPlaybackEvents] = {
	SIGNAL_MEDIAPLAYBACK_PLAYING,
	SIGNAL_MEDIAPLAYBACK_STOPPED,
	SIGNAL_MEDIAPLAYBACK_PAUSED,
	SIGNAL_MEDIAPLAYBACK_DURATION,
	SIGNAL_MEDIAPLAYBACK_PLAYPOSITION,
	SIGNAL_MEDIAPLAYBACK_TAGINFO,
	SIGNAL_MEDIAPLAYBACK_ALBUMART_KEY,
	SIGNAL_MEDIAPLAYBACK_ALBUMART_COMPLETED,
	SIGNAL_MEDIAPLAYBACK_PLAY_ENDED,
	SIGNAL_MEDIAPLAYBACK_SEEK_COMPLETED,
	SIGNAL_MEDIAPLAYBACK_ERROR,
	SIGNAL_MEDIAPLAYBACK_SAMPLERATE,
};

const char *g_methodMediaPlaybackEventNames[TotalMethodMediaPlaybackEvents] = {
	METHOD_MEDIAPLAYBACK_PLAY_START,
	METHOD_MEDIAPLAYBACK_PLAY_STOP,
	METHOD_MEDIAPLAYBACK_PLAY_PAUSE,
	METHOD_MEDIAPLAYBACK_PLAY_RESUME,
	METHOD_MEDIAPLAYBACK_PLAY_SEEK,
	METHOD_MEDIAPLAYBACK_SET_DISPLAY,
	METHOD_MEDIAPLAYBACK_SET_DUAL_DISPLAY,
	METHOD_MEDIAPLAYBACK_SET_DEBUG,
	METHOD_MEDIAPLAYBACK_GET_STATUS,
	METHOD_MEDIAPLAYBACK_GET_ALBUMART_KEY,
	METHOD_MEDIAPLAYBACK_GET_PLAY_ID,
};

/* End of file */

