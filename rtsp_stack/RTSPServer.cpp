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
// A RTSP server
// Implementation
#include "RTSPServer.h"
#include "RTSPCommon.h"
#include "GroupsockHelper.h"

#include "BasicUsageEnvironment.h"
//#include "MPEG4ESVideoRTPSink.h"
#include "RTCP.h"
#include "EncoderMediaSubsession.h"
#include "EncoderSource.h"
#ifndef __WIN32__
#include <netdb.h>
#include <sys/socket.h>
#endif

#include "rsa_crypto.h"
#include "RTSPSdk.h"
#include "Base64.h"
#include <vector>
#include <cstring>
#include "Debug.h"

extern UsageEnvironment *env;
#if defined(__WIN32__) || defined(_WIN32) || defined(_QNX4)
#else
#include <signal.h>
#define USE_SIGNALS 1
#endif
#include <time.h> // for "strftime()" and "gmtime()"

////////// RTSPServer implementation //////////
std::vector< RTSPServer::RTSPClientSession *> http;
std::vector< RTSPServer::RTSPClientSession *>::iterator iter;

RTSPServer*
RTSPServer::createNew(UsageEnvironment& env, Port ourPort,
		char* authDatabase/*NULL*/,
		unsigned reclamationTestSeconds/*65*/) {
	int ourSocket = -1;

	do {
		ourSocket = setUpOurSocket(env, ourPort);
		if (ourSocket == -1) break;

		return new RTSPServer(env, ourSocket, ourPort, authDatabase,
				reclamationTestSeconds);
	} while (0);

	if (ourSocket != -1) ::closeSocket(ourSocket);
	return NULL;
}

Boolean RTSPServer::lookupByName(UsageEnvironment& env,
		char const* name,
		RTSPServer*& resultServer) {
	resultServer = NULL; // unless we succeed

	Medium* medium;
	if (!Medium::lookupByName(env, name, medium)) return False;

	if (!medium->isRTSPServer()) {
		env.setResultMsg(name, " is not a RTSP server");
		return False;
	}

	resultServer = (RTSPServer*)medium;
	return True;
}

void RTSPServer::addServerMediaSession(ServerMediaSession* serverMediaSession) {
	if (serverMediaSession == NULL) return;

	char const* sessionName = serverMediaSession->streamName();
	Debug(ckite_log_message, "addServerMediaSession sessionName = %s\n", sessionName);
	if (sessionName == NULL) sessionName = "";
	ServerMediaSession* existingSession
		= (ServerMediaSession*)
		(fServerMediaSessions->Add(sessionName,
								   (void*)serverMediaSession));
	removeServerMediaSession(existingSession); // if any
}

ServerMediaSession* RTSPServer::lookupServerMediaSession(char const* streamName) {
	return (ServerMediaSession*)(fServerMediaSessions->Lookup(streamName));
}

void RTSPServer::removeServerMediaSession(ServerMediaSession* serverMediaSession) {
	if (serverMediaSession == NULL) return;

	fServerMediaSessions->Remove(serverMediaSession->streamName());
	if (serverMediaSession->referenceCount() == 0) {
		Medium::close(serverMediaSession);
	} else {
		serverMediaSession->deleteWhenUnreferenced() = True;
	}
}

void RTSPServer::removeServerMediaSession(char const* streamName) {
	removeServerMediaSession(lookupServerMediaSession(streamName));
}

char* RTSPServer::rtspURLPrefix(int clientSocket) const {
	struct sockaddr_in ourAddress;
	if (clientSocket < 0) {
		// Use our default IP address in the URL:
		ourAddress.sin_addr.s_addr = ReceivingInterfaceAddr != 0
			? ReceivingInterfaceAddr
			: ourIPAddress(envir()); // hack
	} else {
#ifdef __WIN32__
		SOCKLEN_T namelen = sizeof ourAddress;
#else
		socklen_t namelen = sizeof ourAddress;
#endif
		getsockname(clientSocket, (struct sockaddr*)&ourAddress, &namelen);
	}

	char urlBuffer[100]; // more than big enough for "rtsp://<ip-address>:<port>/"

	portNumBits portNumHostOrder = ntohs(fServerPort.num());
	if (portNumHostOrder == 554 /* the default port number */) {
		sprintf(urlBuffer, "rtsp://%s/", our_inet_ntoa(ourAddress.sin_addr));
	} else {
		sprintf(urlBuffer, "rtsp://%s:%hu/",
				our_inet_ntoa(ourAddress.sin_addr), portNumHostOrder);
	}

	return strDup(urlBuffer);
}

char* RTSPServer
::rtspURL(ServerMediaSession const* serverMediaSession, int clientSocket) const {
	char* urlPrefix = rtspURLPrefix(clientSocket);
	char const* sessionName = serverMediaSession->streamName();

	char* resultURL = new char[strlen(urlPrefix) + strlen(sessionName) + 1];
	sprintf(resultURL, "%s%s", urlPrefix, sessionName);

	delete[] urlPrefix;
	return resultURL;
}

#define LISTEN_BACKLOG_SIZE 20

int RTSPServer::setUpOurSocket(UsageEnvironment& env, Port& ourPort) {
	int ourSocket = -1;

	do {
		ourSocket = setupStreamSocket(env, ourPort);
		if (ourSocket < 0) break;

		// Make sure we have a big send buffer:
		if (!increaseSendBufferTo(env, ourSocket, 50*1024)) break;

		// Allow multiple simultaneous connections:
		if (listen(ourSocket, LISTEN_BACKLOG_SIZE) < 0) {
			env.setResultErrMsg("listen() failed: ");
			break;
		}

		if (ourPort.num() == 0) {
			// bind() will have chosen a port for us; return it also:
			if (!getSourcePort(env, ourSocket, ourPort)) break;
		}

		return ourSocket;
	} while (0);

	if (ourSocket != -1) ::closeSocket(ourSocket);
	return -1;
}

Boolean RTSPServer
::specialClientAccessCheck(int /*clientSocket*/, struct sockaddr_in& /*clientAddr*/, char const* /*urlSuffix*/) {
	// default implementation
	return True;
}

RTSPServer::RTSPServer(UsageEnvironment& env,
		int ourSocket, Port ourPort,
		char* authDatabase,
		unsigned reclamationTestSeconds)
