#include "RTSPServer.h"
#include "RTSPCommon.h"
#include "GroupsockHelper.h"
#include "rsa_crypto.h"
#include "RTSPSdk.h"

#include "RTCP.h"
#include "EncoderMediaSubsession.h"
#include <sys/types.h>
#include<netinet/in.h>
#include<netinet/tcp.h>

#ifdef __WIN32__
#include <iostream.h>
#else
#include <iostream>
#endif
#if defined(__WIN32__) || defined(_WIN32) || defined(_QNX4)
#else
#include <signal.h>
#define USE_SIGNALS 1
#endif
#include <time.h> // for "strftime()" and "gmtime()"
#include "Debug.h"

extern UsageEnvironment *env;
#define STARV_SDK_VERSION  "1.1"

char * rtsp_parse_param_line_within_range(char* msg, int length, char* param_name)
{
	char * line_start, * line_end, * param1, * param2, * param;

	line_start = strstr(msg, param_name);
	if (line_start == NULL)
		return NULL;
	if (line_start-msg > length)
		return NULL;

	line_end = strstr(line_start, "\r\n");
	if (line_end == NULL)
		return NULL;
	if (line_end-msg > length)
		return NULL;

	param1 = strstr(line_start, "=");
	param2 = strstr(line_start, ":");
	if ((param1 == NULL || param1 >= line_end) && (param2 == NULL && param2 >= line_end))
		return NULL;

	if (param1 == NULL)
		param = param2;
	else if (param2 == NULL)
		param = param1;
	else
		param = param1 < param2 ? param1 : param2;

	param += 1;
	while (*param == ' ' || *param == '\t')
		param ++;

	if (*param == '\r' || *param == '\n')
		return NULL;

	return param;
}

int rtsp_get_msg(char * buf, int buf_len, char *msg)
{
	int m, n;
	char * p;

	// Find the end of header
	p = strstr(buf, "\r\n\r\n");
	if (p == NULL)
		return 0;
	m = p - buf;

	// Find "Content-Length" header line
	p = rtsp_parse_param_line_within_range(buf, m, (char *)"Content-Length");
	if (p == NULL)	// No content body
	{
		p = rtsp_parse_param_line_within_range(buf, m, (char *)"Content-length");
	}
	if (p == NULL)
	{
		n = 0;
	}
	else
	{
		n = atoi(p);
	}

	if (buf_len >= m+n+4)
	{
		memset(msg, 0, m+n+5);
		memcpy(msg, buf, m+n+4);
		return m+n+4;
	}
	else
		return 0;
}


////////// RTSPServer::RTSPEncoderSession implementation //////////

	RTSPServer::RTSPEncoderSession
::RTSPEncoderSession(RTSPServer& ourServer, ServerMediaSession* session, unsigned sessionId, struct sockaddr_in addr,
					 char* local_ip, unsigned short local_port, char* path, char* macaddr, char* sn, char* ua,
					 unsigned int cam_id)
: RTSPClientSession(ourServer, sessionId, -1, addr),
	fCamId(cam_id),
	fCSeq(1),
	fSetupNum(0),
	fSession(session),
	videoActive(False),
	videofp(NULL){	
		// Arrange to handle incoming requests:
		memset(fLocalIP, 0, sizeof(fLocalIP));
		memset(fPath, 0, sizeof(fPath));
		memset(fMac, 0, sizeof(fMac));
		memset(fSerial, 0, sizeof(fSerial));
		memset(fUserAgent, 0, sizeof(fUserAgent));
		if (local_ip)
			::strcpy(fLocalIP, local_ip);
		fLocalPort = local_port;
		if (path)
			::strcpy(fPath, path);
		if (macaddr)
			::strcpy(fMac, macaddr);
		if (sn)
			::strcpy(fSerial, sn);
		if (ua)
			::strcpy(fUserAgent, ua);
		memset(videoFile, 0x0, sizeof videoFile);
		if (strcmp(fPath, "store") == 0)
		{
			videoActive = True;
		}
		resetRequestBuffer();
		if (register_Platform(&addr))
		{
			envir().taskScheduler().turnOnBackgroundReadHandling(fClientSocket,
																 (TaskScheduler::BackgroundHandlerProc*)&incomingRequestHandler, this);
			envir().taskScheduler().scheduleDelayedTask(HEARTBEAT_PERIOD, (TaskFunc*)heartbeat, this);
		}
		noteLiveness();
}

RTSPServer::RTSPEncoderSession::~RTSPEncoderSession() {
}

void RTSPServer::RTSPEncoderSession::noteLiveness() {
	if (fOurServer.fReclamationTestSeconds > 0 /*&&fLivenessCheckTask > 0 */&& fClientSocket >= 0) { /*wayde add fLivenessCheckTask > 0*/
		envir().taskScheduler()
			.rescheduleDelayedTask(fLivenessCheckTask,
								   fOurServer.fReclamationTestSeconds*1000000,
								   (TaskFunc*)livenessTimeoutTask, this);
	}
}

void RTSPServer::RTSPEncoderSession
::livenessTimeoutTask(RTSPEncoderSession* encoder) {
	///////////////////////////////////////////////////////////
	::closeSocket(encoder->fClientSocket);
	encoder->fClientSocket = -1;
}

