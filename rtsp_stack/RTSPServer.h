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
// C++ header

#ifndef _RTSP_SERVER_HH
#define _RTSP_SERVER_HH

#ifndef _SERVER_MEDIA_SESSION_HH
#include "ServerMediaSession.h"
#endif
#ifndef _NET_ADDRESS_HH
#include "NetAddress.h"
#endif

#define RTSP_BUFFER_SIZE	10000 // for incoming requests, and outgoing responses
#define RTSP_MSG_SIZE		4096
#define HEARTBEAT_PERIOD	30000000L	// in microseconds
#define LAST_MODIFY_TIME    "20120422"

class RTSPServer: public Medium {
public:
	static RTSPServer* createNew(UsageEnvironment& env, Port ourPort = 554,
								 char* authDatabase = NULL,
								 unsigned reclamationTestSeconds = 65);
	// If ourPort.num() == 0, we'll choose the port number
	// Note: The caller is responsible for reclaiming "authDatabase"
	// If "reclamationTestSeconds" > 0, then the "RTSPClientSession" state for
	//     each client will get reclaimed (and the corresponding RTP stream(s)
	//     torn down) if no RTSP commands - or RTCP "RR" packets - from the
	//     client are received in at least "reclamationTestSeconds" seconds.

	static Boolean lookupByName(UsageEnvironment& env, char const* name,
								RTSPServer*& resultServer);

	void addServerMediaSession(ServerMediaSession* serverMediaSession);
	virtual ServerMediaSession* lookupServerMediaSession(char const* streamName);
	void removeServerMediaSession(ServerMediaSession* serverMediaSession);
	void removeServerMediaSession(char const* streamName);
	void getServerSocket(int &sock) {sock = fServerSocket;}

	char* rtspURL(ServerMediaSession const* serverMediaSession, int clientSocket = -1) const;
	// returns a "rtsp://" URL that could be used to access the
	// specified session (which must already have been added to
	// us using "addServerMediaSession()".
	// This string is dynamically allocated; caller should delete[]
	// (If "clientSocket" is non-negative, then it is used (by calling "getsockname()") to determine
	//  the IP address to be used in the URL.)
	char* rtspURLPrefix(int clientSocket = -1) const;
	// like "rtspURL()", except that it returns just the common prefix used by
	// each session's "rtsp://" URL.
	// This string is dynamically allocated; caller should delete[]

protected:
	RTSPServer(UsageEnvironment& env,
			   int ourSocket, Port ourPort,
			   char* authDatabase,
			   unsigned reclamationTestSeconds);
	// called only by createNew();
	virtual ~RTSPServer();