: Medium(env),
	fServerSocket(ourSocket), fServerPort(ourPort),
	fReclamationTestSeconds(reclamationTestSeconds),
	fServerMediaSessions(HashTable::create(STRING_HASH_KEYS)) {
#ifdef USE_SIGNALS
		// Ignore the SIGPIPE signal, so that clients on the same host that are killed
		// don't also kill us:
		signal(SIGPIPE, SIG_IGN);
#endif

		// Arrange to handle connections from others:
		env.taskScheduler().turnOnBackgroundReadHandling(fServerSocket,
				(TaskScheduler::BackgroundHandlerProc*)&incomingConnectionHandler,
				this);
	}

RTSPServer::~RTSPServer() {

	Debug(ckite_log_message, "[wayde] RTSPServer::~RTSPServer\n");
	// Turn off background read handling:
	envir().taskScheduler().turnOffBackgroundReadHandling(fServerSocket);

	::closeSocket(fServerSocket);

	// Remove all server media sessions (they'll get deleted when they're finished):
	while (1) {
		ServerMediaSession* serverMediaSession
			= (ServerMediaSession*)fServerMediaSessions->RemoveNext();
		if (serverMediaSession == NULL) break;
		removeServerMediaSession(serverMediaSession);
	}

	// Finally, delete the session table itself:
	delete fServerMediaSessions;
}

Boolean RTSPServer::isRTSPServer() const {
	return True;
}

void RTSPServer::incomingConnectionHandler(void* instance, int /*mask*/) {
	RTSPServer* server = (RTSPServer*)instance;
	server->incomingConnectionHandler1();
}

void RTSPServer::incomingConnectionHandler1() {
	struct sockaddr_in clientAddr;
#ifdef __WIN32__
	SOCKLEN_T clientAddrLen = sizeof clientAddr;
#else
	socklen_t clientAddrLen = sizeof clientAddr;
#endif	

	int clientSocket = accept(fServerSocket, (struct sockaddr*)&clientAddr,
			&clientAddrLen);
	if (clientSocket < 0) {
		int err = envir().getErrno();
		if (err != EWOULDBLOCK) {
			envir().setResultErrMsg("accept() failed: ");
		}
		return;
	}
	makeSocketNonBlocking(clientSocket);
	increaseSendBufferTo(envir(), clientSocket, 50*1024);

#if defined(DEBUG) || defined(DEBUG_CONNECTIONS)
	envir() << "accept()ed connection from " << our_inet_ntoa(clientAddr.sin_addr) << '\n';
#endif

	// Create a new object for this RTSP session.
	// (Choose a random 32-bit integer for the session id (it will be encoded as a 8-digit hex number).  We don't bother checking for
	//  a collision; the probability of two concurrent sessions getting the same session id is very low.)
	unsigned sessionId = (unsigned)our_random();
	(void)createNewClientSession(sessionId, clientSocket, clientAddr);
}


////////// RTSPServer::RTSPClientSession implementation //////////

RTSPServer::RTSPClientSession
::RTSPClientSession(RTSPServer& ourServer, unsigned sessionId,
		int clientSocket, struct sockaddr_in clientAddr)
: fOurServer(ourServer), 
	fOurSessionId(sessionId),
	fHttpGetOurSessionId(sessionId),
	fHttpPostOurSessionId(sessionId),
	fOurServerMediaSession(NULL),
	fClientSocket(clientSocket), fClientAddr(clientAddr),
	fHttpGetClientSocket(clientSocket), fHttpGetClientAddr(clientAddr),
	fHttpPostClientSocket(clientSocket),fHttpPostClientAddr(clientAddr),
	IsHttp(False),
	fLivenessCheckTask(NULL),
	fIsMulticast(False), fSessionIsActive(True), fStreamAfterSETUP(False),
	fTCPStreamIdCount(0), fNumStreamStates(0), fStreamStates(NULL) {

		// Arrange to handle incoming requests:
		resetRequestBuffer();
		envir().taskScheduler().turnOnBackgroundReadHandling(fClientSocket,
				(TaskScheduler::BackgroundHandlerProc*)&incomingRequestHandler, this);
		noteLiveness();
	}


RTSPServer::RTSPClientSession::~RTSPClientSession() {

	Debug(ckite_log_message, "[wayde]entry RTSPClientSession::~RTSPClientSession\n");
	// Turn off any liveness checking:
	envir().taskScheduler().unscheduleDelayedTask(fLivenessCheckTask);

	// Turn off background read handling:
	if (fClientSocket >= 0)
	{
	  envir().taskScheduler().turnOffBackgroundReadHandling(fClientSocket);
	  ::closeSocket(fClientSocket);
	  fClientSocket = -1;
	}
	if(IsHttp)
	{
		if (fHttpGetClientSocket >=0)
		{
	  		envir().taskScheduler().turnOffBackgroundReadHandling(fHttpGetClientSocket);
	  		::closeSocket(fHttpGetClientSocket);
	  		fHttpGetClientSocket = -1;
		}
		if (fHttpPostClientSocket >=0)
		{
	  		envir().taskScheduler().turnOffBackgroundReadHandling(fHttpPostClientSocket);
	  		::closeSocket(fHttpPostClientSocket);
	  		fHttpPostClientSocket = -1;
		}
	}

	for( iter = http.begin(); iter != http.end(); iter++)
	{
		if((*iter) == this)
		{
			http.erase(iter);
			break;
		}
	}

	reclaimStreamStates();

	if (fOurServerMediaSession != NULL) {
		fOurServerMediaSession->decrementReferenceCount();
		if (fOurServerMediaSession->referenceCount() == 0
				&& fOurServerMediaSession->deleteWhenUnreferenced()) {
			fOurServer.removeServerMediaSession(fOurServerMediaSession);
		}
	}
}

void RTSPServer::RTSPClientSession::closeHttpSocketAndFreeResource()
{
	Debug(ckite_log_message, "closeHttpSocketAndFreeResource\n");
	delete this;
}

void RTSPServer::RTSPClientSession::reclaimStreamStates() {
	for (unsigned i = 0; i < fNumStreamStates; ++i) {
		if (fStreamStates[i].subsession != NULL) {
				fStreamStates[i].subsession->deleteStream(IsHttp?fHttpGetOurSessionId:fOurSessionId,
					fStreamStates[i].streamToken);
			}
		}
	delete[] fStreamStates; fStreamStates = NULL;
	fNumStreamStates = 0;
}