void RTSPServer::RTSPEncoderSession::handleRequestBytes(int newBytesRead) {
	noteLiveness();


	if (newBytesRead <= 0 || (unsigned)newBytesRead >= fRequestBufferBytesLeft) {
		//delete this;
		if (fClientSocket >= 0)
		{
	      envir().taskScheduler().turnOffBackgroundReadHandling(fClientSocket);
		  ::closesocket(fClientSocket);
		  fClientSocket = -1;
		}
		return;
	}

	Boolean endOfMsg = False;
	unsigned int msg_len;
	unsigned char* ptr = &fRequestBuffer[fRequestBytesAlreadySeen];
#ifdef DEBUG
	ptr[newBytesRead] = '\0';
	Debug(ckite_log_message, "RTSPEncoderSession[%p]::handleRequestBytes() read %d new bytes:%s\n", this, newBytesRead, ptr);
#endif

	// Look for the end of the message: <CR><LF><CR><LF>
	unsigned char *tmpPtr = ptr;
	if (fRequestBytesAlreadySeen > 0) --tmpPtr;
	// in case the last read ended with a <CR>
	while (tmpPtr < &ptr[newBytesRead-1]) {
		if (*tmpPtr == '\r' && *(tmpPtr+1) == '\n') {
			if (tmpPtr - fLastCRLF == 2) { // This is it: 
				endOfMsg = True; //find \r\n\r\n
				break;
			}
			fLastCRLF = tmpPtr;
		}
		++tmpPtr;
	}
	
	fRequestBufferBytesLeft -= newBytesRead;
	fRequestBytesAlreadySeen += newBytesRead;

	if (!endOfMsg) return; // subsequent reads will be needed to complete the request

	while (1)
	{
		msg_len = rtsp_get_msg((char*)fRequestBuffer, fRequestBytesAlreadySeen, (char*)fRequestMsg);
		if (msg_len == 0)
		{
			break;
			//return;
		}
		if (msg_len < fRequestBytesAlreadySeen)
			memmove(fRequestBuffer, fRequestBuffer + msg_len, fRequestBytesAlreadySeen - msg_len);
		fRequestBytesAlreadySeen -= msg_len;

		// Parse the request string into command name and 'CSeq',
		// then handle the command:
		fRequestMsg[msg_len] = '\0';
		char cmdName[RTSP_PARAM_STRING_MAX];
		char urlPreSuffix[RTSP_PARAM_STRING_MAX];
		char urlSuffix[RTSP_PARAM_STRING_MAX];
		char cseq[RTSP_PARAM_STRING_MAX];
		char resultCode[RTSP_PARAM_STRING_MAX] = {0};
		char resultStatus[RTSP_PARAM_STRING_MAX] = {0};
		char contentType[RTSP_PARAM_STRING_MAX] = {0};
		char challenge[RTSP_PARAM_STRING_MAX] = {0};
		char challenge_resp[RTSP_PARAM_STRING_MAX] = {0};
		unsigned int contentLength = 0;
		unsigned char* content = NULL;
		if (strncmp((const char*)fRequestMsg, "RTSP/1.0", 8) == 0)
		{
			// this is an RTSP response
			if (!parseRTSPResponseString((char*)fRequestMsg, msg_len,
										 resultCode, sizeof(resultCode),
										 resultStatus, sizeof(resultStatus),
										 contentType, sizeof(contentType),
										 challenge, sizeof(challenge),
										 &contentLength, &content))
			{
				handleCmd_bad("1");
			}
			else
			{
				if (strcmp(resultCode, "200") == 0)
				{
					if (strcmp(contentType, "application/serial") == 0)
					{
						// Write the serial into flash
						if (contentLength > 0 && content)
						{
							memcpy(fSerial, content, contentLength);
							writeSerial(fSerial);
							//free(content);
						}

						register_Platform(&fClientAddr, false);
						send(fClientSocket, (char const*)fResponseBuffer, strlen((char*)fResponseBuffer), 0);
					}
					if (strlen(challenge) > 0)	// Receive challenge from streaming server
					{
						// encrypt the challenge using public key, and send back to server
						char challenge_resp[257] = {0};
						char out[256] = {0};
						char pub_key[257] = {0};
						static int i = 0;
						readkey(fCamId, pub_key);
						//fprintf(stderr, "pub_key = %s\n", pub_key);
						unsigned char keybuf[512] = {"9FDAB571D593813BD81AC9207E3CD985B2074028CCBDFCF3A381865"
							    "042BAF865794653C6C9173CC36800D2473B80A0A8D63FD73464318D"
								"9E1332EC30D3E106395252BB39C8BBB1440AE642100EAAFFDEE8609"
								"E339CA60AB5774CDC3271EF51174693E556B0B9E17BB9977B8C8021"
								"1EBF22A900B9FFBBFE3E99617E4C8FD51271"};

#if 0
						void* key = init_crypto_key((unsigned char*)pub_key, strlen(pub_key), (unsigned char*)keybuf, 256);
						int ret_len = crypto_encode(key, (unsigned char*)challenge, strlen(challenge),
									  (unsigned char*)challenge_resp, sizeof(challenge_resp));
						Debug(ckite_log_message, "challenge_resq = %s, len = %d\n", challenge_resp, ret_len);
						int decode_len = crypto_decode(key, (unsigned char*)challenge_resp, strlen(challenge_resp),
									  (unsigned char*)out, sizeof(out));
						Debug(ckite_log_message, "out = %s, len = %d\n", out, decode_len);
						do_heartbeat(true, true, challenge_resp);
#else 
						void* key = init_crypto_key((unsigned char*)pub_key, strlen(pub_key), (unsigned char*)"", 0);
						int ret_len = crypto_encode(key, (unsigned char*)challenge, strlen(challenge),
									  (unsigned char*)challenge_resp, sizeof(challenge_resp));
						do_heartbeat(true, true, challenge_resp);
						RSA_free((RSA *)key);
#endif
					}			  
				}
				else if (strcmp(resultCode, "303") == 0 && _strncasecmp(resultStatus,"See Other", 9) == 0){
					handleCmd_SeeOtherServer((char const*)fRequestMsg);
				}
				else if (strcmp(resultCode, "401") == 0 && _strncasecmp(resultStatus,"Unauthorized", 12) == 0){
					handleCmd_Unauthorized();
				}
				else if (strcmp(resultCode, "400") == 0 && _strncasecmp(resultStatus, "Bad Request", 11) == 0){
					handleCmd_Badrequest();
				}
			}
			if (contentLength > 0 && content)
			{
			    free(content);
			}
		}
		else
		{
			if (!parseRTSPRequestString((char*)fRequestMsg, msg_len,
										cmdName, sizeof cmdName,
										urlPreSuffix, sizeof urlPreSuffix,
										urlSuffix, sizeof urlSuffix,
										cseq, sizeof cseq)) {
#ifdef DEBUG
				fprintf(stderr, "parseRTSPRequestString() failed!\n");
#endif
				handleCmd_bad(cseq);
			} else {
#ifdef DEBUG
				fprintf(stderr, "parseRTSPRequestString() returned cmdName \"%s\", urlPreSuffix \"%s\", urlSuffix \"%s\"\n", cmdName, urlPreSuffix, urlSuffix);
#endif
				if (strcmp(cmdName, "OPTIONS") == 0) {
					handleCmd_OPTIONS(cseq);
				} else if (strcmp(cmdName, "DESCRIBE") == 0) {
					handleCmd_DESCRIBE(cseq, urlSuffix, (char const*)fRequestMsg);
				} else if (strcmp(cmdName, "SETUP") == 0) {
					handleCmd_SETUP(cseq, urlPreSuffix, urlSuffix, (char const*)fRequestMsg);
				} else if (strcmp(cmdName, "TEARDOWN") == 0
						   || strcmp(cmdName, "PLAY") == 0
						   || strcmp(cmdName, "PAUSE") == 0
						   || strcmp(cmdName, "GET_PARAMETER") == 0
						   || strcmp(cmdName, "SET_PARAMETER") == 0) {
					handleCmd_withinSession(cmdName, urlPreSuffix, urlSuffix, cseq,
											(char const*)fRequestMsg);
				} else {
					handleCmd_notSupported(cseq);
				}
			}
			if( strcmp(cmdName, "PLAY") != 0)
			{
				send(fClientSocket, (char const*)fResponseBuffer, strlen((char*)fResponseBuffer), 0);
			}
		}
	}

	resetRequestBuffer(); // to prepare for any subsequent request
	if (!fSessionIsActive)
	{
	  // TODO: 
	  //delete this;
	  envir().taskScheduler().turnOffBackgroundReadHandling(fClientSocket);
	  ::closesocket(fClientSocket);
	  fClientSocket = -1;
	}
}

void RTSPServer::RTSPEncoderSession
::handleCmd_Badrequest(void){

#ifndef __WIN32__
	sleep(30);
#else
	Sleep(30*1000);
#endif
	memset(fSerial, 0x0, sizeof(fSerial));
	register_Platform(&fClientAddr, True);
	reNoteLiveness();
}

void RTSPServer::RTSPEncoderSession
::handleCmd_Unauthorized(void){
	char key[256] = {0};
	char deviceIp[60] = {0};
	unsigned short devicePort = 0;
	char deviceMac[20] = {0};
	char *keyPrefix = NULL;
	char *recvBuffer = new char[1024];
	int serverSocket;

	RTSPServer * resultServer;
	lookupByName(*env, fPath, resultServer);
	delete resultServer;

	fOurServer.getServerSocket(serverSocket);
	envir().taskScheduler().turnOffBackgroundReadHandling(fClientSocket);
	::closeSocket(fClientSocket);
	fClientSocket = -1;

	envir().taskScheduler().turnOffBackgroundReadHandling(serverSocket);
	::closeSocket(serverSocket);

	if(fSession != NULL) {
		fSession->decrementReferenceCount();
		if(fSession->referenceCount() == 0
				&& fSession->deleteWhenUnreferenced()) {
			fOurServer.removeServerMediaSession(fSession);
		}
	}
	//delete this;
	writekey(fCamId, key); // set zero of key
	getDeviceInfo(fCamId,deviceIp, &devicePort, deviceMac);
	while(1)
	{
		httpRequestKey(recvBuffer, deviceMac);
		keyPrefix = strstr(recvBuffer, "Key:");
		if(keyPrefix != NULL)
		{
			memcpy(key, keyPrefix+4, 256);
			writekey(fCamId, key);
			break;
		}
		else 
		{
			setSessionState(fPath, KEYFAILED);
#ifndef __WIN32__
			sleep(30*60);
#else
			Sleep(30*60*1000);// request again after half hour
#endif
		}
	}
	delete [] recvBuffer;
	realEntryMain(fCamId);
}

void domainToIpAddress(char *domain, char *host)
{
	char		**pptr;
	struct hostent	*hptr;
	if ((hptr = gethostbyname(domain)) == NULL)
	{
	    printf("gethostbyname error for host: %s\n", host);
	    return;
	}
	
	switch (hptr->h_addrtype)
	{
	case AF_INET:
	case AF_INET6:
	    strcpy(host, inet_ntoa(*(struct in_addr *)*(hptr->h_addr_list)));
	    break;
	
	default:
	    printf("unknown address type.\n");
	    break;
	}
}

void RTSPServer::RTSPEncoderSession
::handleCmd_SeeOtherServer(char const* fullRequestStr){
	char location_other[30] = {0};
	char *host_name = location_other;
	char *port = location_other;
	char seeOtherIp[32] = {0}; //on 2011-12-17

	fprintf(stderr, "see other server.\n");
	if(!parseSpecifiedStringParam(fullRequestStr,location_other,(char *)"Location:",9))
	{
#ifdef DEBUG
		Debug(ckite_log_message, "handleCmd_SeeOtherServer para is error");
#endif
		return ;
	}

	if (port = strstr(location_other, "\r\n"))
		*port = '\0';
	port = location_other;
	while(*port != '\0')
	{
		if(*port == ':')
		{
			*port = '\0';
			port += 1;
			break;			
		}
		port++;
	}
	struct sockaddr_in sa;
	domainToIpAddress(host_name, seeOtherIp); 
	sa.sin_family = AF_INET;

	sa.sin_addr.s_addr = inet_addr(seeOtherIp);


	sa.sin_port = htons(atoi(port));


	writeActiveServerInfo(seeOtherIp,(unsigned short)atoi(port));


	// turnoffBackgroundReadHandling
	// closeSocket

	envir().taskScheduler().turnOffBackgroundReadHandling(fClientSocket);
	::closeSocket(fClientSocket);
	fClientSocket = -1;

	if (register_Platform(&sa,True))
	{
		envir().taskScheduler().turnOnBackgroundReadHandling(fClientSocket,
															 (TaskScheduler::BackgroundHandlerProc*)&incomingRequestHandler, this);
		envir().taskScheduler().scheduleDelayedTask(HEARTBEAT_PERIOD, (TaskFunc*)heartbeat, this);
	}
	noteLiveness();
}


