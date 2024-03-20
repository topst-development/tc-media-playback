/****************************************************************************************
 *   FileName    : TCMultiMediaType.h
 *   Description : Telechips Multimedia Type header
 ****************************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This library contains confidential information of Telechips.
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
#ifndef TC_MULTI_MEDIA_TYPE_H
#define TC_MULTI_MEDIA_TYPE_H

int32_t s_enableDebug;

typedef enum {
	MetaCategoryTitle,
	MetaCategoryArtist,
	MetaCategoryAlbum,
	MetaCategoryGenre,
	TotalMetaCategories
} MetaCategory;

typedef enum {
	MultiMediaErrorResourceTooLazy,
	MultiMediaErrorResourceNotFound,
	MultiMediaErrorResourceBusy,
	MultiMediaErrorResourceOpenReadFailed,
	MultiMediaErrorResourceOpenWriteFailed,
	MultiMediaErrorResourceOpenRWFailed,
	MultiMediaErrorResourceClosed,
	MultiMediaErrorResourceReadFailed,
	MultiMediaErrorResourceWriteFailed,
	MultiMediaErrorResourceSeekFailed,
	MultiMediaErrorResourceSyncFailed,
	MultiMediaErrorResourceSettingFailed,
	MultiMediaErrorResourceNoSpaceLeft,
	TotalMultiMediaErrors
} MultiMediaError;

typedef enum {
	MultiMediaContentTypeAudio,
	MultiMediaContentTypeVideo,
	TotalMultiMediaContentTypes
} MultiMediaContentType;

#endif

