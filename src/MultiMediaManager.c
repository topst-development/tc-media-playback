/****************************************************************************************
 *   FileName    : MultiMediaManager.c
 *   Description : Telechips Media Manager
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
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h> /* for open/close */
#include <fcntl.h> /* for O_RDWR */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <gst/gst.h>
#include <glib.h>
#include "TCLog.h"
#include "TCTime.h"
#include "TCMultiMediaType.h"
#include "MultiMediaManager.h"
#include "MediaPlaybackDBus.h"

#define AUDIO_SINK_LATENCY_TIME		92880
#define AUDIO_SINK_BUFFER_TIME		371520
#define FF_REW_SPEED				(4.0)
#define TURBO_FF_REW_SPEED			(16.0)

static int32_t s_ResourceBusy=0;
	
static int32_t s_errorOccurred =0;

typedef struct stGstVideoSinkProperty{
	const char *x_start;
	const char *y_start;
	const char *width;
	const char *heigth;
	const char *update;
	const char *display_scenario;
	const char *dual_x_start;
	const char *dual_y_start;
	const char *dual_width;
	const char *dual_height;
	const char *dual_update;
	const char *aspectratio;
}GstVideoSinkProperty;

static GstVideoSinkProperty  s_videoSinkProperty = {
	"overlay-set-left",
	"overlay-set-top",
	"overlay-set-width",
	"overlay-set-height",
	"overlay-set-update",
	"displayscenariomode",
	"dual-overlay-set-left",
	"dual-overlay-set-top",
	"dual-overlay-set-width",
	"dual-overlay-set-height",
	"dual-overlay-set-update",
	"aspectratio"
};

static char s_audioSinkName[MAX_SINK_DEVICE_NAME];
static char s_audioDeviceName[MAX_SINK_DEVICE_NAME];
static char *s_audioDeviceNamePtr = NULL;
static char s_v4lDevice[MAX_SINK_DEVICE_NAME];
static int32_t s_shm_id;
static void * s_shm_addr;
static uint8_t s_dualDisplay;

typedef struct stAlbumArt {
	uint32_t length;
	uint8_t *buf;
	bool complete;
} AlbumArt;

#define MAX_ID3_TAG_SIZE	512

typedef struct stID3Information {
	char title[MAX_ID3_TAG_SIZE];
	char artist[MAX_ID3_TAG_SIZE];
	char album[MAX_ID3_TAG_SIZE];
	char genre[MAX_ID3_TAG_SIZE];
	AlbumArt albumArt;
} ID3Information;

typedef struct stVideoInfo {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	uint32_t marginW;
	uint32_t marginH;

	uint32_t dual_x;
	uint32_t dual_y;
	uint32_t dual_width;
	uint32_t dual_height;
} VideoInfo;

typedef struct stAVPlayer {
	GstElement *playbin;
	GstElement *src;
	GstElement *audioSink;
	GstElement *videoSink;
	ID3Information *id3Info;
	GstBus *bus;
	guint watchID;
	bool video;
	int32_t playID;
} AVPlayer;

typedef struct stMultiMediaPlayer {
	AVPlayer avPlayer;
	char  *path;
	gint64 position;
	gint64 startPos;
	bool playing;
	bool userStop;
	bool userPause;
	bool backward;
	bool fastforward;
	bool updatePlayTime;
	bool async_done;
	bool getduration;
	gboolean seek_enabled;
} MultiMediaPlayer;

typedef struct stPlayInfo {
	int32_t id;
	uint8_t content;
	char * path;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t	keepPause;
} PlayInfo;

static void MultiMediaSetResourceStatus(int32_t status);
static void GetSamplerate(MultiMediaPlayer *player, int32_t playID);
static gboolean GstBusHandler(GstBus *bus, GstMessage *msg, gpointer data);
static void GStreamerMessageParser(MultiMediaPlayer *player, GstMessage *msg);
static void InitializeID3Information(void);
static void SetID3Information(const GstTagList * list, const gchar * tag, gpointer user_data);
static bool MultiMediaPlayStart(const char *path, uint8_t hour, uint8_t min, uint8_t sec, bool video, int32_t id, uint8_t keepPause);
static MultiMediaPlayer *CreateAVPlayer(bool video);
static void ReleasePlayer(MultiMediaPlayer *player);
static void ReleaseAVPlayer(AVPlayer *player);
static void SetSourceLocation(GstElement *obj,  GstElement* arg, gpointer userdata);
static bool StartPlayer(MultiMediaPlayer *player, uint8_t keepPause);
static void StopPlayer(MultiMediaPlayer **player);
static void StartPlayTimeThread(void);
static void StopPlayTimeThread(void);
static void StartMediaStartThread(void);
static void StopMediaStartThread(void);
#if 0
static GstState GetCurrentPlayerState(GstElement* player);
#endif
static bool ChangePlayerState(GstElement* player, GstState state);
static gboolean GetCurrentPlayerPosition(gint64 *position);
static void SetAVSync(bool sink);
static void UpdateVideoDisplay(bool update);
static void UpdateDualVideoDisplay(void);
static void ProcessGstErrorMessage(GstMessage *errorMsg, int32_t playID);
static void *PlayTimeThread(void *arg);
static void *MediaStartThread(void *arg);
static void ReleasePlayInfo(void);
static char *CloneString(const char *string);
static int32_t SharedMemoryInitialize(void);
static int32_t SharedMemoryRelease(void);

typedef enum {
	MultiMediaCommandPlay,
	MultiMediaCommandStop,
	MultiMediaCommandPause,
	MultiMediaCommandResume,
	MultiMediaCommandNormal,
	MultiMediaCommandFastForward,
	MultiMediaCommandFastBackward,
	MultiMediaCommandTurboFastForward,
	MultiMediaCommandTurboFastBackward,
	MultiMediaCommandSeek,
	TotalMultiMediaCommands
} MultiMediaCommand;

static void ProcessPlayStart(const char *path, uint8_t hour, uint8_t min, uint8_t sec, bool video, int32_t playID, uint8_t keepPause);
static void ProcessPlayStop(void);
static void ProcessPlayPause(void);
static void ProcessPlayResume(void);
static void ProcessPlayNormal(void);
static void ProcessPlayFastForward(void);
static void ProcessPlayFastBackward(void);
static void ProcessPlayTurboFastForward(void);
static void ProcessPlayTurboFastBackward(void);
static void ProcessPlaySeek(uint8_t hour, uint8_t min, uint8_t sec);

static MultiMediaPlayer *s_currentPlayer = NULL;
static bool s_playtimeRun = false;
static pthread_t s_playtimeThread;

static bool s_mediastartRun = false;
static pthread_t s_mediastartThread;

#define LAST_PLAY_CHECK_TIME				(1.0)

static pthread_mutex_t s_mutex;

static PlayInfo s_playInfo = {
	0,
	0,
	NULL,
	0,
	0,
	0
};

static ID3Information s_id3Information = {
	"",					/* title */
	"",					/* artist */
	"",					/* album */
	"",					/* genre */
	{0, NULL, false}	/* album art(width, height, format, length, buffer) */
};

static VideoInfo s_videoInfo = {0, 0, 800, 480, 0, 0, 0, 0, 0, 0};

static MultiMediaPlayStarted_cb				MultiMediaPlayStartedCB = NULL;
static MultiMediaPlayPaused_cb				MultiMediaPlayPausedCB = NULL;
static MultiMediaPlayStopped_cb				MultiMediaPlayStoppedCB = NULL;
static MultiMediaPlayTimeChange_cb			MultiMediaPlayTimeChangeCB = NULL;
static MultiMediaTotalTimeChange_cb			MultiMediaTotalTimeChangeCB = NULL;
static MultiMediaID3Information_cb			MultiMediaID3InformationCB = NULL;
static MultiMediaAlbumArt_cb				MultiMediaAlbumArtCB = NULL;
static MultiMediaPlayCompleted_cb			MultiMediaPlayCompletedCB = NULL;
static MultiMediaSeekCompleted_cb			MultiMediaSeekCompletedCB = NULL;
static MultiMediaErrorOccurred_cb			MultiMediaErrorOccurredCB = NULL;
static MultiMediaSamplerate_cb				MultiMediaSamplerateCB = NULL;


static MultiMediaCommand s_currentCmd = TotalMultiMediaCommands;
static pthread_mutex_t s_cmdMutex;
static pthread_mutex_t s_timeMutex;
static pthread_mutex_t s_stopMutex;
static pthread_mutex_t s_errorMutex;


void MultiMediaSetDebugLevel(int32_t level)
{
	TCLogSetLevel(level);
}