void RTSPServer::RTSPEncoderSession
::handleCmd_DESCRIBE(char const* cseq, char const* urlSuffix,
					 char const* fullRequestStr) {
	char* sdpDescription = NULL;
	char* rtspURL = NULL;
	char urlBaseSuffix[60];

	if( strcmp(fPath, "store") == 0 )
	{
		int i = 0;
		const char *p = strstr((char const *)fullRequestStr, (const char *)fSerial);

		if (p == NULL) return ;
		p += strlen((const char *)fSerial) + 1;
		memset(urlBaseSuffix, 0x0, sizeof urlBaseSuffix);
		while(p[i] != ' ' &&  p[i] != '\t') //get necessary play file
		{
			if( i < 60)
			{
				urlBaseSuffix[i] = p[i];
			}
			i++;
		}
	}
	if (strlen(videoFile) > 10)
	{
		ServerMediaSubsession *subsession = NULL;
		for (unsigned i = 0; i < fNumStreamStates; ++i) {
			if (subsession == NULL /* means: aggregated operation */
				|| subsession == fStreamStates[i].subsession) {
				fStreamStates[i].subsession->deleteStream(fOurSessionId,
														 fStreamStates[i].streamToken);
			}
		}

//		for ( unsigned i = 0; i < fNumStreamStates; ++i) {
//			Debug(ckite_log_message, "DESCRIBE fStreamStates[%d].subsession = %x\n", i, fStreamStates[i].subsession);
//			if (subsession == NULL /* means: aggregated operation */
//				|| subsession == fStreamStates[i].subsession) {
			//	if ( i == 0)
			//		delete fStreamStates[i].subsession;
//				}
//		}
		//reclaimStreamStates(); // delete stream 
		fSetupNum = 0; //setup set
		fSession->resetMediaServersession();
	}
	if( strcmp(fPath, "store") == 0 )
	{
		int width = 0;	
		int height = 0;
		unsigned int video_bitrate = 0;
		unsigned int framerate = 0;
		unsigned int keyinterval = 0;
		unsigned int audio_bitrate = 0;
		unsigned int framelenght = 0;
		unsigned int samplerate = 0;

		fprintf(stderr, "urlBaseSuffix: %s\n", urlBaseSuffix);
		getStoreFileResolution(urlBaseSuffix, width, height);
		getSubsessionParaConfig(fPath, width, height, video_bitrate, framerate, keyinterval, 
													  audio_bitrate, framelenght, samplerate);

		fprintf(stderr, "fPath:%s, width:%d, height:%d, framerate:%d\n", fPath, width, height, framerate);
		EncoderMediaSubsession *audioSubsession, *videoSubsession;	
		videoSubsession = EncoderMediaSubsession::createNew(*env, NULL, False, False, 6970);
		videoSubsession->SetVideoParameters(fCamId, width, height, video_bitrate, framerate, keyinterval, fPath);
		fSession->addSubsession(videoSubsession);
#if 1
		audioSubsession = EncoderMediaSubsession::createNew(*env, NULL, True, False, 6970);
		audioSubsession->SetAudioParameters(audio_bitrate, framelenght, samplerate, fPath);
		fSession->addSubsession(audioSubsession);
#endif
	}
	do {
		// We should really check that the request contains an "Accept:" #####
		// for "application/sdp", because that's what we're sending back #####


		// Then, assemble a SDP description for this session:
		sdpDescription = fSession->generateSDPDescription();
		if (sdpDescription == NULL) {
			// This usually means that a file name that was specified for a
			// "ServerMediaSubsession" does not exist.
			snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
					 "RTSP/1.0 404 File Not Found, Or In Incorrect Format\r\n"
					 "CSeq: %s\r\n"
					 "%s\r\n",
					 cseq,
					 dateHeader());
			break;
		}
		unsigned sdpDescriptionSize = strlen(sdpDescription);

		// Also, generate our RTSP URL, for the "Content-Base:" header
		// (which is necessary to ensure that the correct URL gets used in
		// subsequent "SETUP" requests).
		rtspURL = fOurServer.rtspURL(fSession, fClientSocket);
		//Debug(ckite_log_message, "DESCRIBE rtspURL = %s, fSession = %x, fClientSocket = %d\n", rtspURL, fSession, fClientSocket);
		

		if (strcmp(fPath, "store") == 0)
		{
			snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
				 "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
				 "%s"
				 "Content-Base: %s/%s/\r\n"
				 "Content-Type: application/sdp\r\n"
				 "Content-Length: %d\r\n\r\n"
				 "%s",
				 cseq,
				 dateHeader(),
				 rtspURL,
				 urlBaseSuffix,
				 sdpDescriptionSize,
				 sdpDescription);
		}
		else
		{
			snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
				 "RTSP/1.0 200 OK\r\nCSeq: %s\r\n"
				 "%s"
				 "Content-Base: %s/\r\n"
				 "Content-Type: application/sdp\r\n"
				 "Content-Length: %d\r\n\r\n"
				 "%s",
				 cseq,
				 dateHeader(),
				 rtspURL,
				 sdpDescriptionSize,
				 sdpDescription);

		}
	} while (0);

	delete[] sdpDescription;
	delete[] rtspURL;
}

