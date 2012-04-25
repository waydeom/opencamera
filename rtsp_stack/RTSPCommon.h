/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2010 Live Networks, Inc.  All rights reserved.
// Common routines used by both RTSP clients and servers
// C++ header

#ifndef _RTSP_COMMON_HH
#define _RTSP_COMMON_HH

#ifndef _BOOLEAN_HH
#include "Boolean.h"
#endif
#ifndef __win32__
#include <time.h>
#endif

#if defined(__WIN32__) || defined(_WIN32) || defined(_QNX4)
#define _strncasecmp _strnicmp
#define snprintf _snprintf
#else
#define _strncasecmp strncasecmp
#endif

#define RTSP_PARAM_STRING_MAX 200

typedef enum StreamingMode {
	RTP_UDP,
		RTP_TCP,
		RAW_UDP
} StreamingMode;

Boolean parseRTSPRequestString(char const *reqStr, unsigned reqStrSize,
			       char *resultCmdName,
			       unsigned resultCmdNameMaxSize,
			       char* resultURLPreSuffix,
			       unsigned resultURLPreSuffixMaxSize,
			       char* resultURLSuffix,
			       unsigned resultURLSuffixMaxSize,
			       char* resultCSeq,
			       unsigned resultCSeqMaxSize);

Boolean parseRTSPResponseString(char const* reqStr,
								unsigned reqStrSize,
								char* resultCode,
								unsigned resultCodeMaxSize,
								char* resultStatus,
								unsigned resultStatusMaxSize,
								char* contentType,
								unsigned contentTypeMaxSize,
								char* challenge,
								unsigned challengeMaxSize,
								unsigned int* contentLength,
								unsigned char** content);

Boolean parseRangeParam(char const* paramStr, double& rangeStart, double& rangeEnd);
Boolean parseRangeHeader(char const* buf, double& rangeStart, double& rangeEnd);
Boolean parseSpecifiedStringParam(char const* buf, char *find_str, char *specified_str, Boolean specified_str_len);
Boolean parseRTSPGetPostString(char const *reqStr,
							   unsigned reqStrSize,
							   char *reqMethod,
							   unsigned reqMethodSize,
							   char *cookie,
							   unsigned cookieSize,
							   char *contentType,
							   unsigned contentTypeSize,
							   char *pragma,
							   unsigned pragmaSize,
							   char *cacheControl,
							   unsigned cacheControlSize);

void get_DirOrFile(char const *dir, char *response, int response_code);
void parseTimeRange_getStartFileDir(char const *buf, char *time, char *time_range_start[]);
void parseTime_GetEndTime(char *time1, char *time2, char *year, char *month, char *day, char *hour, char *minute, char *second);
void parseTime_searchTimeDir(char *response, char *dir, time_t start_time_day,
							time_t end_time_day, time_t start_time_sec, time_t end_time_sec);
void parseTokenFromURLSuffix(char *suffixWithToken, char *suffix, char *token);

void parseTransportHeader(char const* buf,
								 StreamingMode& streamingMode,
								 char*& streamingModeString,
								 char*& destinationAddressStr,
								 unsigned char& destinationTTL,
								 unsigned short& clientRTPPortNum, // if UDP
								 unsigned short& clientRTCPPortNum, // if UDP
								 unsigned char& rtpChannelId, // if TCP
								 unsigned char& rtcpChannelId // if TCP
								 );

Boolean parsePlayNowHeader(char const* buf);
int HexToDec(char *);

#endif