int32_t MultiMediaInitialize(void)
{
	int32_t err;
	int32_t ret = 0;

	(void)memcpy(s_audioSinkName, DEFAULT_AUDIO_SINK_NAME, strnlen(DEFAULT_AUDIO_SINK_NAME,MAX_SINK_DEVICE_NAME));
	(void)memcpy(s_audioDeviceName, ALSA_DEFAULT_DEVICE_NAME, strnlen(ALSA_DEFAULT_DEVICE_NAME,MAX_SINK_DEVICE_NAME));
	s_audioDeviceNamePtr = s_audioDeviceName;
	(void)memcpy(s_v4lDevice, V4L_DEFAULT_DEVICE_NAME, strnlen(V4L_DEFAULT_DEVICE_NAME,MAX_SINK_DEVICE_NAME));

	err = pthread_mutex_init(&s_mutex, NULL);
	if (err == 0)
	{
		ret = 1;
	}
	else
	{
		ERROR_PRINTF("pthread_mutex_init failed. error(%d)\n",err);
	}

	err = pthread_mutex_init(&s_cmdMutex, NULL);
	if (err == 0)
	{
		ret = 1;
	}
	else
	{
		ERROR_PRINTF("command mutex pthread_mutex_init failed: error(%d)\n", err);
	}

	err = pthread_mutex_init(&s_timeMutex, NULL);
	if (err == 0)
	{
		ret = 1;
	}
	else
	{
		ERROR_PRINTF("time mutex pthread_mutex_init failed: error(%d)\n", err);
	}

	err = pthread_mutex_init(&s_stopMutex, NULL);
	if (err == 0)
	{
		ret = 1;
	}
	else
	{
		ERROR_PRINTF("stop mutex pthread_mutex_init failed: error(%d)\n",err);
	}

	err = pthread_mutex_init(&s_errorMutex, NULL);
	if (err == 0)
	{
		ret = 1;
	}
	else
	{
		ERROR_PRINTF("error mutex pthread_mutex_init failed: error(%d)\n", err);
	}

	(void)SharedMemoryInitialize();
	
	StartMediaStartThread();
	
	SetCurrentTime();
	
	return ret;
}

int32_t MultiMediaGetResourceStatus(void)
{
	int32_t ret;
	(void)pthread_mutex_lock(&s_cmdMutex);
	ret = s_ResourceBusy;
	(void)pthread_mutex_unlock(&s_cmdMutex);

	return ret;
}

static void MultiMediaSetResourceStatus(int32_t status)
{
	/* pthread_mutex_lock(&s_cmdMutex); */
	s_ResourceBusy = status;
	/* pthread_mutex_unlock(&s_cmdMutex); */
}


void SetEventCallBackFunctions(TcMultiMediaEventCB *cb)
{
	if (cb != NULL)
	{
		MultiMediaPlayStartedCB = cb->MultiMediaPlayStartedCB;
		MultiMediaPlayPausedCB = cb->MultiMediaPlayPausedCB;
		MultiMediaPlayStoppedCB = cb->MultiMediaPlayStoppedCB;
		MultiMediaPlayTimeChangeCB = cb->MultiMediaPlayTimeChangeCB;
		MultiMediaTotalTimeChangeCB = cb->MultiMediaTotalTimeChangeCB;
		MultiMediaID3InformationCB = cb->MultiMediaID3InformationCB;
		MultiMediaAlbumArtCB = cb->MultiMediaAlbumArtCB;
		MultiMediaPlayCompletedCB = cb->MultiMediaPlayCompletedCB;
		MultiMediaSeekCompletedCB = cb->MultiMediaSeekCompletedCB;
		MultiMediaErrorOccurredCB = cb->MultiMediaErrorOccurredCB;
		MultiMediaSamplerateCB = cb->MultiMediaSamplerateCB;
	}
}

void MultiMediaRelease(void)
{
	int32_t err;

	StopPlayer(&s_currentPlayer);
	if (s_currentPlayer != NULL)
	{
		ReleasePlayer(s_currentPlayer);
	}

	ReleasePlayInfo();

	StopMediaStartThread();	

	err = pthread_mutex_destroy(&s_mutex);
	if (err != 0)
	{
		ERROR_PRINTF("s_mutex destroy faild: error(%d)\n", err);
	}

	err = pthread_mutex_destroy(&s_cmdMutex);
	if (err != 0)
	{
		ERROR_PRINTF("s_cmdMutex destroy faild: error(%d)\n", err);
	}

	err = pthread_mutex_destroy(&s_timeMutex);
	if (err != 0)
	{
		ERROR_PRINTF("s_timeMutex destroy faild: error(%d)\n", err);
	}

	err = pthread_mutex_destroy(&s_stopMutex);
	if (err != 0)
	{
		ERROR_PRINTF("s_stopMutex destroy faild: error(%d)\n", err);
	}

	err = pthread_mutex_destroy(&s_errorMutex);
	if (err != 0)
	{
		ERROR_PRINTF("s_errorMutex destroy faild: error(%d)\n", err);
	}

	(void)SharedMemoryRelease();
}

void MultiMediaSetMargin(uint32_t width, uint32_t height)
{
	if (width < s_videoInfo.width)
	{
		s_videoInfo.marginW = width;
	}
	
	if (height < s_videoInfo.height)
	{
		s_videoInfo.marginH = height;
	}
}

void MultiMediaSetVideoDisplayInfo(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	INFO_PRINTF("Set Display : x(%d), y(%d), width (%d), height(%d)\n",x, y, width, height);
	s_videoInfo.x = x;
	s_videoInfo.y = y;
	s_videoInfo.width = width;
	s_videoInfo.height = height;

	UpdateVideoDisplay(1);
}

void MultiMediaSetDualVideoDisplayInfo(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	INFO_PRINTF("Set Display : x(%d), y(%d), width (%d), height(%d)\n",x, y, width, height);

	if ((width > 0) && (height > 0))
	{
		s_dualDisplay = 1;
	}
	else
	{
		s_dualDisplay = 0;
	}

	s_videoInfo.dual_x = x;
	s_videoInfo.dual_y = y;
	s_videoInfo.dual_width = width;
	s_videoInfo.dual_height = height;

	UpdateDualVideoDisplay();
}

int32_t MultiMediaPlayStartAV(uint8_t content, const char *path, uint8_t hour, uint8_t min, uint8_t sec, int32_t id, uint8_t keepPause)
{
	int32_t ret;
	
	INFO_PRINTF("CONTENT(%u), PATH(%s), HOUR(%u), MINUTE(%u), SECOND(%u), ID(%d), keepPause(%d)\n",
									 content, path, hour, min, sec, id, keepPause);

	(void)pthread_mutex_lock(&s_cmdMutex);
	if(s_ResourceBusy ==0)
	{
		s_playInfo.content = content;
		if(s_playInfo.path != NULL)
		{
			free(s_playInfo.path);
		}
		s_playInfo.path = CloneString(path);
		s_playInfo.hour = hour;
		s_playInfo.min = min;
		s_playInfo.sec = sec;
		s_playInfo.id = id;
		s_playInfo.keepPause = keepPause;
		
		s_currentCmd = MultiMediaCommandPlay;

		s_ResourceBusy = 1;
		ret =0;
	}
	else
	{
		INFO_PRINTF("Can't play -  Resouce is busy \n");
		ret =-1;
	}

	(void)pthread_mutex_unlock(&s_cmdMutex);

	return ret;
}

int32_t MultiMediaPlayStop(int32_t id)
{
	int32_t ret = 0;

	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	if(s_ResourceBusy == 1)
	{

		INFO_PRINTF("Set stop cmd. request id(%d), play id(%d)\n", id, s_playInfo.id);
		if(s_playInfo.id == id)
		{
			s_currentCmd = MultiMediaCommandStop;
		}
		else
		{
			ret = -2;
		}
	}
	else
	{
		INFO_PRINTF("Resource is not playing.\n");
		ret = -1;
	}

	(void)pthread_mutex_unlock(&s_cmdMutex);

	return ret;
}

int32_t MultiMediaPlayPause(int32_t id)
{
	int32_t ret = 0;

	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	if(s_ResourceBusy == 1)
	{
		INFO_PRINTF("Set pause cmd. request id(%d), play id(%d)\n", id, s_playInfo.id);
		if(s_playInfo.id == id)
		{
			s_currentCmd = MultiMediaCommandPause;
		}
		else
		{
			ret = -2;
		}
	}
	else
	{
		INFO_PRINTF("Resource is not playing.\n");
		ret = -1;
	}

	(void)pthread_mutex_unlock(&s_cmdMutex);
	return ret;
}

int32_t MultiMediaPlayResume(int32_t id)
{
	int32_t ret = 0;

	DEBUG_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	if(s_ResourceBusy == 1)
	{
		INFO_PRINTF("Set resume cmd. request id(%d), play id(%d)\n", id, s_playInfo.id);
		if(s_playInfo.id == id)
		{
			s_currentCmd = MultiMediaCommandResume;
		}
		else
		{
			ret = -2;
		}
	}
	else
	{
		INFO_PRINTF("Resource is not playing.\n");
		ret = -1;
	}

	(void)pthread_mutex_unlock(&s_cmdMutex);

	return ret;
}


void MultiMediaPlayNormal(void)
{
	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	s_currentCmd = MultiMediaCommandNormal;

	(void)pthread_mutex_unlock(&s_cmdMutex);
}

void MultiMediaPlayFastForward(void)
{
	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	s_currentCmd = MultiMediaCommandFastForward;

	(void)pthread_mutex_unlock(&s_cmdMutex);
}

void MultiMediaPlayFastBackward(void)
{
	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	s_currentCmd = MultiMediaCommandFastBackward;

	(void)pthread_mutex_unlock(&s_cmdMutex);
}

void MultiMediaPlayTurboFastForward(void)
{
	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	s_currentCmd = MultiMediaCommandTurboFastForward;

	(void)pthread_mutex_unlock(&s_cmdMutex);
}