void RTSPServer::RTSPEncoderSession
::handleCmd_SETUP(char const* cseq,
				  char const* urlPreSuffix, char const* urlSuffix,
				  char const* fullRequestStr) {
	// "urlPreSuffix" should be the session (stream) name, and
	// "urlSuffix" should be the subsession (track) name.
	char const* streamName = urlPreSuffix;
	char const* trackId = urlSuffix;
	ServerMediaSubsession* subsession = NULL;

	setSessionState(fPath, SETUP);
	if (fSetupNum == 0)
	{
		reclaimStreamStates();
		ServerMediaSubsessionIterator iter(*fSession);
		for (fNumStreamStates = 0; iter.next() != NULL; ++fNumStreamStates) {}
		fStreamStates = new struct streamState[fNumStreamStates];
		iter.reset();
		for (unsigned i = 0; i < fNumStreamStates; ++i) {
			subsession = iter.next();
			fStreamStates[i].subsession = subsession;
			if (strcmp(fPath, "store") == 0)
			{
			}
			fStreamStates[i].streamToken = NULL; // for now; reset by SETUP later
		}
	}
	// Look up information for the specified subsession (track):
	unsigned streamNum;
	if (trackId != NULL && trackId[0] != '\0') { // normal case
		for (streamNum = 0; streamNum < fNumStreamStates; ++streamNum) {
			subsession = fStreamStates[streamNum].subsession;
			if (subsession != NULL && strcmp(trackId, subsession->trackId()) == 0) break;
		}
		if (streamNum >= fNumStreamStates) {
			// The specified track id doesn't exist, so this request fails:
			handleCmd_notFound(cseq);
			return;
		}
	} else {
		// Weird case: there was no track id in the URL.
		// This works only if we have only one subsession:
		if (fNumStreamStates != 1) {
			handleCmd_bad(cseq);
			return;
		}
		streamNum = 0;
		subsession = fStreamStates[streamNum].subsession;
	}
	// ASSERT: subsession != NULL

	// Look for a "Transport:" header in the request string,
	// to extract client parameters:
	StreamingMode streamingMode;
	char* streamingModeString = NULL; // set when RAW_UDP streaming is specified
	char* clientsDestinationAddressStr;
	u_int8_t clientsDestinationTTL;
	portNumBits clientRTPPortNum, clientRTCPPortNum;
	unsigned char rtpChannelId, rtcpChannelId;
	parseTransportHeader(fullRequestStr, streamingMode, streamingModeString,
						 clientsDestinationAddressStr, clientsDestinationTTL,
						 clientRTPPortNum, clientRTCPPortNum,
						 rtpChannelId, rtcpChannelId);
	if (streamingMode == RTP_TCP && rtpChannelId == 0xFF) {
		// TCP streaming was requested, but with no "interleaving=" fields.
		// (QuickTime Player sometimes does this.)  Set the RTP and RTCP channel ids to
		// proper values:
		rtpChannelId = fTCPStreamIdCount; rtcpChannelId = fTCPStreamIdCount+1;
	}
	fTCPStreamIdCount += 2;

	Port clientRTPPort(clientRTPPortNum);
	Port clientRTCPPort(clientRTCPPortNum);

	// Next, check whether a "Range:" header is present in the request.
	// This isn't legal, but some clients do this to combine "SETUP" and "PLAY":
	double rangeStart = 0.0, rangeEnd = 0.0;
	fStreamAfterSETUP = parseRangeHeader(fullRequestStr, rangeStart, rangeEnd) ||
		parsePlayNowHeader(fullRequestStr);

	// Then, get server parameters from the 'subsession':
	int tcpSocketNum = streamingMode == RTP_TCP ? fClientSocket : -1;
	netAddressBits destinationAddress = 0;
	u_int8_t destinationTTL = 255;
#ifdef RTSP_ALLOW_CLIENT_DESTINATION_SETTING
	if (clientsDestinationAddressStr != NULL) {
		// Use the client-provided "destination" address.
		// Note: This potentially allows the server to be used in denial-of-service
		// attacks, so don't enable this code unless you're sure that clients are
		// trusted.
		destinationAddress = our_inet_addr(clientsDestinationAddressStr);
	}
	// Also use the client-provided TTL.
	destinationTTL = clientsDestinationTTL;
#endif
	delete[] clientsDestinationAddressStr;
	Port serverRTPPort(0);
	Port serverRTCPPort(0);
	subsession->getStreamParameters(fOurSessionId, fClientAddr.sin_addr.s_addr,
									clientRTPPort, clientRTCPPort,
									tcpSocketNum, rtpChannelId, rtcpChannelId,
									destinationAddress, destinationTTL, fIsMulticast,
									serverRTPPort, serverRTCPPort,
									fStreamStates[streamNum].streamToken);
	struct in_addr destinationAddr; destinationAddr.s_addr = destinationAddress;
	char* destAddrStr = strDup(our_inet_ntoa(destinationAddr));
	struct sockaddr_in sourceAddr; 
#ifdef __WIN32__
	SOCKLEN_T namelen = sizeof sourceAddr;
#else
	socklen_t namelen = sizeof sourceAddr;
#endif

	getsockname(fClientSocket, (struct sockaddr*)&sourceAddr, &namelen);
	char* sourceAddrStr = strDup(our_inet_ntoa(sourceAddr.sin_addr));
	if (fIsMulticast) {
		switch (streamingMode) {
		case RTP_UDP:
			snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
					 "RTSP/1.0 200 OK\r\n"
					 "CSeq: %s\r\n"
					 "%s"
					 "Transport: RTP/AVP;multicast;destination=%s;source=%s;port=%d-%d;ttl=%d\r\n"
					 "Session: %08X\r\n\r\n",
					 cseq,
					 dateHeader(),
					 destAddrStr, sourceAddrStr, ntohs(serverRTPPort.num()), ntohs(serverRTCPPort.num()), destinationTTL,
					 fOurSessionId);
			break;
		case RTP_TCP:
			// multicast streams can't be sent via TCP
			handleCmd_unsupportedTransport(cseq);
			break;
		case RAW_UDP:
			snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
					 "RTSP/1.0 200 OK\r\n"
					 "CSeq: %s\r\n"
					 "%s"
					 "Transport: %s;multicast;destination=%s;source=%s;port=%d;ttl=%d\r\n"
					 "Session: %08X\r\n\r\n",
					 cseq,
					 dateHeader(),
					 streamingModeString, destAddrStr, sourceAddrStr, ntohs(serverRTPPort.num()), destinationTTL,
					 fOurSessionId);
			break;
		}
	} else {
		switch (streamingMode) {
		case RTP_UDP: {
						  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
								   "RTSP/1.0 200 OK\r\n"
								   "CSeq: %s\r\n"
								   "%s"
								   "Transport: RTP/AVP;unicast;destination=%s;source=%s;client_port=%d-%d;server_port=%d-%d\r\n"
								   "Session: %08X\r\n\r\n",
								   cseq,
								   dateHeader(),
								   destAddrStr, sourceAddrStr, ntohs(clientRTPPort.num()), ntohs(clientRTCPPort.num()), ntohs(serverRTPPort.num()), ntohs(serverRTCPPort.num()),
								   fOurSessionId);
						  break;
					  }
		case RTP_TCP: {
						  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
								   "RTSP/1.0 200 OK\r\n"
								   "CSeq: %s\r\n"
								   "%s"
								   "Transport: RTP/AVP/TCP;unicast;destination=%s;source=%s;interleaved=%d-%d\r\n"
								   "Session: %08X\r\n\r\n",
								   cseq,
								   dateHeader(),
								   destAddrStr, sourceAddrStr, rtpChannelId, rtcpChannelId,
								   fOurSessionId);
						  break;
					  }
		case RAW_UDP: {
						  snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
								   "RTSP/1.0 200 OK\r\n"
								   "CSeq: %s\r\n"
								   "%s"
								   "Transport: %s;unicast;destination=%s;source=%s;client_port=%d;server_port=%d\r\n"
								   "Session: %08X\r\n\r\n",
								   cseq,
								   dateHeader(),
								   streamingModeString, destAddrStr, sourceAddrStr, ntohs(clientRTPPort.num()), ntohs(serverRTPPort.num()),
								   fOurSessionId);
						  break;
					  }
		}
	}
	fSetupNum ++;
	delete[] destAddrStr; delete[] sourceAddrStr; delete[] streamingModeString;
}

void RTSPServer::RTSPEncoderSession
::handleCmd_withinSession(char const* cmdName,
						  char const* urlPreSuffix, char const* urlSuffix,
						  char const* cseq, char const* fullRequestStr) {
	// This will either be:
	// - a non-aggregated operation, if "urlPreSuffix" is the session (stream)
	//   name and "urlSuffix" is the subsession (track) name, or
	// - a aggregated operation, if "urlSuffix" is the session (stream) name,
	//   or "urlPreSuffix" is the session (stream) name, and "urlSuffix"
	//   is empty.
	// First, figure out which of these it is:
	if (fSession == NULL) { // There wasn't a previous SETUP!
		handleCmd_notSupported(cseq);
		return;
	}
	ServerMediaSubsession* subsession = NULL;

	if (strcmp(cmdName, "TEARDOWN") == 0) {
		handleCmd_TEARDOWN(subsession, cseq);
	} else if (strcmp(cmdName, "PLAY") == 0) {
		handleCmd_PLAY(subsession, cseq, fullRequestStr);
	} else if (strcmp(cmdName, "PAUSE") == 0) {
		handleCmd_PAUSE(subsession, cseq);
	} else if (strcmp(cmdName, "GET_PARAMETER") == 0) {
		handleCmd_GET_PARAMETER(subsession, cseq, fullRequestStr);
	} else if (strcmp(cmdName, "SET_PARAMETER") == 0) {
		handleCmd_SET_PARAMETER(subsession, cseq, fullRequestStr);
	}
}

void RTSPServer::RTSPEncoderSession
::handleCmd_TEARDOWN(ServerMediaSubsession* subsession, char const* cseq) {
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			 "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s\r\n",
			 cseq, dateHeader());
	for (unsigned i = 0; i < fNumStreamStates; ++i) {
		if (subsession == NULL /* means: aggregated operation */
			|| subsession == fStreamStates[i].subsession) {
			fStreamStates[i].subsession->deleteStream(fOurSessionId,
													 fStreamStates[i].streamToken);
		}
	}
	fSessionIsActive = False; // triggers deletion of ourself after responding
}