	static int setUpOurSocket(UsageEnvironment& env, Port& ourPort);
	virtual Boolean specialClientAccessCheck(int clientSocket, struct sockaddr_in& clientAddr,
											 char const* urlSuffix);
	// a hook that allows subclassed servers to do server-specific access checking
	// on each client (e.g., based on client IP address), without using
	// digest authentication.

private: // redefined virtual functions
	virtual Boolean isRTSPServer() const;

public:
	// The state of each individual session handled by a RTSP server:
	class RTSPClientSession {
	public:
		RTSPClientSession(RTSPServer& ourServer, unsigned sessionId,
						  int clientSocket, struct sockaddr_in clientAddr);
		virtual ~RTSPClientSession();
		void noteLiveness();
		void closeHttpSocketAndFreeResource();
	protected:
		// Make the handler functions for each command virtual, to allow subclasses to redefine them:
		virtual void handleCmd_bad(char const* cseq);
		virtual void handleCmd_notSupported(char const* cseq);
		virtual void handleCmd_notFound(char const* cseq);
		virtual void handleCmd_unsupportedTransport(char const* cseq);
		virtual void handleCmd_OPTIONS(char const* cseq);
		virtual void handleCmd_DESCRIBE(char const* cseq, char const* urlSuffix,
										char const* fullRequestStr);
		virtual void handleCmd_SETUP(char const* cseq,
									 char const* urlPreSuffix, char const* urlSuffix,
									 char const* fullRequestStr);
		virtual void handleCmd_withinSession(char const* cmdName,
											 char const* urlPreSuffix, char const* urlSuffix,
											 char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_TEARDOWN(ServerMediaSubsession* subsession,
										char const* cseq);
		virtual void handleCmd_PLAY(ServerMediaSubsession* subsession,
									char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_PAUSE(ServerMediaSubsession* subsession,
									 char const* cseq);
		virtual void handleCmd_GET_PARAMETER(ServerMediaSubsession* subsession,
											 char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_SET_PARAMETER(ServerMediaSubsession* subsession,
											 char const* cseq, char const* fullRequestStr);
	protected:
		UsageEnvironment& envir() { return fOurServer.envir(); }
		void reclaimStreamStates();
		void resetRequestBuffer();
		Boolean authenticationOK(char const* cmdName, char const* cseq,
								 char const* urlSuffix,
								 char const* fullRequestStr);
		Boolean isMulticast() const { return fIsMulticast; }
		static void incomingRequestHandler(void*, int /*mask*/);
		virtual void incomingRequestHandler1();
		static void incomingHttpRequestHandler(void*, int /*mask*/);
		virtual void incomingHttpRequestHandler1();
		static void incomingHttpGetRequestHandler(void*, int /*mask*/);
		virtual void incomingHttpGetRequestHandler1();
		static void handleAlternativeRequestByte(void*, u_int8_t requestByte);
		virtual void handleAlternativeRequestByte1(u_int8_t requestByte);
		virtual void handleRequestBytes(int newBytesRead);
		static void noteClientLiveness(RTSPClientSession* clientSession);
		static void livenessTimeoutTask(RTSPClientSession* clientSession);
	protected:
		RTSPServer& fOurServer;
		unsigned fOurSessionId;
		ServerMediaSession* fOurServerMediaSession;
		int fClientSocket;
		struct sockaddr_in fClientAddr;

		unsigned fHttpGetOurSessionId;
		int fHttpGetClientSocket;
		struct sockaddr_in fHttpGetClientAddr;

		unsigned fHttpPostOurSessionId;
		int fHttpPostClientSocket;
		struct sockaddr_in fHttpPostClientAddr;
		int IsHttp;
		char httpSessionCookie[60];
		unsigned char fHttpRequestMsg[RTSP_MSG_SIZE];

		TaskToken fLivenessCheckTask;
		unsigned char fRequestBuffer[RTSP_BUFFER_SIZE];
		unsigned char fRequestMsg[RTSP_MSG_SIZE];
		unsigned fRequestBytesAlreadySeen, fRequestBufferBytesLeft;
		unsigned char* fLastCRLF;
		unsigned char fResponseBuffer[RTSP_BUFFER_SIZE];
		Boolean fIsMulticast, fSessionIsActive, fStreamAfterSETUP;
		unsigned char fTCPStreamIdCount; // used for (optional) RTP/TCP
		unsigned fNumStreamStates;
		struct streamState {
			ServerMediaSubsession* subsession;
			void* streamToken;
		} * fStreamStates;
	};

	class RTSPEncoderSession: public RTSPClientSession
	{
	public:
		enum PtzAction
			{
				LEFTSTOP, RIGHTSTOP, UPSTOP, DOWNSTOP,
				AUTOSTOP, FOCUSFAR, FOCUSNEAR, ZOOMTELESTOP,
				ZOOMWIDESTOP, IRISOPENSTOP, IRISCLOSESTOP,
				WIPERON, WIPEROFF, LIGHTON, LIGHTOFF, 
				LEFT, RIGHT, UP, DOWN, AUTO, STOP 
			};
		enum Scene {DISARM, ZONEARM, ACTIVEALARM, STOPALARM, ARM};

		RTSPEncoderSession(RTSPServer& ourServer, ServerMediaSession* session, unsigned sessionId, struct sockaddr_in addr,
						   char* local_ip, unsigned short local_port, char* path, char* macaddr, char* sn, char* ua, unsigned int cam_id);
		virtual ~RTSPEncoderSession();

		int register_Platform(struct sockaddr_in* server_addr, bool do_send = true);

		int do_heartbeat(bool with_sdp = false, bool with_challenge_resp = false, char* challenge_resp = NULL);
		void reNoteLiveness(void);
		unsigned int getMagicValue(void) {return fMagic;}

		static void heartbeat(void* arg);
	protected:
		// Make the handler functions for each command virtual, to allow subclasses to redefine them:
		virtual void handleCmd_DESCRIBE(char const* cseq, char const* urlSuffix,
										char const* fullRequestStr);
		virtual void handleCmd_SETUP(char const* cseq,
									 char const* urlPreSuffix, char const* urlSuffix,
									 char const* fullRequestStr);
		virtual void handleCmd_withinSession(char const* cmdName,
											 char const* urlPreSuffix, char const* urlSuffix,
											 char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_TEARDOWN(ServerMediaSubsession* subsession,
										char const* cseq);
		virtual void handleCmd_PLAY(ServerMediaSubsession* subsession,
									char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_PAUSE(ServerMediaSubsession* subsession,
									 char const* cseq);
		virtual void handleCmd_GET_PARAMETER(ServerMediaSubsession* subsession,
											 char const* cseq, char const* fullRequestStr);
		virtual void handleCmd_SET_PARAMETER(ServerMediaSubsession* subsession,
											 char const* cseq, char const* fullRequestStr);
		//start by wayde
     	void deviceReset(void);
		void deviceAlarm(char *session_name, int alarm_code);
		virtual void handleCmd_SET_PARAMETER_ResponsePtz(ServerMediaSubsession* subsession/*subsession*/, char const* cseq,
														 char const* fullRequestStr);
		virtual void handleCmd_SET_PARAMETER_ResponseScene(ServerMediaSubsession* subsession/*subsession*/, char const* cseq,
														   char const* fullRequestStr);
		virtual void handleCmd_GET_PARAMETER_SubDirList(ServerMediaSubsession* subsession, char const* cseq,
														char const* fullRequestStr);
		virtual void handleCmd_GET_PARAMETER_SubFileList(ServerMediaSubsession* subsession, char const* cseq,
														 char const* fullRequestStr);
		//scene start
	    void deviceHandleArmAndDisarmScene( int actionType);	
		// palyback
        void deviceReponseVideoPlayBack(char *fileName);

		//ptz start
		void  deviceHandlePtz(int actionType, int speed);

		//support web config
		void get_LanWebURL(char *p, int p_len);
		void get_WanWebURL(char *p, int p_len);
		void get_P2PLanURL(char *p, int p_len);
		void get_P2PWanURL(char *p, int p_len);
		void get_LanFtpURL(char *p, int p_len);
		void get_WanFtpURL(char *p, int p_len);
		void get_LanVoiceAddr(char *p, int p_len);
		void get_WanVoiceAddr(char *p, int p_len);
		void get_LanDataAddr(char *p, int p_len);
		void get_WanDataAddr(char *p, int p_len);

 		//see other server
 		virtual void handleCmd_SeeOtherServer(char const* fullRequestStr);
		void handleCmd_Unauthorized(void);
		void handleCmd_Badrequest(void);
		void  writeActiveServerInfo(char const *host, unsigned short port);
		// write serial into flash
		void writeSerial(char const *serial);
		void readSerial(char *serial);

		//end by wayde 
	protected:
		//	  virtual void incomingRequestHandler1();
		//	  virtual void handleAlternativeRequestByte1(u_int8_t requestByte);
		void noteLiveness();
		static void livenessTimeoutTask(RTSPEncoderSession* clientSession);
		virtual void handleRequestBytes(int newBytesRead);
		char* GetRandom();
	protected:
		char					fSerial[20];
		char					fMac[20];
		char					fKey[128];
		char					fPath[20];
		char					fRandom[20];
		char					fLocalIP[20];
		char					fUserAgent[128];
	    unsigned int 			fMagic;
		unsigned short		    fLocalPort;
		unsigned char			fCamId;
		unsigned int			fCSeq;
		unsigned int			fSetupNum;
		FILE*					videofp;
		char 					videoFile[60];
		int 					videoActive;
		int sessionState ;
		ServerMediaSession*	fSession;
	};

	// If you subclass "RTSPClientSession", then you should also redefine this virtual function in order
	// to create new objects of your subclass:
	virtual RTSPClientSession*
		createNewClientSession(unsigned sessionId, int clientSocket, struct sockaddr_in clientAddr);

	virtual RTSPEncoderSession*
		createNewEncoderSession(ServerMediaSession* session, unsigned sessionId, struct sockaddr_in clientAddr,
								char* local_ip, unsigned short local_port, char* path, char* macaddr, char* sn, char* ua,
								unsigned int cam_id);

	// An iterator over our "ServerMediaSession" objects:
	class ServerMediaSessionIterator {
	public:
		ServerMediaSessionIterator(RTSPServer& server);
		virtual ~ServerMediaSessionIterator();
		ServerMediaSession* next();
	private:
		HashTable::Iterator *fOurIterator;
		ServerMediaSession* fNextPtr;
	};

private:
	static void incomingConnectionHandler(void*, int /*mask*/);
	void incomingConnectionHandler1();

private:
	friend class RTSPClientSession;
	friend class RTSPEncoderSession;
	friend class ServerMediaSessionIterator;
	int fServerSocket;
	Port fServerPort;
	//UserAuthenticationDatabase* fAuthDB;
	unsigned fReclamationTestSeconds;
	HashTable* fServerMediaSessions;
};

// Global function declarations:
char const* dateHeader();
Boolean parseScaleHeader(char const* buf, float& scale);

#endif