void MultiMediaPlayTurboFastBackward(void)
{
	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	s_currentCmd = MultiMediaCommandTurboFastBackward;

	(void)pthread_mutex_unlock(&s_cmdMutex);
}

int32_t MultiMediaPlaySeek(uint8_t hour, uint8_t min, uint8_t sec, int32_t id)
{
	int32_t ret = 0;

	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_cmdMutex);

	if(s_ResourceBusy == 1)
	{
		INFO_PRINTF("Set seek cmd. request id(%d), play id(%d)\n", id, s_playInfo.id);
		if(s_playInfo.id == id)
		{
			s_playInfo.hour = hour;
			s_playInfo.min = min;
			s_playInfo.sec = sec;

			s_currentCmd = MultiMediaCommandSeek;
		}
		else
		{
			ret = -2;
		}
	}
	else
	{
		ret = -1;
	}

	(void)pthread_mutex_unlock(&s_cmdMutex);

	return ret;
}

int32_t MultiMediaGetAlbumArt(uint8_t **buffer, uint32_t *length)
{
	int32_t ret=-1;
	(void)pthread_mutex_lock(&s_cmdMutex);

	INFO_PRINTF("\n");

	if(s_currentPlayer != NULL)
	{
		if(s_currentPlayer->avPlayer.id3Info != NULL)
		{
			if((s_currentPlayer->avPlayer.id3Info->albumArt.buf != NULL)
				&& (s_currentPlayer->avPlayer.id3Info->albumArt.length != (uint32_t)0))
			{
				*buffer = s_currentPlayer->avPlayer.id3Info->albumArt.buf;
				*length = s_currentPlayer->avPlayer.id3Info->albumArt.length;
				ret =0;
			}
		}
	}

	(void)pthread_mutex_unlock(&s_cmdMutex);

	return ret;
}

void MultiMediaSetAudioSink(const char *audioSink,const char *device)
{
	if(audioSink!= NULL)
	{
		uint32_t size;
		INFO_PRINTF("set audioSink (%s)\n", audioSink);

		if(strlen(audioSink) > (size_t)MAX_SINK_DEVICE_NAME)
		{
			size = MAX_SINK_DEVICE_NAME-1;
		}
		else
		{
			size = strlen(audioSink);
		}
		(void)memset(s_audioSinkName, 0x00, MAX_SINK_DEVICE_NAME);
		(void)strncpy(s_audioSinkName, audioSink, size);
		if (device != NULL)
		{
			INFO_PRINTF("set audioSink device (%s)\n", device);
			if(strlen(device) > (size_t)MAX_SINK_DEVICE_NAME)
			{
				size = (MAX_SINK_DEVICE_NAME-1);
			}
			else
			{
				size = strlen(device);
			}
			(void)memset(s_audioDeviceName, 0x00, MAX_SINK_DEVICE_NAME);
			(void)strncpy(s_audioDeviceName, device, size);
			s_audioDeviceNamePtr = s_audioDeviceName;
		}
		else
		{
			s_audioDeviceNamePtr = NULL;
		}
	}
}

void MultiMediaSetV4LDevice(const char *device)
{
	if (device != NULL)
	{
		INFO_PRINTF("set v4ldevice (%s)\n", device);
		uint32_t size;
		if(strlen(device) > (size_t)MAX_SINK_DEVICE_NAME)
		{
			size = MAX_SINK_DEVICE_NAME-1;
		}
		else
		{
			size = strlen(device);
		}
		(void)strncpy(s_v4lDevice, device, size);
	}
}

void MultiMediaErrorOccurred(int32_t code, int32_t playID)
{
	if(MultiMediaErrorOccurredCB != NULL)
	{
		(void)pthread_mutex_lock(&s_errorMutex);
		if(s_errorOccurred == 0)
		{
			MultiMediaErrorOccurredCB(code, playID);
			s_errorOccurred = 1;
		}
		(void)pthread_mutex_unlock(&s_errorMutex);
	}
}

static void GetSamplerate(MultiMediaPlayer *player, int32_t playID)
{
	GstPad *audio_pad = NULL;
	GstCaps *caps = NULL;
	if((player != NULL)&&(player->avPlayer.playbin != NULL))
	{
		g_signal_emit_by_name(G_OBJECT(player->avPlayer.playbin),"get-audio-pad",0, &audio_pad);
		if (audio_pad != NULL)
		{
			gint samplerate = 0;
			GstStructure *structure = NULL;
			caps = gst_pad_get_current_caps(audio_pad);
			structure = gst_caps_get_structure(caps, 0);
			gst_structure_get_int(structure,"rate",&samplerate);
			INFO_PRINTF("samplerate = %d\n",samplerate);
			if(MultiMediaSamplerateCB != NULL)
			{
				MultiMediaSamplerateCB(samplerate, playID);
			}
			gst_object_unref(audio_pad);
		}
		else
		{
			ERROR_PRINTF("audio_pad is NULL\n");
		}
	}
	else
	{
		ERROR_PRINTF("player is NULL\n");
	}

}

static gboolean GstBusHandler(GstBus *bus, GstMessage *msg, gpointer data)
{
	MultiMediaPlayer *player = (MultiMediaPlayer *)data;
	GStreamerMessageParser(player, msg);

	(void)bus;
	return (gboolean)TRUE;
}

static void GStreamerMessageParser(MultiMediaPlayer *player, GstMessage *msg)
{

	if ((player != NULL) && (s_currentPlayer != NULL))
	{
		switch (GST_MESSAGE_TYPE(msg))
		{
			case GST_MESSAGE_EOS:
			{
				INFO_PRINTF("GST_MESSAGE_EOS\n");
				if (MultiMediaPlayCompletedCB != NULL)
				{
					MultiMediaPlayCompletedCB(player->avPlayer.playID);
				}

				break;
			}
			case GST_MESSAGE_TAG:
			{
				if (player->avPlayer.video == (bool)0)
				{
					GstTagList *tags = NULL;

					(void)pthread_mutex_lock(&s_mutex);

					gst_message_parse_tag (msg, &tags);

					player->avPlayer.id3Info = &s_id3Information;
					gst_tag_list_foreach(tags, SetID3Information, &player->avPlayer);
					gst_tag_list_free(tags);

					(void)pthread_mutex_unlock(&s_mutex);
				}
				break;
			}
			case GST_MESSAGE_STATE_CHANGED:
			{
				GstState oldState, newState;
				gst_message_parse_state_changed(msg, &oldState, &newState, NULL);

				if (strcmp(GST_OBJECT_NAME(msg->src), "player") == 0)
				{
					INFO_PRINTF("%s STATE CHANGED (%d->%d)\n",
													 GST_OBJECT_NAME(msg->src), (int32_t)oldState, (int32_t)newState);
					if ((newState == GST_STATE_PLAYING) && (oldState == GST_STATE_PAUSED))
					{
						if (MultiMediaPlayStartedCB != NULL)
						{
							MultiMediaPlayStartedCB(player->avPlayer.playID);
						}

						if (!player->playing)
						{
							GstFormat format = GST_FORMAT_TIME;
							int32_t hour, min, sec;
							gint64 duration;

							if (gst_element_query_duration(player->avPlayer.playbin, format, &duration))
							{
								sec = (int32_t)GST_TIME_AS_SECONDS(duration);
								min = sec / 60;
								sec %= 60;
								hour = min / 60;
								min %= 60;
								player->getduration = true;

								if (MultiMediaTotalTimeChangeCB != NULL)
								{
									MultiMediaTotalTimeChangeCB((uint32_t)hour, (uint32_t)min, (uint32_t)sec, player->avPlayer.playID);
								}
							}
							else
							{
								INFO_PRINTF("gst_element_query_duration failed\n");
							}

							player->playing = true;
						}

						player->updatePlayTime = true;
					}
					else if (((newState == GST_STATE_PAUSED) && (oldState == GST_STATE_PLAYING))||
						((newState == GST_STATE_PAUSED) && (oldState == GST_STATE_READY)))
					{
						if (player->userPause)
						{
							if (MultiMediaPlayPausedCB != NULL)
							{
								MultiMediaPlayPausedCB(player->avPlayer.playID);
							}
							player->userPause = false;
						}
					}
					else if (newState == GST_STATE_READY)
					{
						player->playing = false;
					}
					else
					{
						;
					}
				}
				break;
			}
			case GST_MESSAGE_ERROR:
			{
				if(s_currentPlayer->userStop != true)
				{
					WARN_PRINTF("================== %s:GST_MESSAGE_ERROR ==================\n");
					ProcessGstErrorMessage(msg, player->avPlayer.playID);
				}
				break;
			}
			case GST_MESSAGE_ASYNC_DONE:
			{
				player->async_done = true;
				INFO_PRINTF("async_done\n");
				if( s_dualDisplay == 1)
				{
					GetSamplerate(player, player->avPlayer.playID);
				}
				#if 0
				 GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(player->avPlayer.playbin),GST_DEBUG_GRAPH_SHOW_ALL, "gst_dot" ); */
				#endif

				break;
			}
#ifndef GST_VER_0_10
			case GST_MESSAGE_DURATION_CHANGED:
			{
				GstFormat format = GST_FORMAT_TIME;
				uint32_t hour, min, sec;
				gint64 duration;
				if (gst_element_query_duration(player->avPlayer.playbin, format, &duration))
				{
					sec = (uint32_t)GST_TIME_AS_SECONDS(duration);
					min = sec / 60;
					sec %= 60;
					hour = min / 60;
					min %= 60;

					player->getduration = true;
					if (MultiMediaTotalTimeChangeCB != NULL)
					{
						MultiMediaTotalTimeChangeCB(hour, min, sec,player->avPlayer.playID);
					}
				}
				else
				{
					INFO_PRINTF("gst_element_query_duration failed\n");
				}
				break;
			}
#else
			case GST_MESSAGE_DURATION:
			{
				uint32_t hour, min, sec;
				gint64 duration;

				gst_message_parse_duration(msg, NULL, &duration);
				if (duration != (gint64)GST_CLOCK_TIME_NONE)
				{
					sec = (uint32_t)GST_TIME_AS_SECONDS(duration);
					min = sec / 60;
					sec %= 60;
					hour = min / 60;
					min %= 60;
					player->getduration = true;
					if (MultiMediaTotalTimeChangeCB != NULL)
					{
						MultiMediaTotalTimeChangeCB(hour, min, sec, player->avPlayer.playID);
					}
				}
				else
				{
					INFO_PRINTF("gst_message_parse_duration failed\n");
				}
				break;
			}
#endif
			default:
			{
				DEBUG_PRINTF("GST MESSAGE([%d]%s) RECEIVED\n",
						GST_MESSAGE_TYPE(msg),
						GST_MESSAGE_TYPE_NAME(msg));
				break;
			}
		}
	}
}

