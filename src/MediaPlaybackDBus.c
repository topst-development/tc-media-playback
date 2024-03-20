/****************************************************************************************
 *   FileName    : MediaPlaybackDBus.c
 *   Description : Telechips Media Playback D-BUS
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <glib.h>
#include <sys/shm.h>
#include "DBusMsgDef.h"
#include "TCDBusRawAPI.h"
#include "TCLog.h"
#include "TCMultiMediaType.h"
#include "MediaPlaybackDBus.h"
#include "MultiMediaManager.h"

typedef void (*DBusMethodCallFunction)(DBusMessage *message);
static DBusMsgErrorCode OnReceivedMethodCall(DBusMessage *message, const char *interface);
static void MediaPlaybackDBusEmitSignal(uint32_t signalID,int32_t value, int32_t playID);
//static void MediaPlaybackDBusEmitSignal_NoValueType(uint32_t signalID);
static void MediaPlaybackDBusEmitSignal_time(uint32_t signalID, uint8_t hour, uint8_t min, uint8_t sec, int32_t playID);
static void MediaPlaybackDBusEmitSignal_Mem(uint32_t signalID, int32_t key, uint32_t size);
static void MediaPlaybackDBusEmitSignal_playID(uint32_t signalID, int32_t playID);
static void DBusMethodPlayStart(DBusMessage *message);
static void DBusMethodPlayStop(DBusMessage *message);
static void DBusMethodPlayPause(DBusMessage *message);
static void DBusMethodPlayResume(DBusMessage *message);
static void DBusMethodPlaySeek(DBusMessage *message);
static void DBusMethodSetDisplay(DBusMessage *message);
static void DBusMethodSetDualDisplay(DBusMessage *message);
static void DBusMethodSetDebug(DBusMessage *message);
static void DBusMethodGetStatus(DBusMessage *message);
static void DBusMethodGetAlbumArtKey(DBusMessage *message);
static void DBusMethodGetPlayID(DBusMessage *message);

static DBusMethodCallFunction s_DBusMethodProcess[TotalMethodMediaPlaybackEvents] = {
	DBusMethodPlayStart,
	DBusMethodPlayStop,
	DBusMethodPlayPause,
	DBusMethodPlayResume,
	DBusMethodPlaySeek,
	DBusMethodSetDisplay,
	DBusMethodSetDualDisplay,
	DBusMethodSetDebug,
	DBusMethodGetStatus,
	DBusMethodGetAlbumArtKey,
	DBusMethodGetPlayID
};
void MediaPlaybackDBusInitialize(void)
{
	INFO_PRINTF("\n");
	SetDBusPrimaryOwner(MEDIAPLAYBACK_PROCESS_DBUS_NAME);
	SetCallBackFunctions(NULL, OnReceivedMethodCall);
	(void)AddMethodInterface(MEDIAPLAYBACK_EVENT_INTERFACE);
	InitializeRawDBusConnection("MEDIAPLABYBACK DBUS");
}
void MediaPlaybackDBusRelease(void)
{
	ReleaseRawDBusConnection();
}
void MediaPlaybackEmitPlaying(int32_t playID)
{
	MediaPlaybackDBusEmitSignal_playID((uint32_t)SignalMediaPlaybackPlaying, playID);
}
void MediaPlaybackEmitStopped(int32_t playID)
{
	MediaPlaybackDBusEmitSignal_playID((uint32_t)SignalMediaPlaybackStopped, playID);
}
void MediaPlaybackEmitPaused(int32_t playID)
{
	MediaPlaybackDBusEmitSignal_playID((uint32_t)SignalMediaPlaybackPaused,playID);
}
void MediaPlaybackEmitDuration(uint32_t hour, uint32_t min, uint32_t sec,int32_t playID)
{
	MediaPlaybackDBusEmitSignal_time((uint32_t)SignalMediaPlaybackDuration, (uint8_t)hour, (uint8_t)min, (uint8_t)sec, playID);
}
void MediaPlaybackEmitPlayPosition(uint32_t hour, uint32_t min, uint32_t sec,int32_t playID)
{
	MediaPlaybackDBusEmitSignal_time((uint32_t)SignalMediaPlaybackPlayPostion, (uint8_t)hour, (uint8_t)min, (uint8_t)sec, playID);
}
void MediaPlaybackEmitPlayTaginfo(MetaCategory category, const char *info,int32_t playID)
{
	INFO_PRINTF(" \n");
	if ((category < TotalMetaCategories)&&(info!= NULL))
	{
		DBusMessage *message;
		message = CreateDBusMsgSignal(MEDIAPLAYBACK_PROCESS_OBJECT_PATH, MEDIAPLAYBACK_EVENT_INTERFACE,
									SIGNAL_MEDIAPLAYBACK_TAGINFO,
									DBUS_TYPE_INT32, &category,
									DBUS_TYPE_STRING, &info,
									DBUS_TYPE_INT32, &playID,
									DBUS_TYPE_INVALID);
		
		if (message != NULL)
		{
			if (SendDBusMessage(message, NULL))
			{
				INFO_PRINTF("EMIT SIGNAL(%s), category (%d), info(%s), playID(%d)\n",
											 SIGNAL_MEDIAPLAYBACK_TAGINFO, category,info, playID);
			}
			else
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
	
			dbus_message_unref(message);
		}
		else
		{
			ERROR_PRINTF("CreateDBusMsgSignal failed\n");
		}
	}
}
void MediaPlaybackEmitAlbumart(int32_t playID, uint32_t length)
{
	MediaPlaybackDBusEmitSignal_Mem((uint32_t)SignalMediaPlaybackAlbumArtCompleted, playID,  length);
}
void MediaPlaybackEmitPlayEnded(int32_t playID)
{
	MediaPlaybackDBusEmitSignal_playID((uint32_t)SignalMediaPlaybackPlayEnded, playID);
}
void MediaPlaybackEmitSeekCompleted(uint8_t hour, uint8_t min, uint8_t sec, int32_t playID)
{
	MediaPlaybackDBusEmitSignal_time((uint32_t)SignalMediaPlaybackSeekCompleted, hour, min, sec, playID);
}
void MediaPlaybackEmitError(int32_t errCode,int32_t playID)
{
	DEBUG_PRINTF("\n");

	DBusMessage *message;
	message = CreateDBusMsgSignal(MEDIAPLAYBACK_PROCESS_OBJECT_PATH, MEDIAPLAYBACK_EVENT_INTERFACE,
								  SIGNAL_MEDIAPLAYBACK_ERROR,
								  DBUS_TYPE_INT32, &errCode,
								  DBUS_TYPE_INT32, &playID,
								  DBUS_TYPE_INVALID);
	if (message != NULL)
	{
		if (SendDBusMessage(message, NULL))
		{
			INFO_PRINTF("EMIT SIGNAL(%s), errCode(%d), playID(%d)\n",
										 MEDIAPLAYBACK_PROCESS_OBJECT_PATH, errCode, playID);
		}
		else
		{
			ERROR_PRINTF("SendDBusMessage failed\n");
		}
		dbus_message_unref(message);
	}
	else
	{
		ERROR_PRINTF("CreateDBusMsgSignal failed\n");
	}
}

void MediaPlaybackEmitSamplerate(int32_t samplerate, int32_t playID)
{
	MediaPlaybackDBusEmitSignal((uint32_t)SignalMediaPlaybackSamplerate, samplerate, playID);
}


static void MediaPlaybackDBusEmitSignal(uint32_t signalID,int32_t value, int32_t playID)
{
	DEBUG_PRINTF("\n");
	if (signalID < (uint32_t)TotalSignalMediaPlaybackEvents)
	{
		DBusMessage *message;
		message = CreateDBusMsgSignal(MEDIAPLAYBACK_PROCESS_OBJECT_PATH, MEDIAPLAYBACK_EVENT_INTERFACE,
									  g_signalMediaPlaybackEventNames[signalID],
									  DBUS_TYPE_INT32, &value,
									  DBUS_TYPE_INT32, &playID,
									  DBUS_TYPE_INVALID);
		if (message != NULL)
		{
			if (SendDBusMessage(message, NULL))
			{
				INFO_PRINTF("EMIT SIGNAL([%d]%s), value(%d)\n",
											 signalID, g_signalMediaPlaybackEventNames[signalID], value);
			}
			else
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
			dbus_message_unref(message);
		}
		else
		{
			ERROR_PRINTF("CreateDBusMsgSignal failed\n");
		}
	}
	else
	{
		ERROR_PRINTF("signal emit failed.(id[%u])\n",signalID);
	}
}

//Not used yet.
#if 0
static void MediaPlaybackDBusEmitSignal_NoValueType(uint32_t signalID)
{
	DEBUG_PRINTF("\n");
	if( signalID < (uint32_t)TotalSignalMediaPlaybackEvents)
	{
		DBusMessage *message;
		message = CreateDBusMsgSignal(MEDIAPLAYBACK_PROCESS_OBJECT_PATH, MEDIAPLAYBACK_EVENT_INTERFACE,
									g_signalMediaPlaybackEventNames[signalID],
									DBUS_TYPE_INVALID);
		
		if (message != NULL)
		{
			if (SendDBusMessage(message, NULL))
			{
				INFO_PRINTF("EMIT SIGNAL([%d]%s)\n",
										 signalID, g_signalMediaPlaybackEventNames[signalID]);
			}
			else
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
	
			dbus_message_unref(message);
		}
		else
		{
			ERROR_PRINTF("CreateDBusMsgSignal failed\n");
		}
	}
	else
	{
		ERROR_PRINTF("signal emit failed.(id[%u])\n", signalID);
	}
}
#endif
static void MediaPlaybackDBusEmitSignal_time(uint32_t signalID, uint8_t hour, uint8_t min, uint8_t sec, int32_t playID)
{
	/* DEBUG_PRINTF(" \n"); */
	if( signalID < (uint32_t)TotalSignalMediaPlaybackEvents)
	{
		DBusMessage *message;
		message = CreateDBusMsgSignal(MEDIAPLAYBACK_PROCESS_OBJECT_PATH, MEDIAPLAYBACK_EVENT_INTERFACE,
									g_signalMediaPlaybackEventNames[signalID],
									DBUS_TYPE_BYTE, &hour,
									DBUS_TYPE_BYTE,  &min,
									DBUS_TYPE_BYTE, &sec,
									DBUS_TYPE_INT32, &playID,
									DBUS_TYPE_INVALID);
		
		if (message != NULL)
		{
			if (SendDBusMessage(message, NULL))
			{
				INFO_PRINTF("EMIT SIGNAL([%d]%s), hour(%d), min(%d), sec(%d)\n",
											 signalID, g_signalMediaPlaybackEventNames[signalID], hour, min, sec);
			}
			else
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
	
			dbus_message_unref(message);
		}
		else
		{
			ERROR_PRINTF("CreateDBusMsgSignal failed\n");
		}
	}
	else
	{
		ERROR_PRINTF("signal emit failed.(id[%u])\n", signalID);
	}
}
static void MediaPlaybackDBusEmitSignal_Mem(uint32_t signalID, int32_t playID, uint32_t size)
{
	DEBUG_PRINTF("\n");
	if (signalID < (uint32_t)TotalSignalMediaPlaybackEvents)
	{
		DBusMessage *message;
		message = CreateDBusMsgSignal(MEDIAPLAYBACK_PROCESS_OBJECT_PATH, MEDIAPLAYBACK_EVENT_INTERFACE,
									  g_signalMediaPlaybackEventNames[signalID],
									  DBUS_TYPE_INT32, &playID,
									  DBUS_TYPE_UINT32, &size,
									  DBUS_TYPE_INVALID);
		if (message != NULL)
		{
			if (SendDBusMessage(message, NULL))
			{
				INFO_PRINTF("EMIT SIGNAL([%d]%s), key(%d), size(%d)\n",
											 signalID, g_signalMediaPlaybackEventNames[signalID], playID, size);
			}
			else
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
			dbus_message_unref(message);
		}
		else
		{
			ERROR_PRINTF("CreateDBusMsgSignal failed\n");
		}
	}
	else
	{
		ERROR_PRINTF("signal emit failed.(id[%u])\n",playID);
	}
}

