#ifndef ENCODER_SOURCE_H_
#define ENCODER_SOURCE_H_

#include "FramedSource.h"
#include "AMRAudioSource.h"
#ifdef SDKH264
#include "x264.h"
#endif

class TimeCode {
public:
	TimeCode();
	virtual ~TimeCode();
	
	int operator==(TimeCode const& arg2);
	unsigned days, hours, minutes, seconds, pictures;
};

class EncoderVideoSource: public FramedSource
{
public:
	enum VideoCodec { VIDEO_RAW, VIDEO_MPEG4, VIDEO_H264 };

	static EncoderVideoSource* createNew(UsageEnvironment& env, unsigned int width,
		unsigned int height, unsigned int framerate, unsigned int bitrate, unsigned int keyinterval, char *type);
	void setPlayTime(unsigned int time) { playTime = time; }
	void setPauseTime(unsigned int time) { pauseTime = time; }
	void setPlayFile(FILE *videofp) { fp = videofp;}
	void setChannelNumber(unsigned char channel) {fChannel = channel;}

	Boolean& pictureEndMarker() { return fPictureEndMarker; }
	void reset();
	u_int8_t profile_and_level_indication() const {
		return fProfileAndLevelIndication;
	}
	unsigned char* getConfigBytes(unsigned& numBytes) const;
	Boolean currentNALUnitEndsAccessUnit() {return True;}
protected:
	EncoderVideoSource(UsageEnvironment& env, unsigned int width, unsigned int height, unsigned int framerate, unsigned int bitrate, unsigned int keyinterval, VideoCodec codec,char *type);
	virtual ~EncoderVideoSource();
private: // redefined virtual functions
	virtual void doGetNextFrame();
	virtual Boolean isMPEG4VideoStreamFramer() const;
	virtual Boolean isH264VideoStreamFramer() const;
	void MPEG4_doGetNextFrame();
	void H264_doGetNextFrame();
	int computePresentationTime();
	int getLivehdFrame(void);
private:
	FILE* fp;
#if SDKH264
	x264_t *x264_handle;
	x264_param_t m_param;
	x264_picture_t m_pic;
#endif
	unsigned char fChannel;
	unsigned int fWidth;
	unsigned int fHeight;
	unsigned int playTime;
	unsigned int pauseTime;
	unsigned char* fBuffer;
	unsigned int fBitrate;	// in kbps
	unsigned int fFramerate;
	unsigned int fKeyInterval;
	VideoCodec	 fCodec;
	void*		 fEncoderHandle;
private:
	unsigned int fStartTime;
	unsigned int fCurrTime;
	Boolean fPictureEndMarker;
	unsigned fPictureCount;
	u_int8_t fProfileAndLevelIndication;
	unsigned char* fConfigBytes;
	unsigned fNumConfigBytes;
	char mediaType[10];
	char *more_nal[4];
	int more_nal_len[4];
};

class EncoderAudioSource: public AMRAudioSource
{
public:
	enum AudioCodec { AUDIO_RAW, AUDIO_AMRNB, AUDIO_AMRWB };
	static EncoderAudioSource* createNew(UsageEnvironment& env, unsigned int samplerate, unsigned int framelength,
		unsigned int bitrate, char *type);
protected:
	EncoderAudioSource(UsageEnvironment& env, unsigned int samplerate, unsigned int framelength,
		unsigned int bitrate, AudioCodec codec,char *type);
	virtual ~EncoderAudioSource();
private: // redefined virtual functions
	virtual void doGetNextFrame();
	virtual Boolean isAMRAudioSource() const;
	int computeAudioPresentationTime();


private:
	FILE* fp;
	unsigned char* fBuffer;
	unsigned int fSampleRate;
	unsigned int fFrameLength;	// in samples
	unsigned int fBitrate;		// in kbps
	AudioCodec	 fCodec;
	int *fAudioCodec;
	unsigned int fStartTime;
	unsigned int fCurrTime;
	char mediaType[10];
};
#endif