static void InitializeID3Information(void)
{
	(void)memset(s_id3Information.title, 0x00, MAX_ID3_TAG_SIZE);
	(void)memset(s_id3Information.artist, 0x00, MAX_ID3_TAG_SIZE);
	(void)memset(s_id3Information.album, 0x00, MAX_ID3_TAG_SIZE);
	s_id3Information.albumArt.length = 0;
	if(s_id3Information.albumArt.buf != NULL)
	{
		free(s_id3Information.albumArt.buf);
		s_id3Information.albumArt.buf = NULL;
	}
	s_id3Information.albumArt.complete = false;

}

static void SetID3Information(const GstTagList * list, const gchar * tag, gpointer user_data)
{
	int32_t i, num;
	AVPlayer *avPlayer = (AVPlayer *)user_data;
	int32_t ID3_length;
	
	if (avPlayer != NULL)
	{
		num = gst_tag_list_get_tag_size(list, tag);
		for (i = 0; i < num; ++i)
		{
			/* DEBUG_PRINTF("PARSE ID3 TAG(%s)\n",tag); */
											 
			if(gst_tag_get_type (tag) == G_TYPE_STRING)
		 	{
				const GValue *val;
				val = gst_tag_list_get_value_index (list, tag, i);

				if (strncmp(tag, GST_TAG_TITLE, 5) == 0)
				{
					if (G_VALUE_HOLDS_STRING (val) == 1)
					{
						ID3_length = strlen(g_value_get_string(val));
						if(ID3_length > MAX_ID3_TAG_SIZE)
						{
							ID3_length =MAX_ID3_TAG_SIZE;
						}
						(void)memset(avPlayer->id3Info->title, 0x00, MAX_ID3_TAG_SIZE);
						(void)memcpy(avPlayer->id3Info->title, g_value_get_string(val), ID3_length);
						DEBUG_PRINTF("PARSED ID3 TAG(%s:%s)\n", 
														 tag, avPlayer->id3Info->title);
						if (MultiMediaID3InformationCB != NULL)
						{
							MultiMediaID3InformationCB(MetaCategoryTitle, avPlayer->id3Info->title, avPlayer->playID);
						}
					}
					else
					{
						WARN_PRINTF("G_VALUE_HOLDS_STRING failed\n");
					}
				}
				else if (strncmp(tag, GST_TAG_ARTIST, 6) == 0)
				{
					if (G_VALUE_HOLDS_STRING (val) == 1)
					{
						ID3_length = strlen(g_value_get_string(val));
						if(ID3_length > MAX_ID3_TAG_SIZE)
						{
							ID3_length =MAX_ID3_TAG_SIZE;
						}
						(void)memset(avPlayer->id3Info->artist, 0x00, MAX_ID3_TAG_SIZE);
						(void)memcpy(avPlayer->id3Info->artist, g_value_get_string(val), ID3_length);

						DEBUG_PRINTF("PARSED ID3 TAG(%s:%s)\n",
														 tag, avPlayer->id3Info->artist);
						if (MultiMediaID3InformationCB != NULL)
						{
							MultiMediaID3InformationCB(MetaCategoryArtist, avPlayer->id3Info->artist, avPlayer->playID);
						}
					}
					else
					{
						WARN_PRINTF("G_VALUE_HOLDS_STRING failed\n");
					}
				}
				else if (strcmp(tag, GST_TAG_ALBUM) == 0)
				{
					if (G_VALUE_HOLDS_STRING (val))
					{
						ID3_length = strlen(g_value_get_string(val));
						if(ID3_length > MAX_ID3_TAG_SIZE)
						{
							ID3_length =MAX_ID3_TAG_SIZE;
						}
						(void)memset(avPlayer->id3Info->album, 0x00, MAX_ID3_TAG_SIZE);
						(void)memcpy(avPlayer->id3Info->album, g_value_get_string(val), ID3_length);
					
						DEBUG_PRINTF("PARSED ID3 TAG(%s:%s)\n",
														 tag, avPlayer->id3Info->album);
						if (MultiMediaID3InformationCB != NULL)
						{
							MultiMediaID3InformationCB(MetaCategoryAlbum, avPlayer->id3Info->album, avPlayer->playID);
						}
					}
					else
					{
						WARN_PRINTF("G_VALUE_HOLDS_STRING failed\n");
					}
				}
				else if (strncmp(tag, GST_TAG_GENRE, 5) == 0)
				{
					if (G_VALUE_HOLDS_STRING (val) == 1)
					{

						ID3_length = strlen(g_value_get_string(val));
						if(ID3_length > MAX_ID3_TAG_SIZE)
						{
							ID3_length =MAX_ID3_TAG_SIZE;
						}
						(void)memset(avPlayer->id3Info->genre, 0x00, MAX_ID3_TAG_SIZE);
						(void)memcpy(avPlayer->id3Info->genre, g_value_get_string(val), ID3_length);
					
						DEBUG_PRINTF("PARSED ID3 TAG(%s:%s)\n",
														 tag, avPlayer->id3Info->genre);
						if (MultiMediaID3InformationCB != NULL)
						{
							MultiMediaID3InformationCB(MetaCategoryGenre, avPlayer->id3Info->genre, avPlayer->playID);
						}
					}
					else
					{
						WARN_PRINTF("G_VALUE_HOLDS_STRING failed\n");
					}
				}
				else
				{
					;
				}
		 	}
			 else if (gst_tag_get_type (tag) == GST_TYPE_SAMPLE)
		 	{
				if (strncmp(tag, GST_TAG_IMAGE, 5) == 0)
				{

					GstSample *sample = NULL;

					if (gst_tag_list_get_sample_index (list, tag, i, &sample))
					{
						GstBuffer *img = gst_sample_get_buffer (sample);
						if (img)
						{
							GstMapInfo mapInfo;
							(void)gst_buffer_map(img, &mapInfo, GST_MAP_READ);

							DEBUG_PRINTF("PARSING ALBUMART. BUFFER(%p), LENGTH(%u)\n",
							mapInfo.data, mapInfo.size);

							if(avPlayer->id3Info->albumArt.buf  != NULL)
							{
								free(avPlayer->id3Info->albumArt.buf);
								avPlayer->id3Info->albumArt.buf = NULL;
							}

							if(avPlayer->id3Info->albumArt.complete == false)
							{
								(void)memcpy((uint8_t*)s_shm_addr, mapInfo.data, mapInfo.size);
								avPlayer->id3Info->albumArt.complete = true;
								if (MultiMediaAlbumArtCB != NULL)
								{
									MultiMediaAlbumArtCB(avPlayer->playID, mapInfo.size);
								}
							}
							gst_buffer_unmap (img, &mapInfo);
						}
						else
						{
							WARN_PRINTF("NULL BUFFER\n");
						}
					}
				}
		 	}
			 else
		 	{
		 		;
		 	}
		}
	}
}

static bool MultiMediaPlayStart(const char *path, uint8_t hour, uint8_t min, uint8_t sec, bool video, int32_t id, uint8_t keepPause)
{
	uint32_t totalSec;
	bool ret;

	totalSec = sec + (hour * 3600) + (min * 60);

	INFO_PRINTF("PLAYER AV, URI(%s), HOUR(%u), MINUTE(%u), SECOND(%u), VIDEO(%s)\n",
									 (path != NULL) ? path : "", hour, min, sec, video ? "TRUE" : "FALSE");

	gst_init(NULL, NULL);

	if (s_currentPlayer != NULL)
	{
		StopPlayer(&s_currentPlayer);
	}

	s_errorOccurred = 0;

	s_currentPlayer = CreateAVPlayer(video);

	if (s_currentPlayer != NULL)
	{
		if(s_currentPlayer->path != NULL)
		{
			free(s_currentPlayer->path);
		}
		s_currentPlayer->path = CloneString(path);
		s_currentPlayer->startPos = GST_SECOND * totalSec;
		s_currentPlayer->position = 0;
		s_currentPlayer->avPlayer.playID = id;

		if (StartPlayer(s_currentPlayer, keepPause))
		{
			ret = true;
			StartPlayTimeThread();
		}
		else
		{
			ReleasePlayer(s_currentPlayer);
			s_currentPlayer = NULL;
			ret = false;
			ERROR_PRINTF("StartPlayer failed\n");
		}
	}
	else
	{
		ret = false;
		ERROR_PRINTF("s_currentPlayer is NULL\n");
	}

	return ret;
}

