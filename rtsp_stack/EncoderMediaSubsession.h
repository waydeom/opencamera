#ifndef ENCODER_MEDIA_SUBSESSION_H
#define ENCODER_MEDIA_SUBSESSION_H

class EncoderMediaSubsession: public ServerMediaSubsession {
public:
  static EncoderMediaSubsession* createNew(UsageEnvironment& env, void* owner, Boolean isAudio, Boolean reuseFirstSource,
  				portNumBits initialPortNum = 6970);
  void SetVideoParameters(unsigned char channel, int width, int height, int bitrate, int framerate, int keyinterval, char *type);
  void SetAudioParameters(int bitrate, int framelength, int samplerate, char *type);
protected: // we're a virtual base class
  EncoderMediaSubsession(UsageEnvironment& env, void* owner, Boolean isAudio, Boolean reuseFirstSource,
				portNumBits initialPortNum = 6970);
  virtual ~EncoderMediaSubsession();

protected: // redefined virtual functions
  virtual char const* sdpLines();
  virtual void getStreamParameters(unsigned clientSessionId,
				   netAddressBits clientAddress,
                                   Port const& clientRTPPort,
                                   Port const& clientRTCPPort,
				   int tcpSocketNum,
                                   unsigned char rtpChannelId,
                                   unsigned char rtcpChannelId,
                                   netAddressBits& destinationAddress,
				   u_int8_t& destinationTTL,
                                   Boolean& isMulticast,
                                   Port& serverRTPPort,
                                   Port& serverRTCPPort,
                                   void*& streamToken);
  virtual void startStream(unsigned clientSessionId, void* streamToken,
			   TaskFunc* rtcpRRHandler,
			   void* rtcpRRHandlerClientData,
			   unsigned short& rtpSeqNum,
			   unsigned& rtpTimestamp,
			   ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
               void* serverRequestAlternativeByteHandlerClientData,
			   void* videofp,
			   unsigned int fMagic,
			   Boolean isResponse);
  virtual void pauseStream(unsigned clientSessionId, void* streamToken);
  virtual void seekStream(unsigned clientSessionId, void* streamToken, double seekNPT);
  virtual void setStreamScale(unsigned clientSessionId, void* streamToken, float scale);
  virtual void deleteStream(unsigned clientSessionId, void*& streamToken);

protected: // new virtual functions, possibly redefined by subclasses
  virtual char const* getAuxSDPLine(RTPSink* rtpSink,
				    FramedSource* inputSource);
  virtual void seekStreamSource(FramedSource* inputSource, double seekNPT);
  virtual void setStreamSourceScale(FramedSource* inputSource, float scale);
  virtual void closeStreamSource(FramedSource *inputSource);

protected: // new virtual functions, defined by all subclasses
  virtual FramedSource* createNewVideoSource(unsigned clientSessionId,
					      unsigned& estBitrate);
  virtual FramedSource* createNewAudioSource(unsigned clientSessionId,
	                      unsigned& estBitrate);
      // "estBitrate" is the stream's estimated bitrate, in kbps
  virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
				    unsigned char rtpPayloadTypeIfDynamic,
				    FramedSource* inputSource);

private:
  void setSDPLinesFromRTPSink(RTPSink* rtpSink, FramedSource* inputSource, unsigned estBitrate);
      // used to implement "sdpLines()"

private:
  FramedSource*	fMediaSource;
  Boolean fIsAudio;
  Boolean fReuseFirstSource;
  portNumBits fInitialPortNum;
  HashTable* fDestinationsHashTable; // indexed by client session id
  void* fLastStreamToken;
  char* fSDPLines;
  char fCNAME[100]; // for RTCP
  void* fOwner;
  friend class StreamState;

  // Video or audio parameters
  unsigned char fChannel;
  int		fWidth;
  int		fHeight;
  int		fVideoBitrate;
  int		fFramerate;
  int		fKeyInterval;
  int		fAudioBitrate;
  int		fFrameLength;
  int		fSamplerate;
  char      vType[10];
  char      aType[10];
};


#endif