void RTSPServer::RTSPClientSession::resetRequestBuffer() {
	fRequestBytesAlreadySeen = 0;
	fRequestBufferBytesLeft = sizeof fRequestBuffer;
	memset(fRequestBuffer, 0, sizeof fRequestBuffer);
	fLastCRLF = &fRequestBuffer[-3]; // hack
}

void RTSPServer::RTSPClientSession::incomingRequestHandler(void* instance, int /*mask*/) {
	RTSPClientSession* session = (RTSPClientSession*)instance;
	session->incomingRequestHandler1();
}

void RTSPServer::RTSPClientSession::incomingHttpRequestHandler(void *instance, int /*mask*/) {
	RTSPClientSession* session = (RTSPClientSession*)instance;
	session->incomingHttpRequestHandler1();
}

void RTSPServer::RTSPClientSession::incomingHttpGetRequestHandler(void *instance, int /*mask*/) {
	RTSPClientSession* session = (RTSPClientSession*)instance;
	session->incomingHttpGetRequestHandler1();
}

void RTSPServer::RTSPClientSession::incomingHttpGetRequestHandler1() {
	struct sockaddr_in dummy;
	unsigned char *tempBuf;
	unsigned char *out;
	unsigned int outLen = 0;
	tempBuf = new unsigned char [RTSP_MSG_SIZE];
	memset(tempBuf, 0x0, RTSP_MSG_SIZE);
	int bytesRead = readSocket(envir(), /*(*iter)->*/fHttpPostClientSocket, tempBuf, RTSP_MSG_SIZE, dummy);
	out = base64Decode((char *)tempBuf, outLen, True);
	memcpy(&fRequestBuffer[fRequestBytesAlreadySeen], out, outLen);
	fRequestBufferBytesLeft -= outLen;
	Debug(ckite_log_message, "base64 decoder outLen = %d, fRequestBuffer = %s\n", outLen, &fRequestBuffer[fRequestBytesAlreadySeen]);
	delete [] tempBuf;
	delete [] out;
	if(outLen == 0)
	{
		delete this;
	}
}

void RTSPServer::RTSPClientSession::incomingHttpRequestHandler1() {
	struct sockaddr_in dummy; // 'from' address, meaningless in this case
	unsigned char *tempBuf;
	unsigned char *out;
	unsigned int outLen = 0;
	tempBuf = new unsigned char [RTSP_MSG_SIZE];
	memset(tempBuf, 0x0, RTSP_MSG_SIZE);
	int bytesRead = readSocket(envir(), /*(*iter)->*/fHttpPostClientSocket, tempBuf, RTSP_MSG_SIZE, dummy);
	out = base64Decode((char *)tempBuf, outLen, True);
	memcpy(&fRequestBuffer[fRequestBytesAlreadySeen], out, outLen);
	fRequestBufferBytesLeft -= outLen;
	Debug(ckite_log_message, "base64 decoder outLen = %d, fRequestBuffer = %s\n", outLen, &fRequestBuffer[fRequestBytesAlreadySeen]);

	delete [] tempBuf;
	delete [] out;
	handleRequestBytes(outLen);
}

void RTSPServer::RTSPClientSession::incomingRequestHandler1() {
	struct sockaddr_in dummy; // 'from' address, meaningless in this case

	int bytesRead = readSocket(envir(), fClientSocket, &fRequestBuffer[fRequestBytesAlreadySeen], fRequestBufferBytesLeft, dummy);
	Debug(ckite_log_message, "readSocket start, fRequestBytesAlreadySeen = %d\n", fRequestBytesAlreadySeen);
	Debug(ckite_log_message, "readSocket start, fRequestBuffer = %s\n", &fRequestBuffer[fRequestBytesAlreadySeen]);
	Debug(ckite_log_message, "readSocket start, bytesRead = %d\n", bytesRead);
	handleRequestBytes(bytesRead);
}

void RTSPServer::RTSPClientSession::handleAlternativeRequestByte(void* instance, u_int8_t requestByte) {
	RTSPClientSession* session = (RTSPClientSession*)instance;
	session->handleAlternativeRequestByte1(requestByte);
}

void RTSPServer::RTSPClientSession::handleAlternativeRequestByte1(u_int8_t requestByte) {
	// Add this character to our buffer; then try to handle the data that we have buffered so far:
	if (fRequestBufferBytesLeft == 0 || fRequestBytesAlreadySeen >= RTSP_BUFFER_SIZE) return;
	fRequestBuffer[fRequestBytesAlreadySeen] = requestByte;
	handleRequestBytes(1);
}