static MultiMediaPlayer *CreateAVPlayer(bool video)
{
	MultiMediaPlayer *player = (MultiMediaPlayer *)malloc(sizeof (MultiMediaPlayer));
	bool created = false;

	INFO_PRINTF("\n");

	if (player != NULL)
	{
		player->path = NULL;
		player->avPlayer.playbin = NULL;
		player->avPlayer.src = NULL;
		player->avPlayer.audioSink = NULL;
		player->avPlayer.videoSink = NULL;
		player->avPlayer.id3Info = NULL;
		player->avPlayer.bus = NULL;
		player->avPlayer.watchID = FALSE;
		player->avPlayer.video = video;
		player->avPlayer.playID = 0;

		player->position = 0;
		player->startPos = 0;
		player->playing = false;
		player->userStop = false;
		player->userPause = false;
		player->backward = false;
		player->fastforward = false;
		player->updatePlayTime = false;
		player->getduration = false;
		player->async_done = false;
		player->seek_enabled = FALSE;

		InitializeID3Information();

		DEBUG_PRINTF("CREATE PLAY BIN\n");

		player->avPlayer.playbin = gst_element_factory_make("playbin", "player");
		if (player->avPlayer.playbin != NULL)
		{
			INFO_PRINTF("CREATE AUDIO SINK, sink=%s, device=%s \n",
											 s_audioSinkName, s_audioDeviceName);
			player->avPlayer.audioSink = gst_element_factory_make(s_audioSinkName, "audio-sink");
			if (player->avPlayer.audioSink != NULL)
			{
				if (s_audioDeviceNamePtr != NULL)
				{
					g_object_set(player->avPlayer.audioSink, "device", s_audioDeviceNamePtr, NULL);
					INFO_PRINTF("audio-sink-speaker device : %s\n",
													 s_audioDeviceNamePtr);
				}
			}
			else
			{
				(void)fprintf(stderr, "%s: gst_element_factory_make(%s) failed\n", __FUNCTION__, s_audioSinkName);
			}

			if(video == true)
			{
				INFO_PRINTF("CREATE VIDEO SINK, sink=%s, device=%s \n",
												 VIDEO_SINK_NAME, s_v4lDevice);
				player->avPlayer.videoSink = gst_element_factory_make(VIDEO_SINK_NAME, "video-sink");
				if (player->avPlayer.videoSink != NULL)
				{
					g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.x_start, s_videoInfo.x, NULL);
					g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.y_start, s_videoInfo.y, NULL);
					g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.width,  s_videoInfo.width, NULL);
					g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.heigth, s_videoInfo.height, NULL);

					if (s_dualDisplay)
					{
						INFO_PRINTF("Set Dual Display %d, %d, %d, %d\n",
								s_videoInfo.dual_x, s_videoInfo.dual_y, s_videoInfo.dual_width, s_videoInfo.dual_height);
						g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.dual_x_start, s_videoInfo.dual_x, NULL);
						g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.dual_y_start, s_videoInfo.dual_y, NULL);
						g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.dual_width,  s_videoInfo.dual_width, NULL);
						g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.dual_height, s_videoInfo.dual_height, NULL);
						g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.dual_update, 1, NULL);
					}

					g_object_set(player->avPlayer.videoSink, s_videoSinkProperty.aspectratio, 1, NULL);
					g_object_set(player->avPlayer.videoSink, "device", s_v4lDevice, NULL);
					INFO_PRINTF("video-sink device : %s\n", s_v4lDevice);
				}
				else
				{
					ERROR_PRINTF("gst_element_factory_make(%s) failed\n",VIDEO_SINK_NAME);
				}
			}

			DEBUG_PRINTF("GET GSTREAMER BUS FROM PLAY BIN\n");

			player->avPlayer.bus = gst_element_get_bus(player->avPlayer.playbin);

			if (player->avPlayer.bus != NULL)
			{
				DEBUG_PRINTF("ADD WATCH BUS HANDLER\n");
				player->avPlayer.watchID = gst_bus_add_watch(player->avPlayer.bus, GstBusHandler, player);
				if (player->avPlayer.watchID != FALSE)
				{
					INFO_PRINTF("CREATE AV PLAYER COMPLETE\n");
					created = true;
				}
				else
				{
					ERROR_PRINTF("gst_bus_add_watch failed\n");
				}
			}
			else
			{
				ERROR_PRINTF("gst_element_get_bus failed\n");
			}
		}
		else
		{
			ERROR_PRINTF("gst_element_factory_make(playbin2) failed\n");
		}

		
		if (!created)
		{
			ReleasePlayer(player);
			player = NULL;
		}
	}

	return player;
}

static void ReleasePlayer(MultiMediaPlayer *player)
{
	if (player != NULL)
	{
		INFO_PRINTF("\n");
		
		if(player->path != NULL)
		{
			free(player->path);
			player->path = NULL;
		}		
		ReleaseAVPlayer(&player->avPlayer);
		
		free(player);
	}
}

static void ReleaseAVPlayer(AVPlayer *player)
{
	INFO_PRINTF("\n");
	if (player != NULL)
	{

		if (player->watchID != FALSE)
		{
			if (!g_source_remove(player->watchID))
			{
				ERROR_PRINTF("g_source_remove watch id failed\n");
			}
			player->watchID = FALSE;
		}

		if (player->bus != NULL)
		{
			gst_object_unref(player->bus);
		}

		if (player->playbin != NULL)
		{
			gst_object_unref(GST_OBJECT(player->playbin));
		}

		if(player->id3Info!=NULL)
		{
			player->id3Info->albumArt.length =0;
			if(player->id3Info->albumArt.buf != NULL)
			{
				free(player->id3Info->albumArt.buf);
				player->id3Info->albumArt.buf = NULL;
			}
			player->id3Info = NULL;
		}
	}
}

static void SetSourceLocation(GstElement *obj,  GstElement* arg, gpointer userdata)
{
	char * path =(char*)userdata;
	GstElement *source;

	g_object_get(obj,"source", &source , NULL);
	if(source  != NULL)
	{
		INFO_PRINTF("Set Source (%s)\n",  path);
		g_object_set(source , "location", path, NULL);
		g_object_unref(source);
	}
	else
	{
		ERROR_PRINTF("Get Source fail\n")
	}
	(void)arg;
	return;
}