static void MediaPlaybackDBusEmitSignal_playID(uint32_t signalID, int32_t playID)
{
	DEBUG_PRINTF("\n");
	if (signalID < TotalSignalMediaPlaybackEvents)
	{
		DBusMessage *message;
		message = CreateDBusMsgSignal(MEDIAPLAYBACK_PROCESS_OBJECT_PATH, MEDIAPLAYBACK_EVENT_INTERFACE,
									  g_signalMediaPlaybackEventNames[signalID],
									  DBUS_TYPE_INT32, &playID,
									  DBUS_TYPE_INVALID);
		if (message != NULL)
		{
			if (SendDBusMessage(message, NULL))
			{
				INFO_PRINTF("EMIT SIGNAL([%d]%s), playID(%d)\n",
											 signalID, g_signalMediaPlaybackEventNames[signalID], playID);
			}
			else
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
			dbus_message_unref(message);
		}
		else
		{
			ERROR_PRINTF("CreateDBusMsgSignal failed\n");
		}
	}
	else
	{
		ERROR_PRINTF("signal emit failed.(id[%u])\n", signalID);
	}
}


static DBusMsgErrorCode OnReceivedMethodCall(DBusMessage *message, const char *interface)
{
	DBusMsgErrorCode error = ErrorCodeNoError;	
	DEBUG_PRINTF("\n");
	
	if ((interface != NULL) &&
		(strcmp(interface, MEDIAPLAYBACK_EVENT_INTERFACE) == 0))
	{
		uint32_t idx;
		uint32_t stop = 0;
		for (idx = (uint32_t)MethodMediaPlaybackPlayStart; (idx < (uint32_t)TotalMethodMediaPlaybackEvents) && (stop == 0); idx++)
		{
			if (dbus_message_is_method_call(message, 
											MEDIAPLAYBACK_EVENT_INTERFACE, 
											g_methodMediaPlaybackEventNames[idx]))
			{
				s_DBusMethodProcess[idx](message);
				stop = 1;
			}
		}
	}
	
	return error;
}