void RTSPServer::RTSPClientSession::handleRequestBytes(int newBytesRead) {
	noteLiveness();

	Debug(ckite_log_message, "handleRequestBytes newBytesRead = %d\n", newBytesRead);
	if (newBytesRead <= 0 || (unsigned)newBytesRead >= fRequestBufferBytesLeft) {
		// Either the client socket has died, or the request was too big for us.
		// Terminate this connection:
#ifdef DEBUG
		fprintf(stderr, "RTSPClientSession[%p]::handleRequestBytes() read %d new bytes (of %d); terminating connection!\n", this, newBytesRead, fRequestBufferBytesLeft);
#endif
		if(IsHttp)
		{
			if (fHttpPostClientSocket >=0)
			{
				envir().taskScheduler().turnOffBackgroundReadHandling(fHttpPostClientSocket);
				::closeSocket(fHttpPostClientSocket);
				fHttpPostClientSocket = -1;
			}
		}
		else
		{
			delete this;
		}
		return;
	}

	Boolean endOfMsg = False;
	unsigned char* ptr = &fRequestBuffer[fRequestBytesAlreadySeen];
#if 1

	if (fRequestBuffer[0] == (unsigned char)'$')
	{
		fRequestBytesAlreadySeen += newBytesRead;

		while (fRequestBuffer[0] == (unsigned char)'$')
		{
			// handleIncomingRTPRTCP();
			unsigned short len = (fRequestBuffer[2] << 8) | fRequestBuffer[3];
			if (fRequestBytesAlreadySeen >= len + 4)
			{
				Debug(ckite_log_message, "Discard interleaved RTP/RTCP packet %d bytes\n", len);
				memmove(fRequestBuffer, fRequestBuffer + len + 4, fRequestBytesAlreadySeen- len - 4);
				fRequestBytesAlreadySeen -= len + 4;
			}
			else
			{
				break;
			}
		}
		if (fRequestBytesAlreadySeen > newBytesRead)
		{
			ptr = &fRequestBuffer[fRequestBytesAlreadySeen - newBytesRead];
			fRequestBytesAlreadySeen -= newBytesRead;
		}
		else
		{
			ptr = &fRequestBuffer[0];
			fRequestBufferBytesLeft -= newBytesRead - fRequestBytesAlreadySeen;
			newBytesRead = fRequestBytesAlreadySeen;
			fRequestBytesAlreadySeen = 0;
		}
	}
#endif
	// Look for the end of the message: <CR><LF><CR><LF>
	unsigned char *tmpPtr = ptr;
	if (fRequestBytesAlreadySeen > 0) --tmpPtr;
	// in case the last read ended with a <CR>
	while (tmpPtr < &ptr[newBytesRead-1]) {
		if (*tmpPtr == '\r' && *(tmpPtr+1) == '\n') {
			if (tmpPtr - fLastCRLF == 2) { // This is it:
				endOfMsg = True;
				break;
			}
			fLastCRLF = tmpPtr;
		}
		++tmpPtr;
	}

	fRequestBufferBytesLeft -= newBytesRead;
	fRequestBytesAlreadySeen += newBytesRead;

	if (!endOfMsg) return; // subsequent reads will be needed to complete the request

	// Parse the request string into command name and 'CSeq',
	// then handle the command:
	fRequestBuffer[fRequestBytesAlreadySeen] = '\0';
	char cmdName[RTSP_PARAM_STRING_MAX];
	char urlPreSuffix[RTSP_PARAM_STRING_MAX];
	char urlSuffixWithToken[RTSP_PARAM_STRING_MAX] = {0};
	char urlToken[32] = {0};
	char urlSuffix[RTSP_PARAM_STRING_MAX] = {0};
	char cseq[RTSP_PARAM_STRING_MAX];
	char httpRequestMethod[10];
	char httpContentType[30];
	char httpPragma[30];
	char httpCacheControl[30];
	fprintf(stderr, "fRequestBuffer: %s\n", fRequestBuffer);
	if (!parseRTSPRequestString((char*)fRequestBuffer, fRequestBytesAlreadySeen,
				cmdName, sizeof cmdName,
				urlPreSuffix, sizeof urlPreSuffix,
				urlSuffixWithToken, sizeof urlSuffixWithToken,
				cseq, sizeof cseq)) {
#ifdef DEBUG
		fprintf(stderr, "parseRTSPRequestString() failed!\n");
#endif
		Debug(ckite_log_message, "get or post fRequestBuffer = %s\n", fRequestBuffer);
		if (!parseRTSPGetPostString((char *)fRequestBuffer, fRequestBytesAlreadySeen,
					httpRequestMethod, sizeof httpRequestMethod, 
					httpSessionCookie, sizeof httpSessionCookie,
					httpContentType, sizeof httpContentType,
					httpPragma, sizeof httpPragma,
					httpCacheControl, sizeof httpCacheControl)) {

			handleCmd_bad(cseq);
		} else {

			if (strcmp(httpRequestMethod, "GET") == 0 && strcmp(httpContentType, "application/x-rtsp-tunnelled") == 0) {
				char ua[256] = {0};
				getUserAgent(ua, sizeof(ua));
				// insert into hash lable
				snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
						"HTTP/1.0 200 OK\r\nServer: %s\r\nConnecton: close\r\n%sCache-Control: %s\r\nPragma: %s\r\nContent-Type: %s\r\n\r\n",
						ua, dateHeader(), httpCacheControl, httpPragma, httpContentType);
				Debug(ckite_log_message, "fHttpGetClientSocket = %d\n", fHttpGetClientSocket);

				if( send(fHttpGetClientSocket, (char const*)fResponseBuffer, strlen((char*)fResponseBuffer), 0) == -1)
				{
					Debug(ckite_log_message, "http response get is error\n");
				}
				http.push_back(this);

				resetRequestBuffer();
				fClientSocket = -1;
				return ;
			}else if(strcmp(httpRequestMethod, "POST") == 0) {
				// post client session 

				char const *pos = (char const *)fRequestBuffer;
				//char const *ptr = NULL;
				unsigned char *out;
				unsigned int outLen;

				//for(std::vector<RTSPServer::RTSPClientSession>::size_type i=0; i != http.size(); i++)
				for( iter = http.begin(); iter != http.end(); iter++)
				{
					Debug(ckite_log_message, "httpSessionCookie = %s\n", httpSessionCookie);
					Debug(ckite_log_message, "(*iter)->httpSessionCookie = %s\n", (*iter)->httpSessionCookie);
					if (strncmp(httpSessionCookie, (*iter)->httpSessionCookie, strlen(httpSessionCookie)) == 0)
						break;
				}

				pos = strstr((char const *)fRequestBuffer, "\r\n\r\n");
				if (pos == NULL)
				{
					pos += 4;
					memcpy(fHttpRequestMsg, pos,  strlen(pos));
				}
				if( strlen((char const *)fHttpRequestMsg) > 20)
				{
					out = base64Decode((char *)fHttpRequestMsg, outLen, True);
					if (!parseRTSPRequestString((char *)out, strlen((char const *)fHttpRequestMsg ),
							cmdName, sizeof cmdName,
							urlPreSuffix, sizeof urlPreSuffix,
							urlSuffixWithToken, sizeof urlSuffixWithToken,
							cseq, sizeof cseq)) {
					}
					else 
					{
						fprintf(stderr, "urlPreSuffix: %s\n", urlPreSuffix);
						parseTokenFromURLSuffix(urlSuffixWithToken, urlSuffix, urlToken);
						if(strcmp(cmdName, "DESCRIBE") == 0) {
							handleCmd_DESCRIBE(cseq, urlSuffix, (char const*)fHttpRequestMsg);
						 	send((*iter)->fHttpGetClientSocket, (char const*)fResponseBuffer, strlen((char*)fResponseBuffer), 0);
							resetRequestBuffer(); 
						 }
					}
					delete [] out;
				}
				
				(*iter)->fHttpPostOurSessionId = fHttpPostOurSessionId;
				(*iter)->fHttpPostClientSocket = fHttpPostClientSocket;
				(*iter)->fHttpPostClientAddr = fHttpPostClientAddr;
				(*iter)->IsHttp = True;
				if(fHttpPostClientSocket > 0)
				{
					envir().taskScheduler().turnOffBackgroundReadHandling(fHttpPostClientSocket);
					//::closesocket(fHttpPostClientSocket);
					fHttpPostClientSocket = fHttpGetClientSocket = fClientSocket = -1;
				}
				envir().taskScheduler().turnOnBackgroundReadHandling((*iter)->fHttpPostClientSocket,
						(TaskScheduler::BackgroundHandlerProc*)&incomingHttpRequestHandler, (*iter));

				envir().taskScheduler().turnOnBackgroundReadHandling((*iter)->fHttpGetClientSocket,
						(TaskScheduler::BackgroundHandlerProc*)&incomingHttpGetRequestHandler, (*iter));
				noteLiveness();
				delete this; //delete post session
				return ;
			}
		}
	} else {
		parseTokenFromURLSuffix(urlSuffixWithToken, urlSuffix, urlToken);
#if 1 //def DEBUG
		fprintf(stderr, "parseRTSPRequestString() returned cmdName \"%s\", urlSuffixWithToken \"%s\", urlPreSuffix \"%s\", urlSuffix \"%s\"\n", 
													cmdName, urlSuffixWithToken, urlPreSuffix, urlSuffix);
#endif
		if (strcmp(cmdName, "OPTIONS") == 0) {
			handleCmd_OPTIONS(cseq);
		} else if (strcmp(cmdName, "DESCRIBE") == 0) {
			handleCmd_DESCRIBE(cseq, urlSuffix, (char const*)fRequestBuffer);
		} else if (strcmp(cmdName, "SETUP") == 0) {
			handleCmd_SETUP(cseq, urlPreSuffix, urlSuffix, (char const*)fRequestBuffer);
		} else if (strcmp(cmdName, "TEARDOWN") == 0
				|| strcmp(cmdName, "PLAY") == 0
				|| strcmp(cmdName, "PAUSE") == 0
				|| strcmp(cmdName, "GET_PARAMETER") == 0) {
			handleCmd_withinSession(cmdName, urlPreSuffix, urlSuffix, cseq,
					(char const*)fRequestBuffer);
		} else if(strcmp(cmdName, "SET_PARAMETER") == 0) {
			int i = 0;
			unsigned char *tem_ptr = (unsigned char *)fRequestBuffer;
			while(i < strlen((char const *)fRequestBuffer)) {
				if(tem_ptr[i] == '\r' && tem_ptr[i+1] == '\n' && tem_ptr[i+2] == '\r' && tem_ptr[i+3] == '\n'){
					i += 4;
					break;
				}else{
					i++;
				}
			}
			if(i < (strlen((char const *)fRequestBuffer) -1))
			{
				unsigned char *newBuffer = new unsigned char [strlen((char const*)fRequestBuffer) - i + 1];
				memcpy(newBuffer, tem_ptr + i, strlen((char const*)fRequestBuffer) - i);
				Debug(ckite_log_message, "newBuffer = %s, len = %d\n", newBuffer, strlen((char const *)newBuffer));

				if (!parseRTSPRequestString((char*)newBuffer, fRequestBytesAlreadySeen,
							cmdName, sizeof cmdName,
							urlPreSuffix, sizeof urlPreSuffix,
							urlSuffixWithToken, sizeof urlSuffixWithToken,
							cseq, sizeof cseq)) {
					handleCmd_bad(cseq);
				}else{
					parseTokenFromURLSuffix(urlSuffixWithToken, urlSuffix, urlToken);
					Debug(ckite_log_message, "newbuffer is cmdName \"%s\", urlPreSuffix \"%s\", urlSuffix \"%s\"\n", cmdName, urlPreSuffix, urlSuffix);

					if (strcmp(cmdName, "OPTIONS") == 0) {
						handleCmd_OPTIONS(cseq);
					} else if (strcmp(cmdName, "DESCRIBE") == 0) {
						handleCmd_DESCRIBE(cseq, urlSuffix, (char const*)newBuffer);
					} else if (strcmp(cmdName, "SETUP") == 0) {
						handleCmd_SETUP(cseq, urlPreSuffix, urlSuffix, (char const*)newBuffer);
					} else if (strcmp(cmdName, "TEARDOWN") == 0
							|| strcmp(cmdName, "PLAY") == 0
							|| strcmp(cmdName, "PAUSE") == 0
							|| strcmp(cmdName, "GET_PARAMETER") == 0) {
						handleCmd_withinSession(cmdName, urlPreSuffix, urlSuffix, cseq,
								(char const*)newBuffer);
					}
				}
				delete [] newBuffer;
			}else {
				Debug(ckite_log_message, "cmdName %s not valid . not support 3\n", cmdName);
				handleCmd_notSupported(cseq);
			}
		}
	}