static bool StartPlayer(MultiMediaPlayer *player, uint8_t keepPause)
{
	bool started = false;

	if((player != NULL)&&(player->path != NULL))
	{
		int32_t i, locationPath=0;
		char uriType[32];

		for(i=0; i<32;i++)
		{
			uriType[i] = player->path[i];

			if(i >3)
			{
				if((uriType[i]=='/')
					&&(uriType[i-1]=='/')
					&&(uriType[i-2]==':'))
				{
					uriType[i+1]	= '\0';
					locationPath = i+1;
					INFO_PRINTF("uri Type(%s)\n", uriType);
					break;
				}
			}			
		}

		g_object_set(player->avPlayer.playbin, "uri", uriType, NULL);
		
		(void)g_signal_connect(player->avPlayer.playbin, "source-setup",G_CALLBACK(SetSourceLocation), &player->path[locationPath]);
		
		DEBUG_PRINTF("SET AUDIO SINK\n");
		g_object_set(player->avPlayer.playbin, "audio-sink", player->avPlayer.audioSink, NULL);
		if (player->avPlayer.video)
		{
			DEBUG_PRINTF("SET VIDEO SINK\n");
			g_object_set(player->avPlayer.playbin, "video-sink", player->avPlayer.videoSink, NULL);
		}

		SetAVSync(true);

		UpdateVideoDisplay(false);
		if (s_dualDisplay != 0)
		{
			UpdateDualVideoDisplay();
		}

		if(keepPause == 1)
		{
			player->userPause = true;
		}

		if (ChangePlayerState(player->avPlayer.playbin, GST_STATE_PAUSED))
		{
			GstQuery *query;
			gint64 start, end;
			GstFormat format = GST_FORMAT_TIME;
			bool playSuccess = true;

			query = gst_query_new_seeking (format);
			if (gst_element_query (player->avPlayer.playbin, query))
			{
				gst_query_parse_seeking (query, NULL, &player->seek_enabled, &start, &end);
				if (!player->seek_enabled)
				{
					INFO_PRINTF("Seeking is DISABLED.\n");
				}
				else
				{
					INFO_PRINTF("Seeking enable.\n");
				}
			}
			else
			{
				INFO_PRINTF("Seeking query failed.\n");
			}

			gst_query_unref (query);

			if ((uint32_t)GST_TIME_AS_SECONDS(player->startPos) > 0)
			{
				(void)pthread_mutex_lock(&s_mutex);

				if (player->seek_enabled)
				{
					INFO_PRINTF("Set Seek simple.\n");
					(void)gst_element_seek_simple(player->avPlayer.playbin, GST_FORMAT_TIME,
							(GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
							player->startPos);
				}
				else
				{
					player->startPos=0;
					WARN_PRINTF("This contents is not seekable. So, this contents will start in 0 seconds.\n");
				}

				(void)pthread_mutex_unlock(&s_mutex);
			}


			DEBUG_PRINTF("Check Pause state(%d).\n", keepPause);
			if(keepPause != 1)
			{
				INFO_PRINTF("set GST_STATE_PLAYING.\n");
				if (!ChangePlayerState(player->avPlayer.playbin, GST_STATE_PLAYING))
				{
					GstMessage *msg;
					GstState currentState = GST_STATE_NULL;

					ERROR_PRINTF("Failed to start up player!");

					msg = gst_bus_poll(player->avPlayer.bus, GST_MESSAGE_ERROR, GST_TIMEOUT);
					if (msg != NULL)
					{
						ProcessGstErrorMessage(msg, player->avPlayer.playID);
						gst_message_unref(msg);
					}

					(void)gst_element_get_state(player->avPlayer.playbin, &currentState, NULL, GST_TIMEOUT);
					if( currentState != GST_STATE_NULL)
					{
						ERROR_PRINTF("Current state(%d), Set to NULL state\n", currentState);
						(void)ChangePlayerState(player->avPlayer.playbin, GST_STATE_NULL);
					}

					playSuccess = false;
				}
			}

			player->backward = false;
			player->fastforward = false;

			if(playSuccess == true)
			{
				started = true;
			}
			else
			{
				started = false;
			}
		}
		else
		{
			GstState currentState = GST_STATE_NULL;

			ERROR_PRINTF("Failed to start up player! Can't Pause\n");

			(void)gst_element_get_state(player->avPlayer.playbin, &currentState, NULL, GST_TIMEOUT);
			if( currentState != GST_STATE_NULL)
			{
				ERROR_PRINTF("Current state(%d), Set to NULL state\n", currentState);
				(void)ChangePlayerState(player->avPlayer.playbin, GST_STATE_NULL);
			}
		}
	}
	else
	{
		ERROR_PRINTF("player is NULL\n");
	}

	return started;
}

static void StopPlayer(MultiMediaPlayer **player)
{
	MultiMediaPlayer *stopPlayer;
	INFO_PRINTF("\n");

	(void)pthread_mutex_lock(&s_stopMutex);
	stopPlayer = *player;
	if (stopPlayer != NULL)
	{
		int32_t currentID = stopPlayer->avPlayer.playID;
		StopPlayTimeThread();
		(void)ChangePlayerState(stopPlayer->avPlayer.playbin, GST_STATE_NULL);
		ReleasePlayer(stopPlayer);

		*player = NULL;
		MultiMediaSetResourceStatus(0);
		if (MultiMediaPlayStoppedCB != NULL)
		{
			MultiMediaPlayStoppedCB(currentID);
		}
	}
	else
	{
		INFO_PRINTF("Already Player is NULL\n");
	}

	(void)pthread_mutex_unlock(&s_stopMutex);
}

static void StartPlayTimeThread(void)
{
	int32_t err;

	DEBUG_PRINTF("\n");
	(void)pthread_mutex_lock(&s_timeMutex);

	s_playtimeRun = true;
	err = pthread_create(&s_playtimeThread, NULL, PlayTimeThread, NULL);
	if (err != 0)
	{
		s_playtimeRun = false;
		ERROR_PRINTF("create PlayTime thread failed: error(%d)\n", err);
	}

	(void)pthread_mutex_unlock(&s_timeMutex);
}

static void StopPlayTimeThread(void)
{
	DEBUG_PRINTF("\n");
	(void)pthread_mutex_lock(&s_timeMutex);

	if (s_playtimeRun)
	{
		void *res;
		int32_t err;
		s_playtimeRun = false;
		err = pthread_join(s_playtimeThread, &res);
		if (err != 0)
		{
			ERROR_PRINTF("update playtime thread join faild: error(%d)\n", err);
		}
	}

	(void)pthread_mutex_unlock(&s_timeMutex);
}

static void StartMediaStartThread(void)
{
	int32_t err;

	s_mediastartRun = true;
	err = pthread_create(&s_mediastartThread, NULL, MediaStartThread, NULL);
	if (err != 0)
	{
		s_mediastartRun = false;
		ERROR_PRINTF("create PlayTime thread failed: error(%d)\n", err);
	}
}

static void StopMediaStartThread(void)
{
	if (s_mediastartRun)
	{
		void *res;
		int32_t err;
		s_mediastartRun = false;
		err = pthread_join(s_mediastartThread, &res);
		if (err != 0)
		{
			ERROR_PRINTF("update playtime thread join faild: error(%d)\n", err);
		}
	}
}

#if 0
static GstState GetCurrentPlayerState(GstElement* player)
{
	GstState currentState;
	GstStateChangeReturn ret;

	ret = gst_element_get_state(player, &currentState, NULL, GST_TIMEOUT);
	if(ret == GST_STATE_CHANGE_FAILURE)
	{
		currentState = GST_STATE_VOID_PENDING;
	}

	return currentState;
}
#endif

static bool ChangePlayerState(GstElement* player, GstState state)
{
	bool changed;
	GstStateChangeReturn ret;
	GstState currentState = GST_STATE_NULL;

	INFO_PRINTF("STATE(%d)\n", state);
	(void)pthread_mutex_lock(&s_mutex);

	ret = gst_element_set_state(player, state);
	changed = (ret != GST_STATE_CHANGE_FAILURE);

	/* Sometimes the result is success, but the state does not change.
	   So, must check the state value	*/
	if(changed == 1)
	{
		changed = 0;
		ret = gst_element_get_state(player, &currentState, NULL, GST_TIMEOUT);
		if(ret == GST_STATE_CHANGE_SUCCESS)
		{
			if(currentState == state)
			{
				changed = 1;
			}
		}
	}

	(void)pthread_mutex_unlock(&s_mutex);
	if(changed == 1)
	{
		INFO_PRINTF("CHANGE STATE(%d), CURRENT STATE(%d), %s\n",
									 state, currentState, (currentState == state) ? "SUCCEDDED" : "FAILED");
	}
	return changed;
}

static gboolean GetCurrentPlayerPosition(gint64 *position)
{
	gboolean ret = FALSE;

	MultiMediaPlayer *pPlayer = s_currentPlayer;

	if ((pPlayer != NULL) && (position != NULL) && (pPlayer->async_done != FALSE))
	{
		GstFormat format = GST_FORMAT_TIME;
	
		(void)pthread_mutex_lock(&s_mutex);
		ret = gst_element_query_position(pPlayer->avPlayer.playbin, format, position);
		if (!ret)
		{
			WARN_PRINTF("gst_element_query_position failed\n");
		}

		(void)pthread_mutex_unlock(&s_mutex);
	}
	return ret;
}

static void SetAVSync(bool sink)
{
	if (s_currentPlayer != NULL)
	{
		(void)pthread_mutex_lock(&s_mutex);

		if (s_currentPlayer->avPlayer.video)
		{
			INFO_PRINTF("Set Videosink : %d\n", sink);
			g_object_set(s_currentPlayer->avPlayer.playbin, "mute", sink ? FALSE : TRUE, NULL);
			g_object_set(s_currentPlayer->avPlayer.videoSink, "sync", sink ? TRUE : FALSE, NULL);
		}

		INFO_PRINTF("Set auidosink : %d\n", sink);
		g_object_set(s_currentPlayer->avPlayer.audioSink, "sync", sink ? TRUE : FALSE, NULL);

		(void)pthread_mutex_unlock(&s_mutex);

	}
}

static void UpdateVideoDisplay(bool update)
{
	INFO_PRINTF("x(%u), y(%u), width(%u), height(%u), margin(%u, %u)\n",
									 s_videoInfo.x, s_videoInfo.y,
									 s_videoInfo.width, s_videoInfo.height,
									 s_videoInfo.marginW, s_videoInfo.marginH);
	
	if (s_currentPlayer != NULL)
	{
		if ((s_currentPlayer->avPlayer.video) && (s_currentPlayer->avPlayer.videoSink != NULL))
		{
			(void)pthread_mutex_lock(&s_mutex);

			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.x_start, s_videoInfo.x, NULL);
			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.y_start, s_videoInfo.y, NULL);
			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.width,  s_videoInfo.width - s_videoInfo.marginW, NULL);
			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.heigth, s_videoInfo.height - s_videoInfo.marginH, NULL);

			if (update)
			{
				g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.update, 1, NULL);
			}
			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.aspectratio, 1, NULL);

			(void)pthread_mutex_unlock(&s_mutex);
		}
	}
}

static void UpdateDualVideoDisplay(void)
{
	INFO_PRINTF("Dual) x(%u), y(%u), width(%u), height(%u)\n",
									 s_videoInfo.dual_x, s_videoInfo.dual_y,
									 s_videoInfo.dual_width, s_videoInfo.dual_height);

	if (s_currentPlayer != NULL)
	{
		if ((s_currentPlayer->avPlayer.video) && (s_currentPlayer->avPlayer.videoSink != NULL))
		{
			(void)pthread_mutex_lock(&s_mutex);

			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.dual_x_start, s_videoInfo.dual_x, NULL);
			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.dual_y_start, s_videoInfo.dual_y, NULL);
			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.dual_width,  s_videoInfo.dual_width, NULL);
			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.dual_height, s_videoInfo.dual_height, NULL);
			g_object_set(s_currentPlayer->avPlayer.videoSink, s_videoSinkProperty.dual_update, 1, NULL);

			(void)pthread_mutex_unlock(&s_mutex);
		}
	}
}

