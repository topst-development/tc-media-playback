/****************************************************************************************
 *   FileName    : TCTime.c
 *   Description : Telechips Time Utility
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
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#define TC_LOCALTIME(__time) localtime_r(&(__time), &g_tm)
#include "TCTime.h"

static time_t g_time;
static int32_t g_us;
static struct tm g_tm;

void SetTime(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, float64_t second)
{
	struct tm tmBuf;
	tmBuf.tm_year = year - 1900;
	tmBuf.tm_mon = month - 1;
	tmBuf.tm_mday = day;
	tmBuf.tm_hour = hour;
	tmBuf.tm_min = minute;
	tmBuf.tm_sec = (int32_t)second;
	g_us = (int32_t)(((second - floor(second)) * 1000000) + 0.5);
	if (g_us >= 1000000)
	{
		tmBuf.tm_sec++;
		g_us %= 1000000;
	}
	g_time = mktime(&tmBuf);
	(void)TC_LOCALTIME(g_time);
}

void SetCurrentTime(void)
{
	struct timeval timeVal;
	(void)gettimeofday(&timeVal, NULL);
	g_time = timeVal.tv_sec;
	g_us = timeVal.tv_usec;

	if (g_us >= 1000000)
	{
		g_time++;
		g_us %= 1000000;
	}
	TC_LOCALTIME(g_time);
}

int32_t GetYear(void)
{
	return g_tm.tm_year + 1900;
}

int32_t GetMonth(void)
{
	return g_tm.tm_mon + 1;
}

int32_t GetDay(void)
{
	return g_tm.tm_mday;
}

int32_t GetHour(void)
{
	return g_tm.tm_hour;
}

int32_t GetMinute(void)
{
	return g_tm.tm_min;
}

float64_t GetSecond(void)
{
	return g_tm.tm_sec + (g_us / 1000000.0);
}

float64_t GetSecondsUntilNow(void)
{
	struct timeval now;
	(void)gettimeofday(&now, NULL);
	return now.tv_sec - g_time + ((now.tv_usec - g_us) / 1000000.0);
}

