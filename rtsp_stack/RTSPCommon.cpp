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
// Implementation

#include "RTSPCommon.h"
#include "Locale.h"
#include "strDup.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef __WIN32__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif
#include "Debug.h"

char * eat_white(char* s)
{
	while (1)
	{
		if (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
			s ++;
		else
			break;
	}
	return s;
}

char * next_white(char* s)
{
	while (1)
	{
		if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n' && *s != 0)
			s ++;
		else
			break;
	}
	return s;
}

char* next_line(char* s)
{
	while (1)
	{
		if (*s != '\r' && *s != '\n' && *s != 0)
			s ++;
		else
			break;
	}
	if (*s == '\r')
		return s + 2;
	else if (*s == '\n')
		return s + 1;
	else
		return s;
}

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
		unsigned char** content) {
	char* p, *q, *u, *v;

	*contentLength = 0;

	// First line, parse the result code and result status
	p = eat_white((char*)reqStr);
	if (strncmp(p, "RTSP/1.0", 8) != 0)
		return False;
	q = next_white(p);
	p = eat_white(q);
	q = next_white(p);
	if (q - p >= (int)resultCodeMaxSize)
		return False;
	memcpy(resultCode, p, q-p);
	resultCode[q-p] = 0;

	p = eat_white(q);
	q = next_line(p);
	if (q - p >= (int)resultStatusMaxSize)
		return False;
	memcpy(resultStatus, p, q-p-2);
	resultStatus[q-p-2] = 0;

	while (1)
	{
		p = q;
		q = next_line(p);
		if (strncmp(p, "Content-type", strlen("Content-type")) == 0 ||
				strncmp(p, "Content-Type", strlen("Content-Type")) == 0)
		{
			// Parse content-type header
			u = eat_white(next_white(p));
			v = next_white(u);
			memcpy(contentType, u, v-u);
			contentType[v-u] = 0;
		}
		else if (strncmp(p, "Content-length", strlen("Content-length")) == 0 ||
				strncmp(p, "Content-Length", strlen("Content-Length")) == 0)
		{
			// Parse content-length header
			u = eat_white(next_white(p));
			*contentLength = ::atoi(u);
		}
		else if (strncmp(p, "Challenge", strlen("Challenge")) == 0)
		{
			// Parse challenge header
			u = eat_white(next_white(p));
			v = next_white(u);
			memcpy(challenge, u, v-u);
			challenge[v-u] = 0;
		}
		if (*q == 0 || *q == '\r')
			break;
	}
	if (*q == '\r')
		q += 2;
	if (*contentLength > 0 && *q != 0)
	{
		unsigned char* temp = (unsigned char*)malloc(*contentLength);
		memcpy(temp, q, *contentLength);
		*content = temp;
	}
	return True;
}


