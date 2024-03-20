/****************************************************************************************
 *   FileName    : Main.c
 *   Description : Telechips Media Playback Main
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
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#ifdef USE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif
#include "TCLog.h"
#include "MultiMediaManager.h"
#include "MediaPlaybackDBus.h"

#define STACK_BUF_SIZE 100

static void SignalHandler(int32_t sig);
static void Daemonize(void);
static int32_t InitializeMultimediaInterface(void);
static void usage(void);

static GMainLoop *s_mainLoop = NULL;

static const char *s_pid_file = "/var/run/TCMediaPlayback.pid";

int main(int argc, char *argv[])
{
	int32_t ret = 1;
	int32_t idx;
	char *audioSink = NULL;
	char *audioDevice = NULL;
	char *videoDevice = NULL;
	int32_t daemonize = 1;
	int32_t debugLevel = TCLogLevelWarn;

	if (argc > 1)
	{
		for (idx = 1; idx < argc; idx++)
		{
			if (strncmp(argv[idx], "--debug", 7) == 0)
			{
				if(argv[idx+1] != NULL)
				{
					debugLevel = atoi(argv[idx+1]);
				}
				else
				{
					ret = 0;
				}
			}
			else if (strncmp(argv[idx], "--no-daemon", 11) == 0)
			{
				daemonize = 0;
			}
			else if (strncmp(argv[idx], "--audio-sink", 12) == 0)
			{
				if(argv[idx+1] != NULL)
				{
					audioSink = argv[idx+1];
				}
				else
				{
					ret = 0;
				}
			}
			else if (strncmp(argv[idx], "--audio-device", 14) == 0)
			{
				if(argv[idx+1] != NULL)
				{
					audioDevice =argv[idx+1];
				}
				else
				{
					ret = 0;
				}
			}
			else if (strncmp(argv[idx], "--video-device", 14) == 0)
			{
				if(argv[idx+1] != NULL)
				{
					videoDevice =argv[idx+1];
				}
				else
				{
					ret = 0;
				}
			}
			else if((strncmp(argv[idx], "--help", 6) == 0)||
				(strncmp(argv[idx], "-h", 2) == 0))
			{
				ret = 0;
			}
			else
			{
				;
			}

			if(ret == 0)
			{
				usage();
				break;
			}
		}
	}

	if(ret != 0)
	{
		if (daemonize)
		{
			Daemonize();
		}

		(void)signal(SIGINT, SignalHandler);
		(void)signal(SIGTERM, SignalHandler);
		(void)signal(SIGABRT, SignalHandler);

		s_mainLoop = g_main_loop_new(NULL, FALSE);

		if (s_mainLoop != NULL)
		{
			TCLogInitialize("TC_MEDIA_PLAYBACK", NULL, 1);
			TCEnableLog(1);
			if(debugLevel > TCLogLevelDebug)
			{
				debugLevel = TCLogLevelDebug;
			}
			TCLogSetLevel(debugLevel);

			MediaPlaybackDBusInitialize();
			(void)setenv("PULSE_PROP_media.role", "media", 1);

#ifdef USE_SYSTEMD
				sd_notify(0, "READY=1");
#endif

			ret = InitializeMultimediaInterface();
			if (ret ==1)
			{
				if(audioSink !=NULL)
				{
					MultiMediaSetAudioSink(audioSink, audioDevice);
				}

				if(videoDevice !=NULL)
				{
					MultiMediaSetV4LDevice(videoDevice);
				}

				g_main_loop_run(s_mainLoop);
				g_main_loop_unref(s_mainLoop);
				s_mainLoop = NULL;

				MultiMediaRelease();
				MediaPlaybackDBusRelease();

			}
			else
			{
				ERROR_PRINTF("MediaPlayback initialize failed\n");
				ret = -1;
			}
		}
		else
		{
			ERROR_PRINTF("g_main_loop_new failed\n");
			ret = -1;
		}

		if (access(s_pid_file, F_OK) == 0)
		{
			if (unlink(s_pid_file) != 0)
			{
				ERROR_PRINTF("delete pid file failed");
			}
		}
	}
	return ret;
}

static void SignalHandler(int32_t sig)
{
	int i, nptrs;
	void *stacK_buffer[STACK_BUF_SIZE];
	char **symbol_strings;

	if ( sig == SIGABRT )
	{
		ERROR_PRINTF("%d\n", sig);

		nptrs = backtrace(stacK_buffer, STACK_BUF_SIZE);
		ERROR_PRINTF("backtrace() returned %d addresses\n", nptrs);

		symbol_strings = backtrace_symbols(stacK_buffer, nptrs);
		if (symbol_strings == NULL) {
			ERROR_PRINTF("backtrace_symbols");
		} else {
			for (i = 0; i < nptrs; i++)
			{
				ERROR_PRINTF("%s\n", symbol_strings[i]);
			}

			free(symbol_strings);
		}
	}

	if (s_mainLoop != NULL)
	{
		g_main_loop_quit(s_mainLoop);
	}
}

static void Daemonize(void)
{
	pid_t pid;
	FILE *pid_fp;

	/* create child process */
	pid = fork();

	/* fork failed */
	if (pid < 0)
	{
		ERROR_PRINTF("fork failed\n");
		exit(1);
	}

	/* parent process */
	if (pid > 0)
	{
		/* exit parent process for daemonize */
		exit(0);
	}

	/* umask the file mode */
	(void)umask(0);

	pid_fp = fopen(s_pid_file, "w+");
	if (pid_fp != NULL)
	{
		(void)fprintf(pid_fp, "%d\n", getpid());
		(void)fclose(pid_fp);
	}
	else
	{
		ERROR_PRINTF("pid file open failed");
	}

	/* set new session */
	if (setsid() < 0)
	{
		ERROR_PRINTF("set new session failed\n");
		exit(1);
	}

	/* change the current working directory for safety */
	if (chdir("/") < 0)
	{
		ERROR_PRINTF("change directory failed\n");
		exit(1);
	}
}

