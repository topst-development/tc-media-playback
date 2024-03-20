/****************************************************************************************
 *   FileName    : DBusMsgDef.h
 *   Description : DBus Message Define Header
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
#ifndef DBUS_MSG_DEF_H
#define DBUS_MSG_DEF_H

/*================================================ MEDIAPLAYBACK DAEMON DEFINE START ================================================*/

#define MEDIAPLAYBACK_PROCESS_DBUS_NAME					"telechips.mediaplayback.process"
#define MEDIAPLAYBACK_PROCESS_OBJECT_PATH				"/telechips/mediaplayback/process"

#define MEDIAPLAYBACK_EVENT_INTERFACE					"mediaplayback.event"

/*=========================== MEDIAPLAYBACK SIGNAL EVENT DEFINES ============================*/
#define SIGNAL_MEDIAPLAYBACK_PLAYING					"signal_mediaplayback_playing"
#define SIGNAL_MEDIAPLAYBACK_STOPPED					"signal_mediaplayback_stopped"
#define SIGNAL_MEDIAPLAYBACK_PAUSED					"signal_mediaplayback_paused"
#define SIGNAL_MEDIAPLAYBACK_DURATION				"signal_mediaplayback_duration"
#define SIGNAL_MEDIAPLAYBACK_PLAYPOSITION			"signal_mediaplayback_playpostion"
#define SIGNAL_MEDIAPLAYBACK_TAGINFO					"signal_mediaplayback_taginfo"
#define SIGNAL_MEDIAPLAYBACK_ALBUMART_KEY			"signal_mediaplayback_albumart_key"
#define SIGNAL_MEDIAPLAYBACK_ALBUMART_COMPLETED	"signal_mediaplayback_albumart_completed"
#define SIGNAL_MEDIAPLAYBACK_PLAY_ENDED				"signal_mediaplayback_play_ended"
#define SIGNAL_MEDIAPLAYBACK_SEEK_COMPLETED			"signal_mediaplayback_seek_completed"
#define SIGNAL_MEDIAPLAYBACK_ERROR					"signal_mediaplayback_error"
#define SIGNAL_MEDIAPLAYBACK_SAMPLERATE				"signal_mediaplayback_samplerate"

typedef enum {
	SignalMediaPlaybackPlaying,
	SignalMediaPlaybackStopped,
	SignalMediaPlaybackPaused,
	SignalMediaPlaybackDuration,
	SignalMediaPlaybackPlayPostion,
	SignalMediaPlaybackTagInfo,
	SignalMediaPlaybackAlbumArtKey,
	SignalMediaPlaybackAlbumArtCompleted,
	SignalMediaPlaybackPlayEnded,
	SignalMediaPlaybackSeekCompleted,
	SignalMediaPlaybackError,
	SignalMediaPlaybackSamplerate,
	TotalSignalMediaPlaybackEvents
} SignalMediaPlaybackEvent;
extern const char *g_signalMediaPlaybackEventNames[TotalSignalMediaPlaybackEvents];

/*=========================== MEDIAPLAYBACK METHOD EVENT DEFINES ============================*/
#define METHOD_MEDIAPLAYBACK_PLAY_START				"method_mediaplayback_play_start"
#define METHOD_MEDIAPLAYBACK_PLAY_STOP				"method_mediaplayback_play_stop"
#define METHOD_MEDIAPLAYBACK_PLAY_PAUSE				"method_mediaplayback_play_pause"
#define METHOD_MEDIAPLAYBACK_PLAY_RESUME			"method_mediaplayback_play_resume"
#define METHOD_MEDIAPLAYBACK_PLAY_SEEK				"method_mediaplayback_play_seek"
#define METHOD_MEDIAPLAYBACK_SET_DISPLAY			"method_mediaplayback_set_display"
#define METHOD_MEDIAPLAYBACK_SET_DUAL_DISPLAY		"method_mediaplayback_set_dual_display"
#define METHOD_MEDIAPLAYBACK_SET_DEBUG				"method_mediaplayback_set_debug"
#define METHOD_MEDIAPLAYBACK_GET_STATUS				"method_mediaplayback_get_status"
#define METHOD_MEDIAPLAYBACK_GET_ALBUMART_KEY		"method_mediaplayback_get_albumart_key"
#define METHOD_MEDIAPLAYBACK_GET_PLAY_ID			"method_mediaplayback_get_play_id"

typedef enum {
	MethodMediaPlaybackPlayStart,
	MethodMediaPlaybackPlayStop,
	MethodMediaPlaybackPlayPause,
	MethodMediaPlaybackPlayResume,
	MethodMediaPlaybackPlaySeek,
	MethodMediaPlaybackSetDisplay,
	MethodMediaPlaybackSetDualDisplay,
	MethodMediaPlaybackSetDebug,
	MethodMediaPlaybackGetStatus,
	MethodMediaPlaybackGetAlbumArtKey,
	MethodMediaPlaybackGetPlayID,
	TotalMethodMediaPlaybackEvents
} MethodMediaPlaybackEvent;
extern const char *g_methodMediaPlaybackEventNames[TotalMethodMediaPlaybackEvents];

/*================================================ MEDIAPLAYBACK DAEMON DEFINE END ================================================*/

#endif

