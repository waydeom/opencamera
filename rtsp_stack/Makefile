ENCSOURCE = undefined
H264 = defined
MPEG4 = undefined
AMRNB = undefined

#FLAGS += -DSTARV_TEST 
#FLAGS += -DENC_SOURCE

ifeq (defined, $(H264))
FLAGS += -DSDKH264
OBJECTS += H264VideoRTPSink.o  
ifeq (defined, $(ENCSOURCE))
LIBS += -lx264
endif
endif

ifeq (defined, $(MPEG4)) 
FLAGS += -DSDKMPEG4
OBJECTS += xvid_interface.o
ifeq (defined, $(ENCSOURCE))
LIBS += -lxvidcore
endif
endif

ifeq (defined, $(AMRNB)) 
FLAGS += -DSDKAMR
ifeq (defined, $(ENCSOURCE))
LIBS += -lopencore-amrnb
endif
endif

LIBS += -lpthread -lssl
CC = g++
FLAGS += -g
OBJECTS += AMRAudioRTPSink.o AMRAudioSource.o AudioRTPSink.o  \
		  BasicHashTable.o \
		  BasicTaskScheduler.o BasicTaskScheduler0.o BasicUDPSink.o	BasicUsageEnvironment.o	\
		  BasicUsageEnvironment0.o DelayQueue.o \
		  EncoderMediaSubsession.o EncoderSource.o FramedFilter.o	\
		  FramedSource.o GroupEId.o	Groupsock.o	GroupsockHelper.o HashTable.o \
		  inet.o IOHandlers.o Locale.o Media.o MediaSink.o	\
		  MediaSource.o MPEG4ESVideoRTPSink.o	\
	      MultiFramedRTPSink.o NetAddress.o	NetInterface.o	\
		  rsa_crypto.o RTCP.o rtcp_from_spec.o RTPInterface.o	\
		  RTPSink.o	RTPSource.o	RTSPCommon.o RTSPEncoderSession.o \
		  RTSPServer.o ServerMediaSession.o strDup.o mainSdk.o	\
		  UsageEnvironment.o VideoRTPSink.o	RTSPSdk.o	Base64.o \

rtsp_protocol: $(OBJECTS)
	$(CC) -o $@  $^ $(LIBS) 
.cpp.o:
	$(CC) -c $(FLAGS) $<
.PHONY clean:
clean: 
	-rm rtsp_protocol $(OBJECTS) 