void RTSPServer::RTSPEncoderSession
::handleCmd_PLAY(ServerMediaSubsession* subsession, char const* cseq,
				 char const* fullRequestStr) {
	char* rtspURL = fOurServer.rtspURL(fSession, fClientSocket);
	unsigned rtspURLSize = strlen(rtspURL);
	char magicType[30] = {0};
	char playRange[60] = {0};
	char sdp[1024] = {0};
	int sdpLen = 0; 

	// Parse the client's "Scale:" header, if any:
	float scale;

	fMagic = 0xffffffff;
	setSessionState(fPath, PLAY);
	// get play file name

	if( strcmp(fPath, "store") == 0 )
	{
		int i = 0;
		if(parseSpecifiedStringParam(fullRequestStr,magicType,(char *)"magic:",6))
		{
			if(parseSpecifiedStringParam(fullRequestStr,playRange,(char *)"clock=",6))
			{
				while ( i < strlen(playRange))
				{
					if (playRange[i] == 'Z' || playRange[i] == 'z')
					{
						atoi(&playRange[i+1]);
						break;
					}
					else i++;
				}
			}
			fMagic = atoi(magicType);
		}
		const char *p = strstr((char const *)fullRequestStr, (const char *)fSerial);
		if (p == NULL)
			return ;
		p += strlen((const char *)fSerial) + 1;
		memset(videoFile, 0x0, sizeof videoFile);
		i = 0;
		while(p[i] != ' ' && p[i] != '\t') //get necessary play file
		{
			if( i < 60)
			{
				videoFile[i] = p[i];
			}
			i++;
		}
		fprintf(stderr, "videoFile: %s\n", videoFile);
		videofp = (FILE *)getFileHandle(fCamId, videoFile, sdp, &sdpLen);
		if (videofp == NULL)
		{
			return ;
		}
	}
	Boolean sawScaleHeader = parseScaleHeader(fullRequestStr, scale);

	// Try to set the stream's scale factor to this value:
	if (subsession == NULL /*aggregate op*/) {
		fSession->testScaleFactor(scale);
	} else {
		subsession->testScaleFactor(scale);
	}

	char buf[100];
	char* scaleHeader;
	if (!sawScaleHeader) {
		buf[0] = '\0'; // Because we didn't see a Scale: header, don't send one back
	} else {
		sprintf(buf, "Scale: %f\r\n", scale);
	}
	scaleHeader = strDup(buf);

	// Parse the client's "Range:" header, if any:
	double rangeStart = 0.0, rangeEnd = 0.0;
	Boolean sawRangeHeader = parseRangeHeader(fullRequestStr, rangeStart, rangeEnd);

	// Use this information, plus the stream's duration (if known), to create
	// our own "Range:" header, for the response:
	float duration = subsession == NULL /*aggregate op*/
		? fSession->duration() : subsession->duration();
	if (duration < 0.0) {
		// We're an aggregate PLAY, but the subsessions have different durations.
		// Use the largest of these durations in our header
		duration = -duration;
	}

	if (rangeEnd <= 0.0 || rangeEnd > duration) rangeEnd = duration;
	if (rangeStart < 0.0) {
		rangeStart = 0.0;
	} else if (rangeEnd > 0.0 && scale > 0.0 && rangeStart > rangeEnd) {
		rangeStart = rangeEnd;
	}

	char* rangeHeader;
	if (!sawRangeHeader) {
		buf[0] = '\0'; // Because we didn't see a Range: header, don't send one back
	} else if (rangeEnd == 0.0 && scale >= 0.0) {
		sprintf(buf, "Range: npt=%.3f-\r\n", rangeStart);
	} else {
		sprintf(buf, "Range: npt=%.3f-%.3f\r\n", rangeStart, rangeEnd);
	}
	rangeHeader = strDup(buf);

	// Create a "RTP-Info:" line.  It will get filled in from each subsession's state:
	char const* rtpInfoFmt =
		"%s" // "RTP-Info:", plus any preceding rtpInfo items
		"%s" // comma separator, if needed
		"url=%s/%s"
		";seq=%d"
		";rtptime=%u"
		;
	unsigned rtpInfoFmtSize = strlen(rtpInfoFmt);
	char* rtpInfo = strDup("RTP-Info: ");
	unsigned i, numRTPInfoItems = 0;

	// Do any required seeking/scaling on each subsession, before starting streaming:
	for (i = 0; i < fNumStreamStates; ++i) {
		if (subsession == NULL /* means: aggregated operation */
			|| subsession == fStreamStates[i].subsession) {
			if (sawScaleHeader) {
				fStreamStates[i].subsession->setStreamScale(fOurSessionId,
															fStreamStates[i].streamToken,
															scale);
			}
			if (sawRangeHeader) {
				fStreamStates[i].subsession->seekStream(fOurSessionId,
														fStreamStates[i].streamToken,
														rangeStart);
			}
		}
	}

	// Now, start streaming:
	for (i = 0; i < fNumStreamStates; ++i) {
		if (subsession == NULL /* means: aggregated operation */
			|| subsession == fStreamStates[i].subsession) {
			unsigned short rtpSeqNum = 0;
			unsigned rtpTimestamp = 0;
			fStreamStates[i].subsession->startStream(fOurSessionId,
													 fStreamStates[i].streamToken,
													 (TaskFunc*)noteClientLiveness, this,
													 rtpSeqNum, rtpTimestamp,
													 handleAlternativeRequestByte, this, videoActive?videofp:NULL, fMagic, False);
			const char *urlSuffix = fStreamStates[i].subsession->trackId();
			char* prevRTPInfo = rtpInfo;
			unsigned rtpInfoSize = rtpInfoFmtSize
				+ strlen(prevRTPInfo)
				+ 1
				+ rtspURLSize + strlen(urlSuffix)
				+ 5 /*max unsigned short len*/
				+ 10 /*max unsigned (32-bit) len*/
				+ 2 /*allows for trailing \r\n at final end of string*/;
			rtpInfo = new char[rtpInfoSize];
			sprintf(rtpInfo, rtpInfoFmt,
					prevRTPInfo,
					numRTPInfoItems++ == 0 ? "" : ",",
					rtspURL, urlSuffix,
					rtpSeqNum,
					rtpTimestamp
				   );
			delete[] prevRTPInfo;
		}
	}
	if (numRTPInfoItems == 0) {
		rtpInfo[0] = '\0';
	} else {
		unsigned rtpInfoLen = strlen(rtpInfo);
		rtpInfo[rtpInfoLen] = '\r';
		rtpInfo[rtpInfoLen+1] = '\n';
		rtpInfo[rtpInfoLen+2] = '\0';
	}

	// Fill in the response:
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			 "RTSP/1.0 200 OK\r\n"
			 "CSeq: %s\r\n"
			 "%s"
			 "%s"
			 "%s"
			 "Session: %08X\r\n"
			 "%s\r\n",
			 cseq,
			 dateHeader(),
			 scaleHeader,
			 rangeHeader,
			 fOurSessionId,
			 rtpInfo);
	send(fClientSocket, (char const*)fResponseBuffer, strlen((char*)fResponseBuffer), 0);

	for (i = 0; i < fNumStreamStates; ++i) {
		if (subsession == NULL /* means: aggregated operation */
			|| subsession == fStreamStates[i].subsession) {
			unsigned short rtpSeqNum = 0;
			unsigned rtpTimestamp = 0;
			fStreamStates[i].subsession->startStream(fOurSessionId,
													 fStreamStates[i].streamToken,
													 (TaskFunc*)noteClientLiveness, this,
													 rtpSeqNum, rtpTimestamp,
													 handleAlternativeRequestByte, this, videoActive?videofp:NULL, fMagic, True);
		}
	}
	delete[] rtpInfo; delete[] rangeHeader;
	delete[] scaleHeader; delete[] rtspURL;
}

void RTSPServer::RTSPEncoderSession
::handleCmd_PAUSE(ServerMediaSubsession* subsession, char const* cseq) {


	for (unsigned i = 0; i < fNumStreamStates; ++i) {
		if (subsession == NULL /* means: aggregated operation */
			|| subsession == fStreamStates[i].subsession) {
			fStreamStates[i].subsession->pauseStream(fOurSessionId,
													 fStreamStates[i].streamToken);
		}
	}
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			 "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sSession: %08X\r\n\r\n",
			 cseq, dateHeader(), fOurSessionId);
}



void RTSPServer::RTSPEncoderSession
::handleCmd_GET_PARAMETER_SubDirList(ServerMediaSubsession* subsession, char const* cseq,
									 char const* fullRequestStr) {
	char *content = {0};
	char user_agent[30] = {0};
	char session[30] = {0};
	char list_param[30] = {0};
	char count_str[10] = {0};
	char index_str[10] = {0};
	int camid = 0;
	int count = 0; // input/output parameter
	int index = 0;
	int rest_count = 0;
	char **dirList = NULL;
	const char *pos = NULL;
	int getCount = 0;
	int i = 0;
	
	parseSpecifiedStringParam(fullRequestStr,user_agent,(char *)"User-Agent:",11); 
	parseSpecifiedStringParam(fullRequestStr,session,(char *)"Session:",8); 
	parseSpecifiedStringParam(fullRequestStr,list_param,(char *)"List-Param:",11); 

	pos = strstr((char const *)list_param, (char const *)"pagesize=");
	pos += strlen("pagesize=");
	while (pos[i] != ';')
	{
		count_str[i] = pos[i];
		i++;
	}
	i = 0;
	getCount = count = atoi(count_str);
	pos = strstr((char const *)list_param, (char const *)"index="); 
	pos += strlen("index=");
	while ( pos[i] >= '0' && pos[i] <= '9')
	{
		index_str[i] = pos[i];
		i++;
	}
	index = atoi(index_str);
	if(dirList == NULL)
	{
		dirList = new (char *[getCount]);
	}
	for(i = 0; i < getCount; i++)
	{
		dirList[i] = new char[30];
	}
	for(i = 0; i < getCount; i++)
	{
		memset(dirList[i], 0x0, 30);
	}
	getRecordDirectories(camid, dirList, &count, &index, &rest_count); //count may be is change

	content = new (char [30 * count]);
	memset(content, 0x0, 30 * count);
	char *p = content;
	for(i = 0; i < count; i++)
	{
		p += sprintf(p, "%s\r\n", dirList[i]);
	}

	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			 "RTSP/1.0 200 OK\r\nCSeq: %s\r\nContent-Type: application/listdir\r\nContent-Length: %d\r\n"
			 "List-Param: pagesize=%d;restcount=%d\r\nUser-Agent: %s\r\n\r\n%s",
			 cseq,strlen(content),count,rest_count,user_agent,content);
	for(i = 0; i < getCount; i++)
	{
		delete [] dirList[i];
		dirList[i] = NULL;
	}
	delete [] dirList;  dirList = NULL;
	delete [] content;  content = NULL;
}