Boolean parseRTSPRequestString(char const* reqStr,
		unsigned reqStrSize,
		char* resultCmdName,
		unsigned resultCmdNameMaxSize,
		char* resultURLPreSuffix,
		unsigned resultURLPreSuffixMaxSize,
		char* resultURLSuffix,
		unsigned resultURLSuffixMaxSize,
		char* resultCSeq,
		unsigned resultCSeqMaxSize) {
	// This parser is currently rather dumb; it should be made smarter #####

	// Read everything up to the first space as the command name:
	Boolean parseSucceeded = False;
	unsigned i;
	for (i = 0; i < resultCmdNameMaxSize-1 && i < reqStrSize; ++i) {
		char c = reqStr[i];
		if (c == ' ' || c == '\t') {
			parseSucceeded = True;
			break;
		}

		resultCmdName[i] = c;
	}
	resultCmdName[i] = '\0';
	if (!parseSucceeded) return False;

	// Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
	unsigned j = i+1;
	while (j < reqStrSize && (reqStr[j] == ' ' || reqStr[j] == '\t')) ++j; // skip over any additional white space
	for (; (int)j < (int)(reqStrSize-8); ++j) {
		if ((reqStr[j] == 'r' || reqStr[j] == 'R')
				&& (reqStr[j+1] == 't' || reqStr[j+1] == 'T')
				&& (reqStr[j+2] == 's' || reqStr[j+2] == 'S')
				&& (reqStr[j+3] == 'p' || reqStr[j+3] == 'P')
				&& reqStr[j+4] == ':' && reqStr[j+5] == '/') {
			j += 6;
			if (reqStr[j] == '/') {
				// This is a "rtsp://" URL; skip over the host:port part that follows:
				++j;
				while (j < reqStrSize && reqStr[j] != '/' && reqStr[j] != ' ') ++j;
			} else {
				// This is a "rtsp:/" URL; back up to the "/":
				--j;
			}
			i = j;
			break;
		}
	}

	// Look for the URL suffix (before the following "RTSP/"):
	parseSucceeded = False;
	for (unsigned k = i+1; (int)k < (int)(reqStrSize-5); ++k) {
		if (reqStr[k] == 'R' && reqStr[k+1] == 'T' &&
				reqStr[k+2] == 'S' && reqStr[k+3] == 'P' && reqStr[k+4] == '/') {
			while (--k >= i && reqStr[k] == ' ') {} // go back over all spaces before "RTSP/"
			unsigned k1 = k;
			while (k1 > i && reqStr[k1] != '/') --k1;
			// the URL suffix comes from [k1+1,k]

			// Copy "resultURLSuffix":
			if (k - k1 + 1 > resultURLSuffixMaxSize) return False; // there's no room
			unsigned n = 0, k2 = k1+1;
			while (k2 <= k) resultURLSuffix[n++] = reqStr[k2++];
			resultURLSuffix[n] = '\0';

			// Also look for the URL 'pre-suffix' before this:
			unsigned k3 = (k1 == 0) ? 0 : --k1;
			while (k3 > i && reqStr[k3] != '/') --k3;
			// the URL pre-suffix comes from [k3+1,k1]

			// Copy "resultURLPreSuffix":
			if (k1 - k3 + 1 > resultURLPreSuffixMaxSize) return False; // there's no room
			n = 0; k2 = k3+1;
			while (k2 <= k1) resultURLPreSuffix[n++] = reqStr[k2++];
			resultURLPreSuffix[n] = '\0';

			i = k + 7; // to go past " RTSP/"
			parseSucceeded = True;
			break;
		}
	}
	if (!parseSucceeded) return False;

	// Look for "CSeq:", skip whitespace,
	// then read everything up to the next \r or \n as 'CSeq':
	parseSucceeded = False;
	for (j = i; (int)j < (int)(reqStrSize-5); ++j) {
		if (reqStr[j] == 'C' && reqStr[j+1] == 'S' && reqStr[j+2] == 'e' &&
				reqStr[j+3] == 'q' && reqStr[j+4] == ':') {
			j += 5;
			unsigned n;
			while (j < reqStrSize && (reqStr[j] ==  ' ' || reqStr[j] == '\t')) ++j;
			for (n = 0; n < resultCSeqMaxSize-1 && j < reqStrSize; ++n,++j) {
				char c = reqStr[j];
				if (c == '\r' || c == '\n') {
					parseSucceeded = True;
					break;
				}

				resultCSeq[n] = c;
			}
			resultCSeq[n] = '\0';
			break;
		}
	}
	if (!parseSucceeded) return False;

	return True;
}

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
		unsigned cacheControlSize) {

	char const *movePtr = reqStr;
	int i = 0, j = 0;

	memset(reqMethod, 0x0, reqMethodSize);
	memset(cookie, 0x0, cookieSize);
	memset(contentType, 0x0, contentTypeSize);
	memset(pragma, 0x0, pragmaSize);
	memset(cacheControl, 0x0, cacheControlSize);
	while(reqStr[i] != '\0' && i < reqStrSize)
	{
		if(reqStr[i] == ' ' && reqStr[i] == '\t')	
		{
			i++;
		}	
		else
		{
			movePtr = &reqStr[i];
			break;
		}
	}
	i = 0;
	while( movePtr[i] != ' ' &&  movePtr[i] != '\t' && movePtr[i] != '\r' && movePtr[i]!= '\n' && movePtr[i] != 0)
	{
		Debug(ckite_log_message, "i = %d, movePtr[i] = %c\n", i, movePtr[i]);
		reqMethod[i] = movePtr[i];
		i++;
	}
	reqMethod[i] = '\0';
	Debug(ckite_log_message, "reqMethod = %s\n", reqMethod);

	if (strcmp(reqMethod, "GET") == 0) {
		parseSpecifiedStringParam((char const*)reqStr, contentType, "Accept:", strlen("Accept:"));
	} else if (strcmp(reqMethod, "POST") == 0){
		parseSpecifiedStringParam((char const*)reqStr, contentType, "Content-Type:", strlen("Content-Type:"));
	} else {
		return 0;
	}
	parseSpecifiedStringParam((char const*)reqStr, cookie, "x-sessioncookie:", strlen("x-sessioncookie:"));
	parseSpecifiedStringParam((char const*)reqStr, pragma, "Pragma:", strlen("Pragma:"));
	parseSpecifiedStringParam((char const*)reqStr, cacheControl, "Cache-Control:", strlen("Cache-Control:"));

	return 1;
}