#ifdef DEBUG
	fprintf(stderr, "sending response: %s", fResponseBuffer);
#endif

	if( strcmp(cmdName, "PLAY") != 0 )
	{
		 if(!IsHttp)
		 	send(fClientSocket, (char const*)fResponseBuffer, strlen((char*)fResponseBuffer), 0);
		 else
		 	send(/*(*iter)->*/fHttpGetClientSocket, (char const*)fResponseBuffer, strlen((char*)fResponseBuffer), 0);
	}

	if (strcmp(cmdName, "SETUP") == 0 && fStreamAfterSETUP) {
		// The client has asked for streaming to commence now, rather than after a
		// subsequent "PLAY" command.  So, simulate the effect of a "PLAY" command:
		handleCmd_withinSession("PLAY", urlPreSuffix, urlSuffix, cseq,
				(char const*)fRequestBuffer);
	}

	resetRequestBuffer(); // to prepare for any subsequent request
	if (!fSessionIsActive) 
	{
		delete this;
	}
}

// Handler routines for specific RTSP commands:

// Generate a "Date:" header for use in a RTSP response:
char const* dateHeader() {
	static char buf[200];
#if !defined(_WIN32_WCE)
	time_t tt = time(NULL);
	strftime(buf, sizeof buf, "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", gmtime(&tt));
#else
	// WinCE apparently doesn't have "time()", "strftime()", or "gmtime()",
	// so generate the "Date:" header a different, WinCE-specific way.
	// (Thanks to Pierre l'Hussiez for this code)
	SYSTEMTIME SystemTime;
	GetSystemTime(&SystemTime);
	WCHAR dateFormat[] = L"ddd, MMM dd yyyy";
	WCHAR timeFormat[] = L"HH:mm:ss GMT\r\n";
	WCHAR inBuf[200];
	DWORD locale = LOCALE_NEUTRAL;

	int ret = GetDateFormat(locale, 0, &SystemTime,
			(LPTSTR)dateFormat, (LPTSTR)inBuf, sizeof inBuf);
	inBuf[ret - 1] = ' ';
	ret = GetTimeFormat(locale, 0, &SystemTime,
			(LPTSTR)timeFormat,
			(LPTSTR)inBuf + ret, (sizeof inBuf) - ret);
	wcstombs(buf, inBuf, wcslen(inBuf));
#endif
	return buf;
}