void RTSPServer::RTSPEncoderSession
::handleCmd_GET_PARAMETER_SubFileList(ServerMediaSubsession* subsession, char const* cseq,
									  char const* fullRequestStr) {
	char user_agent[30] = {0};
	char session[30] = {0};
	char *content = NULL;
	char getTime[30];
	char list_param[30] = {0};
	char count_str[10] = {0};
	char index_str[10] = {0};
	char *getTimePosBegin = NULL;
	char *getTimePosEnd = NULL;
	char **fileList = NULL;
	const char *pos = NULL;
	int camid = 0;
	int count = 0; 
	int index = 0;
	int rest_count = 0;
	int getCount = 0;;
	int i = 0;

	parseSpecifiedStringParam(fullRequestStr,user_agent,(char *)"User-Agent:",11);
	parseSpecifiedStringParam(fullRequestStr,session,(char *)"Session:",8);
	parseSpecifiedStringParam(fullRequestStr,list_param,(char *)"List-Param:",11); 



	pos = strstr((char const *)list_param, (char const *)"pagesize=");
	pos += strlen("pagesize=");
	while( pos[i] != ';')
	{
		count_str[i] = pos[i];
		i++;
	}
	i = 0;
	getCount = count = atoi(count_str);
	pos = strstr((char const *)list_param,(char *)"index="); 
	pos += strlen("index=");
	while( pos[i] >= '0' && pos[i+1] <= '9' )
	{
		index_str[i] = pos[i];
		i++;
	}
	index = atoi(index_str);

	getTimePosBegin = strstr((char *)fullRequestStr, "\r\n\r\n");
	getTimePosBegin += 4;
	while(*getTimePosBegin == ' ')  ++getTimePosBegin;
	getTimePosEnd = strstr(getTimePosBegin, "\r\n");
	memcpy(getTime, getTimePosBegin, getTimePosEnd-getTimePosBegin);
	//Debug(ckite_log_message, "getTime = %s\n", getTime);

	if(fileList == NULL)
	{
		fileList = new (char *[getCount]);
	}
	for(i = 0; i < getCount; i++)
	{
		fileList[i] = new char[30];
		memset(fileList[i], 0x0, 30);
	}

	getRecordFiles( camid, getTime, fileList, &count, &index, &rest_count);

	content = new (char[30 * count]);
	memset(content, 0x0, 30 * count);
	char *p = content;
	for(i = 0; i < count; i++)
	{
		p += sprintf(p, "%s\r\n", fileList[i]);
	}

	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			 "RTSP/1.0 200 OK\r\nCSeq: %s\r\nContent-Type: application/listfile\r\nContent-Length: %d\r\n"
			 "List-Param: pagesize=%d;restcount=%d\r\nUser-Agent: %s\r\n\r\n%s",
			 cseq, strlen(content), count, rest_count, user_agent, content);

	for(i = 0; i < getCount; i++)
	{
		delete [] fileList[i];
		fileList[i] = NULL;
	}
	delete [] fileList; fileList = NULL;
	delete [] content; content = NULL;
}

void RTSPServer::RTSPEncoderSession
::handleCmd_GET_PARAMETER(ServerMediaSubsession* subsession, char const* cseq,
						  char const* fullRequestStr) {
	char contentTypeStr[30] = {0};
	int contentTypeLen = 0;


	Debug(ckite_log_message, "GET PARAMETER \n");
	contentTypeLen = parseSpecifiedStringParam(fullRequestStr,contentTypeStr,(char *)"Content-Type:",13);
	if(!contentTypeLen)
	{
		Debug(ckite_log_message, "parseContentTypeHeader para is error");
		return ;
	}

	Debug(ckite_log_message, "contentTypeStr = %s, listfile size = %d\n", contentTypeStr, strlen("application/listfile"));
	if(strncmp(contentTypeStr,"application/listdir",strlen("application/listdir")) == 0){
		//return listdir's sub dir list
		handleCmd_GET_PARAMETER_SubDirList(subsession, cseq, fullRequestStr);
	}else if(strncmp(contentTypeStr,"application/listfile",strlen("application/listfile")) == 0){
		//return file's list
		handleCmd_GET_PARAMETER_SubFileList(subsession, cseq, fullRequestStr);
	}else{
		//handle other	
	}

}

void RTSPServer::RTSPEncoderSession
::handleCmd_SET_PARAMETER_ResponsePtz(ServerMediaSubsession* subsession/*subsession*/, char const* cseq,
									  char const* fullRequestStr){

	char ptzAction[20]={0}; 
	char speedBuf[5] = {0};
	int speed = 0;
	Debug(ckite_log_message, "fullRequestStr = %s\n", fullRequestStr);
	Debug(ckite_log_message, "ptz\n");
	if(!parseSpecifiedStringParam(fullRequestStr,ptzAction,(char *)"action:",7))
	{
#ifdef DEBUG
		Debug(ckite_log_message, "handleCmd_SET_PARAMETER_ResponsePtz para is error");
#endif
		return ;
	}

	if(!parseSpecifiedStringParam(fullRequestStr, speedBuf, (char *)"Speed:", 6))
	{
		speed = 0;	
	}
	else
	{
		speed = atoi((char *)speedBuf);
	}
	if(strcmp(ptzAction,"left stop") == 0){
		// handle left stop 
		deviceHandlePtz(LEFTSTOP, speed);
	}else if(strcmp(ptzAction,"right stop") == 0){
		// handle right stop 
		deviceHandlePtz(RIGHTSTOP, speed);
	}else if(strcmp(ptzAction,"up stop") == 0){
		// handle up stop 
		deviceHandlePtz(UPSTOP, speed);
	}else if(strcmp(ptzAction,"down stop") == 0){
		// handle down stop 
		deviceHandlePtz(DOWNSTOP, speed);
	}else if(strcmp(ptzAction,"autostop") == 0){
		// handle autostop
		deviceHandlePtz(AUTOSTOP, speed);
	}else if(strcmp(ptzAction,"focusfar stop") == 0){
		// handle focusfar stop
		deviceHandlePtz(FOCUSFAR, speed);
	}else if(strcmp(ptzAction,"focusnear stop") == 0){
		// handle focusnear stop 
		deviceHandlePtz(FOCUSNEAR, speed);
	}else if(strcmp(ptzAction,"zoomtele stop") == 0){
		// handle zoomtele stop
		deviceHandlePtz(ZOOMTELESTOP, speed);
	}else if(strcmp(ptzAction,"zoomwide stop") == 0){
		// handle zoomwide stop 
		deviceHandlePtz(ZOOMWIDESTOP, speed);
	}else if(strcmp(ptzAction,"irisopen stop") == 0){
		// handle irisopen stop 
		deviceHandlePtz(IRISOPENSTOP, speed);
	}else if(strcmp(ptzAction,"irisclose stop") == 0){
		// handle Irisclose stop 
		deviceHandlePtz(IRISCLOSESTOP, speed);
	}else if(strcmp(ptzAction,"wiperon") == 0){
		// handle wiperon
		deviceHandlePtz(WIPERON, speed);
	}else if(strcmp(ptzAction,"wiperoff") == 0){
		// handle wiperoff
		deviceHandlePtz(WIPEROFF, speed);
	}else if(strcmp(ptzAction,"lighton") == 0){
		// handle lighton 
		deviceHandlePtz(LIGHTON, speed);
	}else if(strcmp(ptzAction,"lightoff") == 0){
		// handle lightoff
		deviceHandlePtz(LIGHTOFF, speed);
	}else if(strcmp(ptzAction,"left") == 0){
		// handle left
		deviceHandlePtz(LEFT, speed);
	}else if(strcmp(ptzAction,"right") == 0){
		// handle right  
		deviceHandlePtz(RIGHT, speed);
	}else if(strcmp(ptzAction,"up") == 0){
		// handle up  
		deviceHandlePtz(UP, speed);
	}else if(strcmp(ptzAction,"down") == 0){
		// handle down 			
		deviceHandlePtz(DOWN, speed);
	}else if(strcmp(ptzAction,"auto") == 0){
		// handle auto 
		deviceHandlePtz(AUTO, speed);
	}else if(strcmp(ptzAction,"stop") == 0){
		// handle stop 
		deviceHandlePtz(STOP, speed);
	}else{
		//other handle
	}
#if 1
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			 "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s\r\n",
			 cseq, dateHeader());