Boolean parseRangeParam(char const* paramStr, double& rangeStart, double& rangeEnd) {
	double start, end;
	Locale l("C", LC_NUMERIC);
	if (sscanf(paramStr, "npt = %lf - %lf", &start, &end) == 2) {
		rangeStart = start;
		rangeEnd = end;
	} else if (sscanf(paramStr, "npt = %lf -", &start) == 1) {
		rangeStart = start;
		rangeEnd = 0.0;
	} else {
		return False; // The header is malformed
	}

	return True;
}

Boolean parseRangeHeader(char const* buf, double& rangeStart, double& rangeEnd) {
	// First, find "Range:"
	while (1) {
		if (*buf == '\0') return False; // not found
		if (_strncasecmp(buf, "Range: ", 7) == 0) break;
		++buf;
	}

	// Then, run through each of the fields, looking for ones we handle:
	char const* fields = buf + 7;
	while (*fields == ' ') ++fields;
	return parseRangeParam(fields, rangeStart, rangeEnd);
}


Boolean parseSpecifiedStringParam(char const* buf, char *find_str, char *specified_str, Boolean specified_str_len)
{
	// First, find "Content-type:"
	while (1) {
		if (*buf == '\0') return False; // not found
		if (_strncasecmp(buf, specified_str, specified_str_len) == 0) break;
		++buf;
	}
	// Then, run through each of the fields, looking for ones we handle:
	char const* fields = buf + specified_str_len;
	while (*fields == ' ') ++fields;

	Boolean i = 0;
	while (1) {	   	
		if (fields[i] == '\r' && fields[i+1] == '\n'){
			find_str[i] = '\0';
			return i;
		}
		find_str[i] = fields[i];
		++i;
	}
	return True;
}

void get_DirOrFile(char const *dir, char *response, int response_code)
{
#ifndef __WIN32__
	struct dirent *dp;
	DIR *dfd;
	struct stat stbuf;
	char *p = response;

	if(stat(dir,&stbuf) == -1)
		return ;
	if(S_ISDIR(stbuf.st_mode) == 0)
		return ;
	if((dfd = opendir(dir)) == NULL)
		return ;
	while((dp = readdir(dfd)) != NULL)
	{
		if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;
		if(response_code == 0){
			p += sprintf(p, "%s\r\n",dp->d_name);	
		}

	}
	closedir(dfd);
#endif 
}

#if 0
unsigned int parseTime_searchTimeFile(char *response, char *dir, char *time_dir, time_t start_time_sec, time_t end_time_sec, unsigned int len)
{
#ifndef __WIN32__
	struct dirent *dp;
	DIR *dfd;
	struct stat st_buf;
	char *p = response + len;
	struct tm tmp_time;
	time_t file_time;
	char year[5] = {0};
	char mon[3] = {0};
	char day[3] = {0};
	char hour[3] = {0};
	char min[3] = {0};
	char sec[3] = {0};
	char *tmp = NULL;
	char timeMS[10] = {0};
	char new_path[30] = "../../IPCamera/20100710";

	//printf("new_path == %s\n", dir);
	//printf("time_path == %s\n", time_dir);

	if(stat(dir,&st_buf) == -1){
		printf("error 1\n");
		return 0;
	}
	if(S_ISDIR(st_buf.st_mode) == 0){
		printf("error 2\n");   
		return 0;
	}
	if((dfd = opendir(dir)) == NULL){
		printf("error 3\n");
		return 0;
	}
	while((dp = readdir(dfd)) != NULL)
	{
		if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;
		else{
			//	printf("dp->d_name file = %s", dp->d_name);
			tmp = dp->d_name;
			tmp += 1;
			strncpy(timeMS, tmp, 6);
			//p += sprintf(p, "%s\r\n",dp->d_name);	
			parseTime_GetEndTime(time_dir, timeMS, year, mon, day, hour, min, sec);
			tmp_time.tm_year = atoi(year) - 1900;
			tmp_time.tm_mon = atoi(mon) - 1;
			tmp_time.tm_mday = atoi(day);
			tmp_time.tm_hour = atoi(hour); 
			tmp_time.tm_min = atoi(min); 
			tmp_time.tm_sec = atoi(sec); 
			file_time = mktime(&tmp_time);
			if(file_time >= start_time_sec && file_time <= end_time_sec){
				p += sprintf(p, "%s\r\n",dp->d_name);	
			}

		}

	}
	closedir(dfd);
	return strlen(response);
#else
	return 0;
#endif 	
}
#endif