static void DBusMethodPlayStart(DBusMessage *message)
{
	DEBUG_PRINTF("\n");
	
	if (message != NULL)
	{
		char * path=NULL;
		uint8_t hour, min, sec;
		uint8_t isVideo;
		uint8_t keepPause;
		int32_t id;
		int32_t ret;
		int32_t currentPlayID;
		DBusMessage *returnMessage;
		if (GetArgumentFromDBusMessage(message, 
										DBUS_TYPE_STRING, &path,
										DBUS_TYPE_BYTE, &hour, 
										DBUS_TYPE_BYTE,  &min, 
										DBUS_TYPE_BYTE, &sec,
										DBUS_TYPE_BYTE, &isVideo,
										DBUS_TYPE_INT32, &id,
										DBUS_TYPE_BYTE, &keepPause,
										DBUS_TYPE_INVALID))
		{
			INFO_PRINTF("path(%s), hour(%d), min(%d), sec(%d), IsVideo(%d), ID(%d) \n", path, hour, min, sec, isVideo, id);
			if(isVideo ==1)
			{
				ret = MultiMediaPlayStartAV((uint8_t)MultiMediaContentTypeVideo, (const char *)path, hour, min, sec, id,keepPause);
			}
			else
			{
				ret = MultiMediaPlayStartAV((uint8_t)MultiMediaContentTypeAudio, (const char *)path, hour, min, sec, id,keepPause);
			}

			if(ret != 0)
			{
				currentPlayID = getCurrentPlayID();
			}
			else
			{
				currentPlayID = 0;
			}

			INFO_PRINTF("return message busy(%d), playID(%d)\n", ret, currentPlayID);

			returnMessage = CreateDBusMsgMethodReturn(message,
	 													DBUS_TYPE_INT32, &ret,
														DBUS_TYPE_INT32, &currentPlayID,
														DBUS_TYPE_INVALID);
			if (returnMessage != NULL)
			{
				if (SendDBusMessage(returnMessage, NULL) == 0)
				{
					ERROR_PRINTF("SendDBusMessage failed\n");
				}
				dbus_message_unref(returnMessage);
			}
		}
		else
		{
			ERROR_PRINTF("GetArgumentFromDBusMessage failed\n");
		}
	}
}