#endif 
}

void RTSPServer::RTSPEncoderSession
::handleCmd_SET_PARAMETER_ResponseScene(ServerMediaSubsession* subsession/*subsession*/, char const* cseq,
										char const* fullRequestStr){

	char sceneType[20]={0}; 
	if(!parseSpecifiedStringParam(fullRequestStr,sceneType,(char *)"scene:",7))
	{
#ifdef DEBUG
		Debug(ckite_log_message, "handleCmd_SET_PARAMETER_ResponsePtz para is error");
#endif
		return ;
	}
	if(strcmp(sceneType,"disarm") == 0){
		// handle disarm
		deviceHandleArmAndDisarmScene(DISARM);
	}else if(strcmp(sceneType,"zone-arm") == 0){
		// handle zone-arm
		deviceHandleArmAndDisarmScene(ZONEARM);
	}else if(strcmp(sceneType,"active-alarm") == 0){
		// handle active-alarm 
		deviceHandleArmAndDisarmScene(ACTIVEALARM);
	}else if(strcmp(sceneType,"stop-alarm") == 0){
		// handle stop-alarm
		deviceHandleArmAndDisarmScene(STOPALARM);
	}else if(strcmp(sceneType,"arm") == 0){
		// handle arm 
		deviceHandleArmAndDisarmScene(ARM);
	}else{
		//handle other
	}
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			 "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s\r\n",
			 cseq, dateHeader());
}

void RTSPServer::RTSPEncoderSession
::handleCmd_SET_PARAMETER(ServerMediaSubsession* subsession/*subsession*/, char const* cseq,
						  char const* fullRequestStr) {

	char contentTypeStr[30]={0};
	int contentTypeLen = 0;
	contentTypeLen = parseSpecifiedStringParam(fullRequestStr,contentTypeStr,(char *)"Content-Type:",13);
	if(!contentTypeLen)
	{
#ifdef DEBUG
		Debug(ckite_log_message, "parseContentTypeHeader para is error");
#endif
		return ;
	}
	if(strcmp(contentTypeStr,"application/command")==0 && strstr(fullRequestStr,"reset")){
		deviceReset();
		snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			 "RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s\r\n",
			 cseq, dateHeader());
	}else if(strcmp(contentTypeStr,"text/parameter")==0 && strstr(fullRequestStr,"camera:")){
	//	handleCmd_SET_PARAMETER_ResponseCameraSwitch(subsession, cseq, fullRequestStr); /*camera*/
	}else if(strcmp(contentTypeStr,"application/ptz")==0 && strstr(fullRequestStr,"action:")){	
		handleCmd_SET_PARAMETER_ResponsePtz(subsession, cseq, fullRequestStr);
	}else if(strcmp(contentTypeStr,"application/command")==0 && strstr(fullRequestStr,"scene:")){
		handleCmd_SET_PARAMETER_ResponseScene(subsession, cseq, fullRequestStr);
	}else if(strcmp(contentTypeStr,"application/command")==0 && strstr(fullRequestStr,"start_recode:")){
		//handleCmd_SET_PARAMETER_ResponseStartRecode(subsession, cseq, fullRequestStr);
	}else if(strcmp(contentTypeStr,"application/command")==0 && strstr(fullRequestStr,"stop_recode:")){
		//handleCmd_SET_PARAMETER_ResponseStopRecode(subsession, cseq, fullRequestStr);
	}else if(strcmp(contentTypeStr, "application/upgrade-status")==0){
		char *upgrade_status = new char [128];
		char *upgrade_version = new char [64];
		getUpgradeStatus(upgrade_version, upgrade_status);
		snprintf((char *)fResponseBuffer, sizeof fResponseBuffer, 
				"RTSP/1.0 200 OK\r\nCSeq: %s\r\nContent-Type: %s\r\nContent-Lenght: %d\r\n\r\n%s\r\n%s\r\n",
				cseq, contentTypeStr, strlen(upgrade_status)+strlen(upgrade_version)+4, 
				upgrade_status, upgrade_version);
	}else{	
		//other handle
		return ;
    }
}

void RTSPServer::RTSPEncoderSession::reNoteLiveness(void)
{
	envir().taskScheduler().turnOnBackgroundReadHandling(fClientSocket,
											(TaskScheduler::BackgroundHandlerProc*)&incomingRequestHandler, this);
	envir().taskScheduler().scheduleDelayedTask(HEARTBEAT_PERIOD, (TaskFunc*)heartbeat, this);
	noteLiveness();
}

void RTSPServer::RTSPEncoderSession::heartbeat(void* arg)
{
	RTSPEncoderSession* encoder = (RTSPEncoderSession*)arg;
	encoder->noteLiveness();
	if (!encoder->do_heartbeat())
	{
		Debug(ckite_log_message, "register again\n");
			//encoder->register_Platform(&encoder->fClientAddr, true);
		memset( encoder->fSerial, 0x0 , sizeof(encoder->fSerial));
		if ( encoder->register_Platform( &encoder->fClientAddr, True) )
		{
			encoder->reNoteLiveness();
		}
	}
	Debug(ckite_log_message, "HEARTBEAT_PERIOD = %u\n", HEARTBEAT_PERIOD);
	Debug(ckite_log_message, "heartbeat = %p\n", heartbeat);
	Debug(ckite_log_message, "encoder = %p\n", encoder);
	encoder->envir().taskScheduler().scheduleDelayedTask(HEARTBEAT_PERIOD, (TaskFunc*)heartbeat, encoder);
}

int RTSPServer::RTSPEncoderSession
::do_heartbeat(bool with_sdp, bool with_challenge_resp, char* challenge_resp)
{
	char url[256];
	char * p = (char*)fResponseBuffer;
	char * sdp = NULL;
	int ret;
	struct timeval timeout = {10,0};
        struct timeval timeout1 = {0,0};

	if (fClientSocket < 0)
	{
		Debug(ckite_log_message, "The socket is broken!\n");
		return 0;
	}
	if (strlen(fSerial) == 0)	// This should never happen
	{
		Debug(ckite_log_message, "No available SN in heartbeat()!\n");
		return 0;
	}
	setsockopt(fClientSocket,SOL_SOCKET,SO_SNDTIMEO,(char *)&timeout,sizeof(struct timeval));
	printf("do_heartbeat1\n");	
	snprintf(url, sizeof(url)-1, "rtsp://%s:%d/%s/%d/%s/%s", fLocalIP, fLocalPort, fPath, fCamId, fSerial, GetRandom());


	if (with_sdp)
		sdp = fSession->generateSDPDescription();
	p += snprintf(p, sizeof(fResponseBuffer)-1, "ANNOUNCE %s RTSP/1.0\r\n", url);
	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "CSeq: %d\r\n", fCSeq++);
	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "User-Agent: %s\r\n", fUserAgent);
	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "Version: BuildNo%s\r\n", LAST_MODIFY_TIME);
	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "Content-type: application/sdp\r\n");
	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "Content-length: %d\r\n", with_sdp?strlen(sdp):0);
	if (with_challenge_resp)
		p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "Challenge-Response: %s\r\n", challenge_resp);
	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "\r\n");
	if (with_sdp)
		p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "%s", sdp);

	ret = send(fClientSocket, (char*)fResponseBuffer, p - (char*)fResponseBuffer, 0);
	if (ret > 0)
		Debug(ckite_log_message, "send heartbeat msg is sucess\n");
	if (ret < 0)
	{
		ret = 0;
		ServerMediaSubsession* subsession = NULL;
		for (unsigned i = 0; i < fNumStreamStates; ++i) {
			if (subsession == NULL /* means: aggregated operation */
				|| subsession == fStreamStates[i].subsession) {
//				fStreamStates[i].subsession->closeStreamSource()
				fStreamStates[i].subsession->deleteStream(fOurSessionId,
					fStreamStates[i].streamToken);
				printf("fStreamStates[%d].subsession->deleteStream(%x)\n", i, fStreamStates[i].subsession);
			}
		}
		envir().taskScheduler().turnOffBackgroundReadHandling(fClientSocket);
		::closesocket(fClientSocket);
		fClientSocket = -1;
		printf("Failed to send heartbeat msg to streaming server, ret %d\n", ret);
		Debug(ckite_log_message, "Failed to send heartbeat msg to streaming server, code = %d\n", ret);
		ret = 0;
		//return 0;
	}

	resetRequestBuffer(); // to prepare for any subsequent request

	if (with_sdp)
		delete [] sdp;	
	return ret;
}