static int32_t InitializeMultimediaInterface(void)
{
	int32_t ret;

	TcMultiMediaEventCB cb;

	ret = MultiMediaInitialize();
	if(ret == 1)
	{
		cb.MultiMediaPlayStartedCB = MediaPlaybackEmitPlaying;
		cb.MultiMediaPlayPausedCB = MediaPlaybackEmitPaused;
		cb.MultiMediaPlayStoppedCB = MediaPlaybackEmitStopped;
		cb.MultiMediaPlayTimeChangeCB = MediaPlaybackEmitPlayPosition;
		cb.MultiMediaTotalTimeChangeCB = MediaPlaybackEmitDuration;
		cb.MultiMediaID3InformationCB = MediaPlaybackEmitPlayTaginfo;
		cb.MultiMediaAlbumArtCB = MediaPlaybackEmitAlbumart;
		cb.MultiMediaPlayCompletedCB = MediaPlaybackEmitPlayEnded;
		cb.MultiMediaSeekCompletedCB = MediaPlaybackEmitSeekCompleted;
		cb.MultiMediaErrorOccurredCB = MediaPlaybackEmitError;
		cb.MultiMediaSamplerateCB =  MediaPlaybackEmitSamplerate;
		
		SetEventCallBackFunctions(&cb);
	}
	return ret;
}

static void usage (void)
{
	(void)fprintf(stderr, "media-playback : Telechips mulitmedia playback daemon.\n");
	(void)fprintf(stderr, "Usage :  media-playback [OPTIONS] \n");
	(void)fprintf(stderr, "OPTIONS include : \n");
	(void)fprintf(stderr, "\t--debug loglevel : set debug log level \n");
	(void)fprintf(stderr, "\t--audio-sink sink-name : set GSteamer audio sink, default (%s) \n",DEFAULT_AUDIO_SINK_NAME);
	(void)fprintf(stderr, "\t--audio-device device-name : set device of audio-sink, default (%s)\n", ALSA_DEFAULT_DEVICE_NAME);
	(void)fprintf(stderr, "\t--vidoe-device device-name : set device of video-sink(v4l2sink) device, default (%s)\n", V4L_DEFAULT_DEVICE_NAME);
	(void)fprintf(stderr, "\t--no-daemon : Don't fork\n");
	(void)fprintf(stderr, "\t--help or -h : show this message\n");
}