#ifdef ENABLE_FF_REW
static void SetForward(MultiMediaPlayer *player, gint64 position, gdouble speed)
{
	if (player != NULL)
	{
		GstEvent *seek_event;

		SetAVSync(false);

		(void)pthread_mutex_lock(&s_mutex);

		seek_event = gst_event_new_seek(speed, GST_FORMAT_TIME,
										(GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
										GST_SEEK_TYPE_SET,
										position,
										GST_SEEK_TYPE_NONE,
										0);
		if (seek_event != NULL)
		{
			s_currentPlayer->async_done = false;
			if (gst_element_send_event(player->avPlayer.playbin, seek_event))
			{
				s_currentPlayer->backward = false;
				s_currentPlayer->fastforward = true;
			}
			else
			{
				(void)fprintf(stderr, "%s: gst_element_send_event failed\n", __FUNCTION__);
			}
		}
		else
		{
			(void)fprintf(stderr, "%s: gst_event_new_seek failed\n", __FUNCTION__);
		}

		(void)pthread_mutex_unlock(&s_mutex);

	}
}

static void SetBackward(MultiMediaPlayer *player, gint64 position, gdouble speed)
{
	if (player != NULL)
	{
		GstEvent *seek_event;

		SetAVSync(false);
		
		(void)pthread_mutex_lock(&s_mutex);

		seek_event = gst_event_new_seek(speed, GST_FORMAT_TIME,
										(GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
										GST_SEEK_TYPE_SET,
										0,
										GST_SEEK_TYPE_NONE,
										position);
		if (seek_event != NULL)
		{
			player->async_done = false;
			if (gst_element_send_event(player->avPlayer.playbin, seek_event))
			{
				player->backward = true;
				player->fastforward = false;
			}
			else
			{
				(void)fprintf(stderr, "%s: gst_element_send_event failed\n", __FUNCTION__);
			}
		}
		else
		{
			(void)fprintf(stderr, "%s: gst_event_new_seek failed\n", __FUNCTION__);
		}
		(void)pthread_mutex_unlock(&s_mutex);
	}
}
#endif

static void ProcessGstErrorMessage(GstMessage *errorMsg, int32_t playID)
{
	gchar *debug = NULL;
	GError *error = NULL;
	gst_message_parse_error(errorMsg, &error, &debug);

	WARN_PRINTF("GST_MESSAGE_ERROR[%s(%d:%d)] \n",error->message,error->domain, error->code);

	if (strncmp(error->message, "Internal data stream error.", 27) == 0)
	{
		MultiMediaErrorOccurred(-1, playID);
	}
	else
	{
		if (error->domain == GST_CORE_ERROR)
		{
			WARN_PRINTF("gstreamer core error\n");
			MultiMediaErrorOccurred(-1,playID);
		}
		else if (error->domain == GST_LIBRARY_ERROR)
		{
			WARN_PRINTF("gstreamer libraray error\n");
			MultiMediaErrorOccurred(-1,playID);
		}
		else if (error->domain == GST_RESOURCE_ERROR)
		{
			WARN_PRINTF("gstreamer resource error\n");

			switch (error->code)
				{
				case GST_RESOURCE_ERROR_TOO_LAZY:
					MultiMediaErrorOccurred(MultiMediaErrorResourceTooLazy,playID);
					break;
				case GST_RESOURCE_ERROR_NOT_FOUND:
					MultiMediaErrorOccurred(MultiMediaErrorResourceNotFound,playID);
					break;
				case GST_RESOURCE_ERROR_BUSY:
					MultiMediaErrorOccurred(MultiMediaErrorResourceBusy,playID);
					break;
				case GST_RESOURCE_ERROR_OPEN_READ:
					MultiMediaErrorOccurred(MultiMediaErrorResourceOpenReadFailed,playID);
					break;
				case GST_RESOURCE_ERROR_OPEN_WRITE:
					MultiMediaErrorOccurred(MultiMediaErrorResourceOpenWriteFailed,playID);
					break;
				case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
					MultiMediaErrorOccurred(MultiMediaErrorResourceOpenRWFailed,playID);
					break;
				case GST_RESOURCE_ERROR_CLOSE:
					MultiMediaErrorOccurred(MultiMediaErrorResourceClosed,playID);
					break;
				case GST_RESOURCE_ERROR_READ:
					MultiMediaErrorOccurred(MultiMediaErrorResourceReadFailed,playID);
					break;
				case GST_RESOURCE_ERROR_WRITE:
					MultiMediaErrorOccurred(MultiMediaErrorResourceWriteFailed,playID);
					break;
				case GST_RESOURCE_ERROR_SEEK:
					MultiMediaErrorOccurred(MultiMediaErrorResourceSeekFailed,playID);
					break;
				case GST_RESOURCE_ERROR_SYNC:
					MultiMediaErrorOccurred(MultiMediaErrorResourceSyncFailed,playID);
					break;
				case GST_RESOURCE_ERROR_SETTINGS:
					MultiMediaErrorOccurred(MultiMediaErrorResourceSettingFailed,playID);
					break;
				case GST_RESOURCE_ERROR_NO_SPACE_LEFT:
					MultiMediaErrorOccurred(MultiMediaErrorResourceNoSpaceLeft,playID);
					break;
				default:
					MultiMediaErrorOccurred(-1,playID);
					break;
				}
		}
		else if (error->domain == GST_STREAM_ERROR)
		{
			WARN_PRINTF("gstreamer stream error\n");
			MultiMediaErrorOccurred(-1,playID);
		}
		else
		{
			if(s_currentPlayer != NULL)
			{
				MultiMediaErrorOccurred(-1,playID);
			}
		}
	}

	if(debug != NULL)
	{
		g_free(debug);
	}
	if(error != NULL)
	{
		g_error_free(error);
	}
}

static void *PlayTimeThread(void *arg)
{
	while (s_playtimeRun)
	{
		usleep(250000);
		MultiMediaPlayer *pPlayer = s_currentPlayer;

		if (pPlayer != NULL)
		{
			gint64 pos;

			if(pPlayer->getduration == false)
			{
				GstFormat format = GST_FORMAT_TIME;
				int32_t hour, min, sec;
				gint64 duration;
				if (gst_element_query_duration(pPlayer->avPlayer.playbin, format, &duration))
				{
					sec = GST_TIME_AS_SECONDS(duration);
					min = sec / 60;
					sec %= 60;
					hour = min / 60;
					min %= 60;

					pPlayer->getduration = true;
					if (MultiMediaTotalTimeChangeCB != NULL)
					{
						MultiMediaTotalTimeChangeCB(hour, min, sec, pPlayer->avPlayer.playID);
					}
				}
				else
				{
					WARN_PRINTF("get duration fail\n");
				}
			}

			if (GetCurrentPlayerPosition(&pos))
			{
				int64_t hour, min, sec, prevSec;
				bool update;
				sec = (int32_t)GST_TIME_AS_SECONDS(pos);
				prevSec = (int32_t)GST_TIME_AS_SECONDS(pPlayer->position);

				if (pPlayer->backward)
				{
					update = ((prevSec - sec) >= 1);
				}
				else if(pPlayer->fastforward)
				{
					update = ((sec - prevSec) >= 1);
				}
				else
				{
					update = (sec != prevSec);
				}

				if (update)
				{
					min = sec / 60;
					sec %= 60;
					hour = min / 60;
					min %= 60;

					if (MultiMediaPlayTimeChangeCB != NULL)
					{
						/* fprintf(stderr, "%s: update time (%d:%d:%d)\n", __FUNCTION__, (uint32_t)hour, (uint32_t)min, (uint32_t)sec); */
						MultiMediaPlayTimeChangeCB((uint32_t)hour, (uint32_t)min, (uint32_t)sec, pPlayer->avPlayer.playID);
					}
					pPlayer->position = pos;
				}
			}
		}
	}	

	(void)arg;
	pthread_exit((void *)"update play time thread exit\n");
}

static void *MediaStartThread(void *arg)
{
	while (s_mediastartRun)
	{
		usleep(1000);
		(void)pthread_mutex_lock(&s_cmdMutex);

		switch (s_currentCmd)
		{
			case MultiMediaCommandPlay:
				ProcessPlayStart(s_playInfo.path,
							s_playInfo.hour, s_playInfo.min, s_playInfo.sec,
							(s_playInfo.content == MultiMediaContentTypeVideo), s_playInfo.id, s_playInfo.keepPause);
				break;
			case MultiMediaCommandStop:
				ProcessPlayStop();
				s_playInfo.id = 0;
				break;
			case MultiMediaCommandPause:
				ProcessPlayPause();
				break;
			case MultiMediaCommandResume:
				ProcessPlayResume();
				break;
			case MultiMediaCommandNormal:
				ProcessPlayNormal();
				break;
			case MultiMediaCommandFastForward:
				ProcessPlayFastForward();
				break;
			case MultiMediaCommandFastBackward:
				ProcessPlayFastBackward();
				break;
			case MultiMediaCommandTurboFastForward:
				ProcessPlayTurboFastForward();
				break;
			case MultiMediaCommandTurboFastBackward:
				ProcessPlayTurboFastBackward();
				break;
			case MultiMediaCommandSeek:
				ProcessPlaySeek(s_playInfo.hour, s_playInfo.min, s_playInfo.sec);
				break;
			default:
				break;
		}
		
		s_currentCmd = TotalMultiMediaCommands;
		
		(void)pthread_mutex_unlock(&s_cmdMutex);

		(void)usleep(1000);
	}	

	(void)arg;
	pthread_exit((void *)"media process thread exit\n");
}

static void ReleasePlayInfo(void)
{
	if(s_playInfo.path != NULL)
	{
		free(s_playInfo.path);
		s_playInfo.path = NULL;
	}
}

static char *CloneString(const char *string)
{
	char *clone = NULL;
	if (string != NULL)
	{
		uint32_t length = strlen(string);
		clone = malloc(length + 1);
		if(clone != NULL)
		{
			(void)memcpy(clone, string, length);
			clone[length] = '\0';
		}
	}

	return clone;
}

static void ProcessPlayStart(const char *path, uint8_t hour, uint8_t min, uint8_t sec, bool video, int32_t playID, uint8_t keepPause)
{
	DEBUG_PRINTF("\n");

	if ((s_currentPlayer == NULL) &&
		(path != NULL))
	{
		bool ret;

		ret = MultiMediaPlayStart(path, hour, min, sec, video, playID, keepPause);
		if(ret == true)
		{
			SetCurrentTime();
		}
		else
		{
			MultiMediaSetResourceStatus(0);
			MultiMediaErrorOccurred(-1, playID);
		}
	}
}

static void ProcessPlayStop(void)
{

	DEBUG_PRINTF("\n");

	if (s_currentPlayer != NULL)
	{
		s_currentPlayer->userStop = true;
		s_currentPlayer->backward = false;
		s_currentPlayer->fastforward = false;
		StopPlayer(&s_currentPlayer);
	}
	else
	{
		MultiMediaSetResourceStatus(0);
		if (MultiMediaPlayStoppedCB != NULL)
		{
			MultiMediaPlayStoppedCB(-1);
		}	
	}
}

static void ProcessPlayPause(void)
{
	DEBUG_PRINTF("\n");
	if (s_currentPlayer != NULL)
	{
		s_currentPlayer->userPause = true;
		if (ChangePlayerState(s_currentPlayer->avPlayer.playbin, GST_STATE_PAUSED))
		{
			s_currentPlayer->backward = false;
			s_currentPlayer->fastforward = false;
		}
		else
		{
			(void)fprintf(stderr, "%s: ChangePlayerState failed\n", __FUNCTION__);
			MultiMediaErrorOccurred(-1, s_currentPlayer->avPlayer.playID);
		}
	}
}

static void ProcessPlayResume(void)
{
	DEBUG_PRINTF("\n");
	if (s_currentPlayer != NULL)
	{
		if (ChangePlayerState(s_currentPlayer->avPlayer.playbin, GST_STATE_PLAYING))
		{
			s_currentPlayer->backward = false;
			s_currentPlayer->fastforward = false;
		}
		else
		{
			(void)fprintf(stderr, "%s: ChangePlayerState failed\n", __FUNCTION__);
			MultiMediaErrorOccurred(-1,s_currentPlayer->avPlayer.playID);
		}
	}
}

static void ProcessPlayNormal(void)
{
	DEBUG_PRINTF("\n");
	if (s_currentPlayer != NULL)
	{
#ifdef ENABLE_FF_REW
		gint64 position;
		GstEvent *seek_event;
#endif

		SetAVSync(true);
#ifdef ENABLE_FF_REW
		if (GetCurrentPlayerPosition(&position))
		{

			(void)pthread_mutex_lock(&s_mutex);

			seek_event = gst_event_new_seek(1.0, GST_FORMAT_TIME,
											(GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
											GST_SEEK_TYPE_SET,
											position,
											GST_SEEK_TYPE_NONE,
											0);
			if (seek_event != NULL)
			{
				s_currentPlayer->async_done = false;
				if (gst_element_send_event(s_currentPlayer->avPlayer.playbin, seek_event))
				{
					s_currentPlayer->backward = false;
					s_currentPlayer->fastforward = false;
				}
				else
				{
					(void)fprintf(stderr, "%s: gst_element_send_event failed\n", __FUNCTION__);
				}
			}
			else
			{
				(void)fprintf(stderr, "%s: gst_event_new_seek failed\n", __FUNCTION__);
			}
			(void)pthread_mutex_unlock(&s_mutex);
		}
#endif
	}
	else
	{
		ERROR_PRINTF("s_currentPlayer is NULL\n");
	}		
}

static void ProcessPlayFastForward(void)
{
	DEBUG_PRINTF("\n");
	if (s_currentPlayer != NULL)
	{
#ifdef ENABLE_FF_REW
		gint64 position;
		
		if (GetCurrentPlayerPosition(&position))
		{
			SetForward(s_currentPlayer, position, FF_REW_SPEED);
		}
#endif
	}
	else
	{
		ERROR_PRINTF("s_currentPlayer is NULL\n");
	}	
}

static void ProcessPlayFastBackward(void)
{
	DEBUG_PRINTF("\n");
	if (s_currentPlayer != NULL)
	{
#ifdef ENABLE_FF_REW
		gint64 position;
		
		if (GetCurrentPlayerPosition(&position))
		{
			SetBackward(s_currentPlayer, position, FF_REW_SPEED);
		}
#endif
	}
	else
	{
		ERROR_PRINTF("s_currentPlayer is NULL\n");
	}	
}

static void ProcessPlayTurboFastForward(void)
{
	DEBUG_PRINTF("\n");
	if (s_currentPlayer != NULL)
	{
#ifdef ENABLE_FF_REW
		gint64 position;
		
		if (GetCurrentPlayerPosition(&position))
		{
			SetForward(s_currentPlayer, position, TURBO_FF_REW_SPEED);
		}
#endif
	}
	else
	{
		ERROR_PRINTF("s_currentPlayer is NULL\n");
	}
}

static void ProcessPlayTurboFastBackward(void)
{
	DEBUG_PRINTF("\n");
	if (s_currentPlayer != NULL)
	{
#ifdef ENABLE_FF_REW
		gint64 position;
		
		if (GetCurrentPlayerPosition(&position))
		{
			SetBackward(s_currentPlayer, position, TURBO_FF_REW_SPEED);
		}
#endif
	}
	else
	{
		ERROR_PRINTF("s_currentPlayer is NULL\n");
	}	
}

static void ProcessPlaySeek(uint8_t hour, uint8_t min, uint8_t sec)
{
	DEBUG_PRINTF("\n");
	if (s_currentPlayer != NULL)
	{
		gint64 position;
		uint32_t totalSec;

		totalSec = s_playInfo.sec + (s_playInfo.min * 60) + (s_playInfo.hour * 3600);
		position = GST_SECOND * totalSec;
		DEBUG_PRINTF("HOUR(%u), MINUTE(%u), SECOND(%u)\n", 
										 hour,
										 min,
										 sec);

		s_currentPlayer->backward = false;
		s_currentPlayer->fastforward = false;

		(void)pthread_mutex_lock(&s_mutex);
		if(s_currentPlayer->seek_enabled)
		{
			if (gst_element_seek_simple(s_currentPlayer->avPlayer.playbin, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), position))
			{
				if (MultiMediaSeekCompletedCB != NULL)
				{
					MultiMediaSeekCompletedCB(hour,min,sec, s_currentPlayer->avPlayer.playID);
				}
			}
		}
		else
		{
			INFO_PRINTF("This contents is not seekable\n");
		}

		(void)pthread_mutex_unlock(&s_mutex);

	}
}

static int32_t SharedMemoryInitialize(void)
{
	int32_t ret = 0;
	int32_t shm_id;
	void *shm_addr;

	shm_id = shmget((key_t)KEY_NUM, (size_t)MEM_SIZE, 0666 |IPC_CREAT);

	if(shm_id == -1)
	{
		ERROR_PRINTF("shmget failed\n");
		ret = -1;
	}
	else
	{
		s_shm_id = shm_id;
		shm_addr = shmat(shm_id, (void *)0, 0);
		if(shm_addr == (void *)-1)
		{
			ERROR_PRINTF("shmat failed\n");
			ret = -1;
		}
		else
		{
			s_shm_addr = shm_addr;
		}
	}
	return ret;
}

static int32_t SharedMemoryRelease(void)
{
	int32_t ret = 0;
	void *shm_addr;
	int32_t shm_id, shm_dt, shm_ctl;
	shm_addr = s_shm_addr;
	shm_id = s_shm_id;
	shm_dt = shmdt(shm_addr);
	if(shm_dt == -1)
	{
		ERROR_PRINTF("failed distribute shared memory\n");
		ret = -1;
	}
	else
	{
		shm_ctl = shmctl(shm_id, IPC_RMID, 0);
		if(shm_ctl == -1)
		{
			ERROR_PRINTF("failed remove shared momory \n");
			ret = -1;
		}
		else
		{
			s_shm_id = 0;
			s_shm_addr = NULL;
		}
	}
	return ret;
}

int32_t getCurrentPlayID(void)
{
	int32_t ret = 0;

	ret = s_playInfo.id;

	return ret;
}