#if 0
void parseTime_searchTimeDir(char *response, char *dir, time_t start_time_day,
		time_t end_time_day, time_t start_time_sec, time_t end_time_sec)
{
#ifndef __WIN32__
	struct dirent *dp;
	DIR *dfd;
	struct stat stbuf;
	char *p = response;
	struct tm tmp_time;
	time_t dir_time;
	char year[5] = {0};
	char mon[3] = {0};
	char day[3] = {0};
	char hour[3] = {0};
	char min[3] = {0};
	char sec[3] = {0};
	char new_path[30] = {0};
	unsigned int len = 0, s_len = 0;
	//	printf("dir_path = %s\n",dir); //start time sec

	if(stat(dir,&stbuf) == -1)
		return ;
	if(S_ISDIR(stbuf.st_mode) == 0)
		return ;
	if((dfd = opendir(dir)) == NULL)
		return ;
	while((dp = readdir(dfd)) != NULL)
	{
		if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;
		else{
			parseTime_GetEndTime(dp->d_name, NULL, year, mon, day, hour, min, sec);
			tmp_time.tm_year = atoi(year) - 1900;
			tmp_time.tm_mon = atoi(mon) - 1;
			tmp_time.tm_mday = atoi(day);
			tmp_time.tm_hour = 0; 
			tmp_time.tm_min = 0; 
			tmp_time.tm_sec = 0; 
			dir_time = mktime(&tmp_time);
			if(dir_time >= start_time_day && dir_time <= end_time_day){
				memset(new_path, 0x0, sizeof new_path);
				sprintf(new_path, "%s/%s", dir, dp->d_name);
				s_len = parseTime_searchTimeFile(response, new_path, dp->d_name, start_time_sec, end_time_sec, len);
				len += s_len;
			}

		}

	}
	closedir(dfd);
#endif 
}
#endif

#if 0
void parseTimeRange_getStartFileDir(char const *buf, char *time, char *time_range_start[])
{
	//char time[30] = {0};
	char const *p = NULL;
	Boolean is_delimiter = 1, k = 0;

	if(buf == NULL)
	{
		Debug(ckite_log_message,"buf is NULL");
		return ;
	}
	p = strstr(buf, "\r\n\r\n");
	p += 4;
	while (*p == ' ') ++p;
	strncpy(time, p, strlen(p));
	Debug(ckite_log_message,"time = %s",time);

	Boolean i = 0;
	while(time[i] != '\r' && time[i+1] != '\n')
	{
		if(is_delimiter)
		{
			if(time[i] != 'T' || time[i] != 'Z' || time[i] != 'P')
			{
				time_range_start[k++] = &time[i];
				is_delimiter = 0;
			}
		}
		else
		{
			if(time[i] == 'T' || time[i] == 'Z' || time[i] == 'P')
			{
				time[i] = '\0';
				is_delimiter = 1;
			}
		}
		++i;
	}
}
#endif