static void DBusMethodPlayStop(DBusMessage *message)
{
	DEBUG_PRINTF("\n");

	if (message != NULL)
	{
		int32_t id;
		int32_t ret;
		DBusMessage *returnMessage;

		if (GetArgumentFromDBusMessage(message,
										DBUS_TYPE_INT32, &id,
										DBUS_TYPE_INVALID))
		{
			INFO_PRINTF("ID(%d)\n", id);

			ret = MultiMediaPlayStop(id);

			returnMessage = CreateDBusMsgMethodReturn(message,
				DBUS_TYPE_INT32,
				&ret,
				DBUS_TYPE_INVALID);
			if (returnMessage != NULL)
			{
				if (!SendDBusMessage(returnMessage, NULL))
				{
					ERROR_PRINTF("SendDBusMessage failed\n");
				}
				dbus_message_unref(returnMessage);
			}
		}
		else
		{
			ERROR_PRINTF("GetArgumentFromDBusMessage failed\n");
		}
	}
}

static void DBusMethodPlayPause(DBusMessage *message)
{
	DEBUG_PRINTF("\n");

	if (message != NULL)
	{
		int32_t id;
		int32_t ret;
		DBusMessage *returnMessage;

		if (GetArgumentFromDBusMessage(message,
										DBUS_TYPE_INT32, &id,
										DBUS_TYPE_INVALID))
		{
			INFO_PRINTF("ID(%d)\n", id);

			ret = MultiMediaPlayPause(id);

			returnMessage = CreateDBusMsgMethodReturn(message,
														DBUS_TYPE_INT32, &ret,
														DBUS_TYPE_INVALID);
			if (returnMessage != NULL)
			{
				if (!SendDBusMessage(returnMessage, NULL))
				{
					ERROR_PRINTF("SendDBusMessage failed\n");
				}
				dbus_message_unref(returnMessage);
			}
		}
		else
		{
			ERROR_PRINTF("GetArgumentFromDBusMessage failed\n");
		}
	}

}