static char const* allowedCommandNames
= "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER";

void RTSPServer::RTSPClientSession::handleCmd_bad(char const* /*cseq*/) {
	// Don't do anything with "cseq", because it might be nonsense
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 400 Bad Request\r\n%sAllow: %s\r\n\r\n",
			dateHeader(), allowedCommandNames);
}

void RTSPServer::RTSPClientSession::handleCmd_notSupported(char const* cseq) {
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 405 Method Not Allowed\r\nCSeq: %s\r\n%sAllow: %s\r\n\r\n",
			cseq, dateHeader(), allowedCommandNames);
}

void RTSPServer::RTSPClientSession::handleCmd_notFound(char const* cseq) {

	Debug(ckite_log_message, "handleCmd_notFound\n");
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 404 Stream Not Found\r\nCSeq: %s\r\n%s\r\n",
			cseq, dateHeader());
	fSessionIsActive = False; // triggers deletion of ourself after responding
}

void RTSPServer::RTSPClientSession::handleCmd_unsupportedTransport(char const* cseq) {
	Debug(ckite_log_message, "handleCmd_unsupportedTransport\n");
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 461 Unsupported Transport\r\nCSeq: %s\r\n%s\r\n",
			cseq, dateHeader());
	fSessionIsActive = False; // triggers deletion of ourself after responding
}

void RTSPServer::RTSPClientSession::handleCmd_OPTIONS(char const* cseq) {
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sPublic: %s\r\n\r\n",
			cseq, dateHeader(), allowedCommandNames);
}

void RTSPServer::RTSPClientSession
::handleCmd_DESCRIBE(char const* cseq, char const* urlSuffix,
		char const* fullRequestStr) {
	char* sdpDescription = NULL;
	char* rtspURL = NULL;
	int width, height;
	char sessionType[20] = {0};


	do {
		//if (!authenticationOK("DESCRIBE", cseq, urlSuffix, fullRequestStr))
		//	break;

		// We should really check that the request contains an "Accept:" #####
		// for "application/sdp", because that's what we're sending back #####

		// Begin by looking up the "ServerMediaSession" object for the
		// specified "urlSuffix":
		fprintf(stderr, "urlSuffix = %s\n", urlSuffix);
		memset(sessionType, 0x0, sizeof sessionType);
		memcpy(sessionType, urlSuffix, strlen((char const *)urlSuffix));
		if (strcmp(sessionType, "livehd") == 0)
		{
			width = 720;
			height = 576;
		}
		else if (strcmp(sessionType, "live") == 0)
		{
			width = 352;
			height = 288;
		}
		else if (strcmp(sessionType, "mobile") == 0)
		{
			width = 176;
			height = 144;
		}
		// Create 'groupsocks' for RTP and RTCP:
		struct in_addr destinationAddress;
		destinationAddress.s_addr = 0;
		// Note: This is a multicast address.  If you wish instead to stream
		// using unicast, then you should use the "testOnDemandRTSPServer"
		// test program - not this test program - as a model.
		unsigned int video_bitrate = 0;
		unsigned int framerate = 0;
		unsigned int keyinterval = 0;
		unsigned int audio_bitrate = 0;
		unsigned int framelength = 0;
		unsigned int samplerate = 0;

		width = 352;
		height = 288;
		memset(sessionType, 0, strlen(sessionType));
		memcpy(sessionType, "live", strlen("live"));
		getSubsessionParaConfig(sessionType, width, height, video_bitrate, framerate, keyinterval, 
															audio_bitrate, framelength, samplerate);
		fprintf(stderr, "sessionType: %s, ramerate: %d\n", sessionType, framerate);
		ServerMediaSession* session = ServerMediaSession::createNew(*env, sessionType, "", "server media session" ,True);
		if (session == NULL) {
			handleCmd_notFound(cseq);
			break;
		}
		EncoderMediaSubsession* p2pAudioSubsession, *p2pVideoSubsession;
		p2pVideoSubsession = EncoderMediaSubsession::createNew(*env, this, False, False, 6970);
		p2pVideoSubsession->SetVideoParameters(-1, width, height, video_bitrate, framerate, keyinterval, sessionType);
		session->addSubsession(p2pVideoSubsession);
#if 0
		p2pAudioSubsession = EncoderMediaSubsession::createNew(*env, this, True, False, 6970);
		p2pAudioSubsession->SetAudioParameters(audio_bitrate, framelength, samplerate, sessionType);
		session->addSubsession(p2pAudioSubsession);
#endif
		fOurServerMediaSession = session;
		fOurServerMediaSession->incrementReferenceCount();
		// Set up our array of states for this session's subsessions (tracks):
		reclaimStreamStates();
		ServerMediaSubsessionIterator iter(*fOurServerMediaSession);
		for (fNumStreamStates = 0; iter.next() != NULL; ++fNumStreamStates) {}
		fStreamStates = new struct streamState[fNumStreamStates]; //
		iter.reset();
		ServerMediaSubsession* subsession;
		for (unsigned i = 0; i < fNumStreamStates; ++i) {
			subsession = iter.next();
			fStreamStates[i].subsession = subsession;
			fStreamStates[i].streamToken = NULL; // for now; reset by SETUP later
		}
		// Then, assemble a SDP description for this session:
		sdpDescription = session->generateSDPDescription();
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
		rtspURL = fOurServer.rtspURL(session, fClientSocket);

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
	} while (0);

	delete[] sdpDescription;
	delete[] rtspURL;
}

