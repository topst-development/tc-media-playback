/****************************************************************************************
 *   FileName    : TCTime.h
 *   Description : Telechips Time utility header
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
#ifndef TC_TIME_H_
#define TC_TIME_H_

typedef double float64_t;

void SetTime(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t minute, float64_t second);
void SetCurrentTime(void);
int32_t GetYear(void);
int32_t GetMonth(void);
int32_t GetDay(void);
int32_t GetHour(void);
int32_t GetMinute(void);
float64_t GetSecond(void);
float64_t GetSecondsUntilNow(void);
 
#endif