static void DBusMethodPlayResume(DBusMessage *message)
{
	DEBUG_PRINTF("\n");
	if (message != NULL)
	{
		int32_t id;
		int32_t ret;
		DBusMessage *returnMessage;

		if (GetArgumentFromDBusMessage(message,
										DBUS_TYPE_INT32, &id,
										DBUS_TYPE_INVALID))
		{
			INFO_PRINTF("ID(%d)\n", id);

			ret = MultiMediaPlayResume(id);

			returnMessage = CreateDBusMsgMethodReturn(message,
														DBUS_TYPE_INT32, &ret,
														DBUS_TYPE_INVALID);
			if (returnMessage != NULL)
			{
				if (!SendDBusMessage(returnMessage, NULL))
				{
					ERROR_PRINTF("SendDBusMessage failed\n");
				}
				dbus_message_unref(returnMessage);
			}
		}
		else
		{
			ERROR_PRINTF("GetArgumentFromDBusMessage failed\n");
		}
	}

}
static void DBusMethodPlaySeek(DBusMessage *message)
{
	DEBUG_PRINTF("\n");
	if (message != NULL)
	{
		uint8_t hour, min, sec;
		int32_t id;
		int32_t ret;
		DBusMessage *returnMessage;
		if (GetArgumentFromDBusMessage(message, 
										DBUS_TYPE_BYTE, &hour, 
										DBUS_TYPE_BYTE,  &min, 
										DBUS_TYPE_BYTE, &sec,
										DBUS_TYPE_INT32, &id,
										DBUS_TYPE_INVALID))
		{
			INFO_PRINTF("hour(%d), min(%d), sec(%d), id(%d)\n", (int32_t) hour, (int32_t)min, (int32_t)sec, id);
			ret = MultiMediaPlaySeek(hour, min, sec, id);
			returnMessage = CreateDBusMsgMethodReturn(message,
														DBUS_TYPE_INT32, &ret,
														DBUS_TYPE_INVALID);
			if (returnMessage != NULL)
			{
				if (!SendDBusMessage(returnMessage, NULL))
				{
					ERROR_PRINTF("SendDBusMessage failed\n");
				}
				dbus_message_unref(returnMessage);
			}

		}
		else
		{
			ERROR_PRINTF("GetArgumentFromDBusMessage failed\n");
		}
	}
}