void RTSPServer::RTSPClientSession
::handleCmd_SETUP(char const* cseq,
		char const* urlPreSuffix, char const* urlSuffix,
		char const* fullRequestStr) {
	// "urlPreSuffix" should be the session (stream) name, and
	// "urlSuffix" should be the subsession (track) name.
	char const* streamName = urlPreSuffix;
	char const* trackId = urlSuffix;

	// Check whether we have existing session state, and, if so, whether it's
	// for the session that's named in "streamName".  (Note that we don't
	// support more than one concurrent session on the same client connection.) #####
	if (fOurServerMediaSession != NULL
			&& strcmp(streamName, fOurServerMediaSession->streamName()) != 0) {
		fOurServerMediaSession = NULL;
	}
	if (fOurServerMediaSession == NULL) {
		// Set up this session's state.

		// Look up the "ServerMediaSession" object for the specified stream:
		if (streamName[0] != '\0' ||
				fOurServer.lookupServerMediaSession("") != NULL) { // normal case
		} else { // weird case: there was no track id in the URL
			streamName = urlSuffix;
			trackId = NULL;
		}
		fOurServerMediaSession = fOurServer.lookupServerMediaSession(streamName);
		if (fOurServerMediaSession == NULL) {
			Debug(ckite_log_message, "session not found\n");
			handleCmd_notFound(cseq);
			return;
		}

		fOurServerMediaSession->incrementReferenceCount();

		// Set up our array of states for this session's subsessions (tracks):
		reclaimStreamStates();
		ServerMediaSubsessionIterator iter(*fOurServerMediaSession);
		for (fNumStreamStates = 0; iter.next() != NULL; ++fNumStreamStates) {}
		fStreamStates = new struct streamState[fNumStreamStates];
		iter.reset();
		ServerMediaSubsession* subsession;
		for (unsigned i = 0; i < fNumStreamStates; ++i) {
			subsession = iter.next();
			fStreamStates[i].subsession = subsession;
			fStreamStates[i].streamToken = NULL; // for now; reset by SETUP later
		}
	}

	// Look up information for the specified subsession (track):
	ServerMediaSubsession* subsession = NULL;
	unsigned streamNum;
	if (trackId != NULL && trackId[0] != '\0') { // normal case
		for (streamNum = 0; streamNum < fNumStreamStates; ++streamNum) {
			subsession = fStreamStates[streamNum].subsession;
			if (subsession != NULL && strcmp(trackId, subsession->trackId()) == 0) break;
		}
		if (streamNum >= fNumStreamStates) {
			// The specified track id doesn't exist, so this request fails:
			Debug(ckite_log_message, "Not found %d, %d\n", streamNum, fNumStreamStates);
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
	int tcpSocketNum = streamingMode == RTP_TCP ? ( IsHttp? fHttpGetClientSocket:fClientSocket ): -1;
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
	subsession->getStreamParameters(IsHttp?fHttpGetOurSessionId:fOurSessionId, IsHttp?fHttpGetClientAddr.sin_addr.s_addr:fClientAddr.sin_addr.s_addr,
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
					IsHttp?fHttpGetOurSessionId:fOurSessionId);
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
					IsHttp?fHttpGetOurSessionId:fOurSessionId);
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
								  IsHttp?fHttpGetOurSessionId:fOurSessionId);
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
								  IsHttp?fHttpGetOurSessionId:fOurSessionId);
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
								  IsHttp?fHttpGetOurSessionId:fOurSessionId);
						  break;
					  }
		}
	}
	delete[] destAddrStr; delete[] sourceAddrStr; delete[] streamingModeString;
}

void RTSPServer::RTSPClientSession
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
	if (fOurServerMediaSession == NULL) { // There wasn't a previous SETUP!
		handleCmd_notSupported(cseq);
		return;
	}
	ServerMediaSubsession* subsession;
	if (urlSuffix[0] != '\0' &&
			strcmp(fOurServerMediaSession->streamName(), urlPreSuffix) == 0) {
		// Non-aggregated operation.
		// Look up the media subsession whose track id is "urlSuffix":
		ServerMediaSubsessionIterator iter(*fOurServerMediaSession);
		while ((subsession = iter.next()) != NULL) {
			if (strcmp(subsession->trackId(), urlSuffix) == 0) break; // success
		}
		if (subsession == NULL) { // no such track!
			handleCmd_notFound(cseq);
			return;
		}
	} else if (strcmp(fOurServerMediaSession->streamName(), urlSuffix) == 0 ||
			strcmp(fOurServerMediaSession->streamName(), urlPreSuffix) == 0) {
		// Aggregated operation
		subsession = NULL;
	} else { // the request doesn't match a known stream and/or track at all!
		handleCmd_notFound(cseq);
		return;
	}

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

void RTSPServer::RTSPClientSession
::handleCmd_TEARDOWN(ServerMediaSubsession* subsession, char const* cseq) {

	for (unsigned i = 0; i < fNumStreamStates; ++i) {
		if (subsession == NULL /* means: aggregated operation */
				|| subsession == fStreamStates[i].subsession) {
			//fStreamStates[i].subsession->closeStreamSource()
			fStreamStates[i].subsession->deleteStream(IsHttp?fHttpGetOurSessionId:fOurSessionId,
					fStreamStates[i].streamToken);
		}
	}
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%s\r\n",
			cseq, dateHeader());
	fSessionIsActive = False; // triggers deletion of ourself after responding
}