void parseTime_GetEndTime(char *time1, char *time2, char *year, char *month, char *day, char *hour, char *minute, char *second)
{
	int i = 0, j = 0, k = 0;

	//	printf("time1 address = %x\n, time2 address = %x\n",time1, time2);

	if (time1 == NULL)
		return ;
	while(time1[i] != '\0'){
		if(i >= 0 &&i <=3 ){
			year[i] = time1[i];
		}
		else if(i >= 4 && i <=5 ){
			month[j++] = time1[i];
		}
		else if(i >= 6 && i<= 7)
			day[k++] = time1[i];
		++i;
	}
	i = 0, j = 0, k = 0;
	if(time2 == NULL)
		return ;
	while(time2[i] != '\0'){
		if(i >= 0 && i <=1 ){
			hour[i] = time2[i];
		}
		else if(i >= 2 && i <= 3)
			minute[j++] = time2[i];
		else if(i >= 4 && i<= 5)
			second[k++] = time2[i];
		++i;
	}
}

void parseTransportHeader(char const* buf,
		StreamingMode& streamingMode,
		char*& streamingModeString,
		char*& destinationAddressStr,
		unsigned char& destinationTTL,
		unsigned short& clientRTPPortNum, // if UDP
		unsigned short& clientRTCPPortNum, // if UDP
		unsigned char& rtpChannelId, // if TCP
		unsigned char& rtcpChannelId // if TCP
		) {
	// Initialize the result parameters to default values:
	streamingMode = RTP_UDP;
	streamingModeString = NULL;
	destinationAddressStr = NULL;
	destinationTTL = 255;
	clientRTPPortNum = 0;
	clientRTCPPortNum = 1;
	rtpChannelId = rtcpChannelId = 0xFF;

	unsigned short p1, p2;
	unsigned ttl, rtpCid, rtcpCid;

	// First, find "Transport:"
	while (1) {
		if (*buf == '\0') return; // not found
		if (_strncasecmp(buf, "Transport: ", 11) == 0) break;
		++buf;
	}

	// Then, run through each of the fields, looking for ones we handle:
	char const* fields = buf + 11;
	char* field = strDupSize(fields);
	while (sscanf(fields, "%[^;]", field) == 1) {
		if (strcmp(field, "RTP/AVP/TCP") == 0) {
			streamingMode = RTP_TCP;
		} else if (strcmp(field, "RAW/RAW/UDP") == 0 ||
				strcmp(field, "MP2T/H2221/UDP") == 0) {
			streamingMode = RAW_UDP;
			streamingModeString = strDup(field);
		} else if (_strncasecmp(field, "destination=", 12) == 0) {
			delete[] destinationAddressStr;
			destinationAddressStr = strDup(field+12);
		} else if (sscanf(field, "ttl%u", &ttl) == 1) {
			destinationTTL = (unsigned char)ttl;
		} else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) {
			clientRTPPortNum = p1;
			clientRTCPPortNum = p2;
		} else if (sscanf(field, "client_port=%hu", &p1) == 1) {
			clientRTPPortNum = p1;
			clientRTCPPortNum = streamingMode == RAW_UDP ? 0 : p1 + 1;
		} else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
			rtpChannelId = (unsigned char)rtpCid;
			rtcpChannelId = (unsigned char)rtcpCid;
		}

		fields += strlen(field);
		while (*fields == ';') ++fields; // skip over separating ';' chars
		if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
	}
	delete[] field;
}

Boolean parsePlayNowHeader(char const* buf) {
	// Find "x-playNow:" header, if present
	while (1) {
		if (*buf == '\0') return False; // not found
		if (_strncasecmp(buf, "x-playNow:", 10) == 0) break;
		++buf;
	}

	return True;
}
void parseTokenFromURLSuffix(char *suffixWithToken, char *suffix, char *token)
{
	int i = 0; 
	while( i < strlen(suffixWithToken))
	{   
		if (suffixWithToken[i] == '?')
		{   
			memcpy(token, &suffixWithToken[i+1], sizeof token);
			suffixWithToken[i] = '\0';
			break;
		}
		suffix[i] = suffixWithToken[i];
		i++;
	}

}

#if 0
int HexToDec(char *Str)
{
	int result=0;
	int i=0,szLen=0,nHex;

	szLen = strlen(Str);
	nHex = '0';
	for (; i < szLen; i++)
	{
		if ((Str[i] >= '0') && (Str[i] <= '9'))
			nHex = '0';
		else if ((Str[i] >= 'A') && (Str[i] <= 'F'))
			nHex = 'A' - 10;
		else if ((Str[i] >= 'a') && (Str[i] <= 'f'))
			nHex = 'a' - 10;
		else
			return 0;
		result = result * 16 + Str[i] - nHex;
	}
	return result;
} 
#endif