static void DBusMethodSetDisplay(DBusMessage *message)
{
	DEBUG_PRINTF("\n");

	if (message != NULL)
	{
		uint32_t x, y;
		uint32_t width, height;
		if (GetArgumentFromDBusMessage(message,
										DBUS_TYPE_INT32, &x,
										DBUS_TYPE_INT32, &y,
										DBUS_TYPE_INT32, &width,
										DBUS_TYPE_INT32, &height,
										DBUS_TYPE_INVALID))
		{
			INFO_PRINTF("x(%d), y(%d), width(%d), height(%d)\n", x, y, width, height);
			MultiMediaSetVideoDisplayInfo((uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height);
		}
		else
		{
			ERROR_PRINTF("GetArgumentFromDBusMessage failed\n");
		}
	}
}

static void DBusMethodSetDualDisplay(DBusMessage *message)
{
	DEBUG_PRINTF("\n");

	if (message != NULL)
	{
		uint32_t x, y;
		uint32_t width, height;
		if (GetArgumentFromDBusMessage(message,
										DBUS_TYPE_INT32, &x,
										DBUS_TYPE_INT32, &y,
										DBUS_TYPE_INT32, &width,
										DBUS_TYPE_INT32, &height,
										DBUS_TYPE_INVALID))
		{
			INFO_PRINTF("x(%d), y(%d), width(%d), height(%d)\n", x, y, width, height);
			MultiMediaSetDualVideoDisplayInfo((uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height);
		}
		else
		{
			ERROR_PRINTF("GetArgumentFromDBusMessage failed\n");
		}
	}
}

static void DBusMethodSetDebug(DBusMessage *message)
{
	DEBUG_PRINTF("\n");
	
	if (message != NULL)
	{
		int32_t debug = 0;
		if (GetArgumentFromDBusMessage(message, 
										DBUS_TYPE_INT32, &debug,
										DBUS_TYPE_INVALID))
		{
			INFO_PRINTF("debug level (%d)\n", debug);
			MultiMediaSetDebugLevel(debug);
		}
		else
		{
			ERROR_PRINTF("GetArgumentFromDBusMessage failed\n");
		}
	}
}

static void DBusMethodGetStatus(DBusMessage *message)
{
	DEBUG_PRINTF("\n");
	if (message != NULL)
	{
		DBusMessage *returnMessage;
		int32_t status;
		status = MultiMediaGetResourceStatus();
		INFO_PRINTF("Current Status : %d\n",status);
		returnMessage = CreateDBusMsgMethodReturn(message,
 													DBUS_TYPE_INT32, &status,
													DBUS_TYPE_INVALID);
		if (returnMessage != NULL)
		{
			if (SendDBusMessage(returnMessage, NULL) == 0)
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
			dbus_message_unref(returnMessage);
		}
	}
	else
	{
		ERROR_PRINTF("mesage is NULL\n");
	}
}

static void DBusMethodGetAlbumArtKey(DBusMessage *message)
{
	DEBUG_PRINTF("\n");

	if (message != NULL)
	{
		DBusMessage *returnMessage;
		int32_t key = KEY_NUM;
		uint32_t size = MEM_SIZE;

		returnMessage = CreateDBusMsgMethodReturn(message,
													DBUS_TYPE_INT32, &key,
													DBUS_TYPE_UINT32, &size,
													DBUS_TYPE_INVALID);
		if (returnMessage != NULL)
		{
			if (!SendDBusMessage(returnMessage, NULL))
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
			dbus_message_unref(returnMessage);
		}

	}
	else
	{
		ERROR_PRINTF("mesage is NULL\n");
	}
}

static void DBusMethodGetPlayID(DBusMessage *message)
{
	DEBUG_PRINTF("\n");
	if (message != NULL)
	{
		DBusMessage *returnMessage;
		int32_t playingID = 0;
		playingID = getCurrentPlayID();
		returnMessage = CreateDBusMsgMethodReturn(message,
													DBUS_TYPE_INT32, &playingID,
													DBUS_TYPE_INVALID);
		DEBUG_PRINTF("Current Play ID is %d\n", playingID);
		if (returnMessage != NULL)
		{
			if (!SendDBusMessage(returnMessage, NULL))
			{
				ERROR_PRINTF("SendDBusMessage failed\n");
			}
			dbus_message_unref(returnMessage);
		}
	}
	else
	{
		ERROR_PRINTF("mesage is NULL\n");
	}
}