Boolean parseScaleHeader(char const* buf, float& scale) {
	// Initialize the result parameter to a default value:
	scale = 1.0;

	// First, find "Scale:"
	while (1) {
		if (*buf == '\0') return False; // not found
		if (_strncasecmp(buf, "Scale: ", 7) == 0) break;
		++buf;
	}

	// Then, run through each of the fields, looking for ones we handle:
	char const* fields = buf + 7;
	while (*fields == ' ') ++fields;
	float sc;
	if (sscanf(fields, "%f", &sc) == 1) {
		scale = sc;
	} else {
		return False; // The header is malformed
	}

	return True;
}

void RTSPServer::RTSPClientSession
::handleCmd_PLAY(ServerMediaSubsession* subsession, char const* cseq,
		char const* fullRequestStr) {
	char* rtspURL = fOurServer.rtspURL(fOurServerMediaSession, fClientSocket);
	unsigned rtspURLSize = strlen(rtspURL);

	// Parse the client's "Scale:" header, if any:
	float scale;
	Boolean sawScaleHeader = parseScaleHeader(fullRequestStr, scale);

	// Try to set the stream's scale factor to this value:
	if (subsession == NULL /*aggregate op*/) {
		fOurServerMediaSession->testScaleFactor(scale);
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
		? fOurServerMediaSession->duration() : subsession->duration();
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
				fStreamStates[i].subsession->setStreamScale(IsHttp?fHttpGetOurSessionId:fOurSessionId,
						fStreamStates[i].streamToken,
						scale);
			}
			if (sawRangeHeader) {
				fStreamStates[i].subsession->seekStream(IsHttp?fHttpGetOurSessionId:fOurSessionId,
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
			fStreamStates[i].subsession->startStream(IsHttp?fHttpGetOurSessionId:fOurSessionId,
					fStreamStates[i].streamToken,
					(TaskFunc*)noteClientLiveness, this,
					rtpSeqNum, rtpTimestamp,
					handleAlternativeRequestByte, this, NULL, 0xffffffff, False);

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
			IsHttp?fHttpGetOurSessionId:fOurSessionId,
			rtpInfo);

	send(IsHttp?fHttpGetClientSocket:fClientSocket, (char const*)fResponseBuffer, strlen((char*)fResponseBuffer), 0);
	for (i = 0; i < fNumStreamStates; ++i) {
		if (subsession == NULL /* means: aggregated operation */
				|| subsession == fStreamStates[i].subsession) {
			unsigned short rtpSeqNum = 0;
			unsigned rtpTimestamp = 0;
			fStreamStates[i].subsession->startStream(IsHttp?fHttpGetOurSessionId:fOurSessionId,
					fStreamStates[i].streamToken,
					(TaskFunc*)noteClientLiveness, this,
					rtpSeqNum, rtpTimestamp,
					handleAlternativeRequestByte, this, NULL, 0xffffffff, True);
		}
	}

	delete[] rtpInfo; delete[] rangeHeader;
	delete[] scaleHeader; delete[] rtspURL;
}

void RTSPServer::RTSPClientSession
::handleCmd_PAUSE(ServerMediaSubsession* subsession, char const* cseq) {
	for (unsigned i = 0; i < fNumStreamStates; ++i) {
		if (subsession == NULL /* means: aggregated operation */
				|| subsession == fStreamStates[i].subsession) {
			fStreamStates[i].subsession->pauseStream(IsHttp?fHttpGetOurSessionId:fOurSessionId,
					fStreamStates[i].streamToken);
		}
	}
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sSession: %08X\r\n\r\n",
			cseq, dateHeader(), 
			IsHttp?fHttpGetOurSessionId:fOurSessionId);
}

void RTSPServer::RTSPClientSession
::handleCmd_GET_PARAMETER(ServerMediaSubsession* subsession, char const* cseq,
		char const* /*fullRequestStr*/) {
	// We implement "GET_PARAMETER" just as a 'keep alive',
	// and send back an empty response:
	snprintf((char*)fResponseBuffer, sizeof fResponseBuffer,
			"RTSP/1.0 200 OK\r\nCSeq: %s\r\n%sSession: %08X\r\n\r\n",
			cseq, dateHeader(), IsHttp?fHttpGetOurSessionId:fOurSessionId);
}

void RTSPServer::RTSPClientSession
::handleCmd_SET_PARAMETER(ServerMediaSubsession* /*subsession*/, char const* cseq,
		char const* /*fullRequestStr*/) {
	// By default, we don't implement "SET_PARAMETER":
	handleCmd_notSupported(cseq);
}


void RTSPServer::RTSPClientSession::noteLiveness() {
	Debug(ckite_log_message, "RTSPServer::RTSPClientSession::noteLiveness\n");
	if (fOurServer.fReclamationTestSeconds > 0) {
		envir().taskScheduler()
			.rescheduleDelayedTask(fLivenessCheckTask,
					fOurServer.fReclamationTestSeconds*1000000,
					(TaskFunc*)livenessTimeoutTask, this);
	}
}

void RTSPServer::RTSPClientSession
::noteClientLiveness(RTSPClientSession* clientSession) {
	Debug(ckite_log_message, "noteClientLiveness\n");
	clientSession->noteLiveness();
}

void RTSPServer::RTSPClientSession
::livenessTimeoutTask(RTSPClientSession* clientSession) {
	// If this gets called, the client session is assumed to have timed out,
	// so delete it:
#ifdef DEBUG
	fprintf(stderr, "RTSP client session from %s has timed out (due to inactivity)\n", our_inet_ntoa(clientSession->fClientAddr.sin_addr));
#endif
	delete clientSession;
}

RTSPServer::RTSPClientSession*
RTSPServer::createNewClientSession(unsigned sessionId, int clientSocket, struct sockaddr_in clientAddr) {
	return new RTSPClientSession(*this, sessionId, clientSocket, clientAddr);
}


////////// ServerMediaSessionIterator implementation //////////

RTSPServer::ServerMediaSessionIterator
::ServerMediaSessionIterator(RTSPServer& server)
: fOurIterator((server.fServerMediaSessions == NULL)
		? NULL : HashTable::Iterator::create(*server.fServerMediaSessions)) {
}

RTSPServer::ServerMediaSessionIterator::~ServerMediaSessionIterator() {
	delete fOurIterator;
}

ServerMediaSession* RTSPServer::ServerMediaSessionIterator::next() {
	if (fOurIterator == NULL) return NULL;

	char const* key; // dummy
	return (ServerMediaSession*)(fOurIterator->next(key));
}