void RTSPServer::RTSPEncoderSession
::deviceAlarm(char *session_name, int alarm_code)
{
	char *fBuffer;
	static int size = 1024;
	char url[256] = {NULL};
	int ret = 0;
	char alarmContent[16] = {0};
	unsigned char alarm_type, alarm_chn;

	alarm_type = ((unsigned int)alarm_code) >> 24;
	alarm_chn = ((unsigned int)alarm_code) & 0xff;

	fBuffer = new char [size];
	char *p = (char *)fBuffer;

	if (alarm_type == 0x01) // on/off alarm
	{
		snprintf(alarmContent, sizeof(alarmContent)-1, "din%d\r\n", alarm_chn); 
	}
	else if (alarm_type == 0x02) // motion detect alarm 
	{
		snprintf(alarmContent, sizeof(alarmContent)-1, "motion%d\r\n", alarm_chn);
	}
	else if (alarm_type == 0x03) // loss alarm
	{
		snprintf(alarmContent, sizeof(alarmContent)-1, "vloss%d\r\n", alarm_chn);
	}
	else if (alarm_type == 0x04) // cover alarm
	{
		snprintf(alarmContent, sizeof(alarmContent)-1, "vcover%d\r\n", alarm_chn);
	}

	snprintf(url, sizeof(url)-1, "rtsp://%s:%d/%s/%d/%s/%s", fLocalIP, fLocalPort, session_name, fCamId, fSerial, GetRandom());
	p += snprintf(p, size-1, "ANNOUNCE %s RTSP/1.0\r\n", url);
	p += snprintf(p, size-1-(p-(char*)fBuffer), "CSeq: %d\r\n", fCSeq++);
	p += snprintf(p, size-1-(p-(char*)fBuffer), "Content-Type: %s\r\n", "application/alarm");
	p += snprintf(p, size-1-(p-(char*)fBuffer), "Content-Length: %d\r\n", strlen((char const *)alarmContent));
	p += snprintf(p, size-1-(p-(char*)fBuffer), "User-Agent: %s\r\n", fUserAgent);
	p += snprintf(p, size-1-(p-(char*)fBuffer), "Session: %d\r\n\r\n", fOurSessionId);
	p += snprintf(p, size-1-(p-(char*)fBuffer), "%s",alarmContent);

	Debug(ckite_log_message, "[alarm] fBuffer = %s\n", fBuffer);

    ret = send(fClientSocket, (char*)fBuffer, p - (char*)fBuffer, 0);
	delete [] fBuffer;
}
	int RTSPServer::RTSPEncoderSession
::register_Platform(struct sockaddr_in* server_addr, bool do_send)
{
	char url[512];
	char * p = (char*)fResponseBuffer;
	char * sdp = NULL;
	int ret;
	char activeTime[20] = {0};
	time_t now = time(0); 
	tm *tnow = localtime(&now); 
	struct timeval timeout = {10,0};
  	struct timeval timeout1 = {0,0};
	sprintf(activeTime,"%d-%d-%d", 1900+tnow->tm_year, tnow->tm_mon+1, tnow->tm_mday);//tnow->tm_hour, tnow->tm_min, tnow->tm_sec 
	Debug(ckite_log_message, "activeTime = %s\n", activeTime);
	Debug(ckite_log_message, "Enter register_Platform\n");

	if(strlen(fSerial) == 0)
	{
		readSerial(fSerial);
	}

	if (strlen(fSerial) == 0)
	{
		snprintf(url, sizeof(url)-1, "rtsp://%s:%d/%s/%d/MAC=%s/%s/version:%s/build:%s", 
										fLocalIP, fLocalPort, fPath, fCamId, fMac, GetRandom(),STARV_SDK_VERSION,activeTime);
	}
	else
	{
		snprintf(url, sizeof(url)-1, "rtsp://%s:%d/%s/%d/%s/%s", fLocalIP, fLocalPort, fPath, fCamId, fSerial, GetRandom());
	}
	
	if (videoActive == False)
	{
		sdp = fSession->generateSDPDescription();
	}
	p += snprintf(p, sizeof(fResponseBuffer)-1, "ANNOUNCE %s RTSP/1.0\r\n", url);
	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "CSeq: %d\r\n", fCSeq++);
	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "User-Agent: %s\r\n", fUserAgent);
	if(strlen(fSerial)){
#if 1
		get_LanWebURL(p, strlen(p));
		get_WanWebURL(p, strlen(p));
		get_P2PLanURL(p, strlen(p));
		get_P2PWanURL(p, strlen(p));
		get_LanFtpURL(p, strlen(p));
		get_WanFtpURL(p, strlen(p));
		get_LanVoiceAddr(p, strlen(p));
		get_WanVoiceAddr(p, strlen(p));
		get_LanDataAddr(p, strlen(p));
		get_WanDataAddr(p, strlen(p));
		p += strlen(p);
#endif 
	}

	p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "Content-type: application/sdp\r\n");
	if (videoActive == False)
	{
		p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "Content-length: %d\r\n", strlen(sdp));
		p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "\r\n");
		p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "%s", sdp);
	}
	else
	{
		p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "Content-length: %d\r\n", 0);
		p += snprintf(p, sizeof(fResponseBuffer)-1-(p-(char*)fResponseBuffer), "\r\n");
	}

	if (do_send)
	{
		int nFlag = 1;
		fClientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
		if (fClientSocket < 0)
		{
			Debug(ckite_log_message, "Failed to create socket in RTSPEncoderSession.\n");
			return 0;
		}
		setsockopt(fClientSocket,IPPROTO_TCP,TCP_NODELAY, (char *)&nFlag, sizeof(int));
		setsockopt(fClientSocket,SOL_SOCKET,SO_SNDTIMEO,(char *)&timeout,sizeof(struct timeval));
		printf("register connect\n");
		ret = connect(fClientSocket, (sockaddr *)server_addr, sizeof(struct sockaddr_in));
		printf("register connect end=%d\n",ret);
		if (ret < 0)
		{
			Debug(ckite_log_message, "Failed to connect streaming server in RTSPEncoderSession, code = %d.\n", ret);
			::closesocket(fClientSocket);
			fClientSocket = -1;

			ret = 0; 

		}
		else
		{
			Debug(ckite_log_message, "Connected to streaming server, port %d\n", server_addr->sin_port);
		}

		ret = send(fClientSocket, (char*)fResponseBuffer, p - (char*)fResponseBuffer, 0);
		if (ret < 0)
		{
			Debug(ckite_log_message, "Failed to send msg to streaming server, code = %d\n", ret);



			::closesocket(fClientSocket);

			fClientSocket = -1;
			ret = 0;

		}
		resetRequestBuffer(); // to prepare for any subsequent request
	}
	if(sdp)
	{
		delete [] sdp;	
	}

	return ret; 
}


	RTSPServer::RTSPEncoderSession*
RTSPServer::createNewEncoderSession(ServerMediaSession* session, unsigned sessionId, struct sockaddr_in server_addr,
									char* local_ip, unsigned short local_port, char* path, char* macaddr, char* sn,
									char* ua, unsigned int cam_id) 
{
	return new RTSPEncoderSession(*this, session, sessionId, server_addr, local_ip, local_port, path, macaddr, sn, ua, cam_id);
}

char * RTSPServer::RTSPEncoderSession::GetRandom()
{
	int i;
	char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

	for (i=0; i<16; i++)
	{
		fRandom[i] = charset[rand() % (sizeof(charset)-1)];
	}
	fRandom[16] = 0;
	return fRandom;
}

static int allSessionState[4] ;
int setSessionState(char *session_name, int st) 
{
	char *allSessionName[4] = {(char *)"live", (char *)"mobile", (char *)"store", (char *)"livehd"};
	int i = 0;

	while ((i < 4) && session_name)
	{
		if (strcmp(session_name, allSessionName[i]) == 0)
		{
			allSessionState[i] = st;
			break;
		}
		i++;
	}
	return 0;
}

int getSessionState(char *session_name)
{
	char *allSessionName[4] = {(char *)"live", (char *)"mobile", (char *)"store", (char *)"livehd"};
	int i = 0;
	int state = 0;

	while ((i < 4) && session_name)
	{
		if (strcmp(session_name, allSessionName[i]) == 0)
		{
			state = allSessionState[i];
			break;
		}
		i++;
	}
	return state;
}
