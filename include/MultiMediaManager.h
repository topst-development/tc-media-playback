/****************************************************************************************
 *   FileName    : MultiMediaManager.h
 *   Description : Telechips MultiMedia Manager header
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
#ifndef MULTI_MEDIA_MANAGER_H
#define MULTI_MEDIA_MANAGER_H

#include "TCMultiMediaType.h"

#define MAX_SINK_DEVICE_NAME		32
#define DEFAULT_AUDIO_SINK_NAME		"alsasink"
#define ALSA_DEFAULT_DEVICE_NAME		"plug:mainvol"
#define VIDEO_SINK_NAME				"v4l2sink"
#define V4L_DEFAULT_DEVICE_NAME		"/dev/video10"

#define GST_TIMEOUT		(2*GST_SECOND)

#define KEY_NUM							(3443)
#define MEM_SIZE						(8*1024*1024)


#define ERROR_PRINTF(format, arg...) \
		(void)TCLog(TCLogLevelError, "%s: "format"", __FUNCTION__, ##arg);

#define WARN_PRINTF(format, arg...) \
		(void)TCLog(TCLogLevelWarn, "%s: "format"", __FUNCTION__, ##arg);

#define INFO_PRINTF(format, arg...) \
		(void)TCLog(TCLogLevelInfo, "%s: "format"", __FUNCTION__, ##arg);

#define DEBUG_PRINTF(format, arg...) \
		(void)TCLog(TCLogLevelDebug, "%s: "format"", __FUNCTION__, ##arg);


typedef void (*MultiMediaPlayStarted_cb)(int32_t playID);
typedef void (*MultiMediaPlayPaused_cb)(int32_t playID);
typedef void (*MultiMediaPlayStopped_cb)(int32_t playID);
typedef void (*MultiMediaPlayTimeChange_cb)(uint32_t hour, uint32_t min, uint32_t sec, int32_t playID);
typedef void (*MultiMediaTotalTimeChange_cb)(uint32_t hour, uint32_t min, uint32_t sec, int32_t playID);
typedef void (*MultiMediaID3InformationAll_cb)(const char * title,const char * artist, const char * album, const char * genre, int32_t playID);
typedef void (*MultiMediaID3Information_cb)(MetaCategory category,const  char * info,int32_t playID);
typedef void (*MultiMediaAlbumArt_cb)(int32_t playID, uint32_t length);
typedef void (*MultiMediaPlayCompleted_cb)(int32_t playID);
typedef void (*MultiMediaSeekCompleted_cb)(uint8_t hour, uint8_t min, uint8_t sec,int32_t playID);
typedef void (*MultiMediaErrorOccurred_cb)(int32_t code, int32_t playID);
typedef void (*MultiMediaSamplerate_cb)(int32_t samplerate, int32_t playID);


typedef struct stMultiMediaEventCB {
	MultiMediaPlayStarted_cb			MultiMediaPlayStartedCB;
	MultiMediaPlayPaused_cb				MultiMediaPlayPausedCB;
	MultiMediaPlayStopped_cb			MultiMediaPlayStoppedCB;
	MultiMediaPlayTimeChange_cb			MultiMediaPlayTimeChangeCB;
	MultiMediaTotalTimeChange_cb		MultiMediaTotalTimeChangeCB;
	MultiMediaID3Information_cb			MultiMediaID3InformationCB;
	MultiMediaAlbumArt_cb				MultiMediaAlbumArtCB;
	MultiMediaPlayCompleted_cb			MultiMediaPlayCompletedCB;
	MultiMediaSeekCompleted_cb			MultiMediaSeekCompletedCB;
	MultiMediaErrorOccurred_cb			MultiMediaErrorOccurredCB;
	MultiMediaSamplerate_cb				MultiMediaSamplerateCB;
} TcMultiMediaEventCB;

void MultiMediaSetDebugLevel(int32_t level);
int32_t MultiMediaInitialize(void);
int32_t MultiMediaGetResourceStatus(void);
void SetEventCallBackFunctions(TcMultiMediaEventCB *cb);
void MultiMediaRelease(void);
void MultiMediaSetMargin(uint32_t width, uint32_t height);
void MultiMediaSetVideoDisplayInfo(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void MultiMediaSetDualVideoDisplayInfo(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
int32_t MultiMediaPlayStartAV(uint8_t content, const char *path, uint8_t hour, uint8_t min, uint8_t sec, int32_t id, uint8_t keepPause);
int32_t MultiMediaPlayStop(int32_t id);
int32_t MultiMediaPlayPause(int32_t id);
int32_t MultiMediaPlayResume(int32_t id);
void MultiMediaPlayNormal(void);
void MultiMediaPlayFastForward(void);
void MultiMediaPlayFastBackward(void);
void MultiMediaPlayTurboFastForward(void);
void MultiMediaPlayTurboFastBackward(void);
int32_t MultiMediaPlaySeek(uint8_t hour, uint8_t min, uint8_t sec, int32_t id);

int32_t MultiMediaGetAlbumArt(uint8_t **buffer, uint32_t *length);
void MultiMediaSetAudioSink(const char *audioSink,const char *device);
void MultiMediaSetV4LDevice(const char * device);
void MultiMediaErrorOccurred(int32_t code, int32_t playID);
int32_t getCurrentPlayID(void);

#endif

