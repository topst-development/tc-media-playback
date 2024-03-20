/****************************************************************************************
 *   FileName    : MediaPlaybackDBus.h
 *   Description : Telechips MediaPlayback D-Bus header
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
#ifndef MEDIAPLAYBACK_DBUS_H
#define MEDIAPLAYBACK_DBUS_H

#ifdef __cplusplus
extern "C" {
#endif

void MediaPlaybackDBusInitialize(void);
void MediaPlaybackDBusRelease(void);

void MediaPlaybackEmitPlaying(int32_t playID);
void MediaPlaybackEmitStopped(int32_t playID);
void MediaPlaybackEmitPaused(int32_t playID);
void MediaPlaybackEmitDuration(uint32_t hour, uint32_t min, uint32_t sec, int32_t playID);
void MediaPlaybackEmitPlayPosition(uint32_t hour, uint32_t min, uint32_t sec, int32_t playID);
void MediaPlaybackEmitPlayTaginfo(MetaCategory category,const  char * info, int32_t playID);
void MediaPlaybackEmitAlbumart(int32_t playID, uint32_t length);
void MediaPlaybackEmitPlayEnded(int32_t playID);
void MediaPlaybackEmitSeekCompleted(uint8_t hour, uint8_t min, uint8_t sec, int32_t playID);
void MediaPlaybackEmitError(int32_t errCode, int32_t playID);
void MediaPlaybackEmitSamplerate(int32_t samplerate, int32_t playID);


#ifdef __cplusplus
}
#endif

#endif

