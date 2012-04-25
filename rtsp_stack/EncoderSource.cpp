#include "EncoderSource.h"

#ifdef ENC_SOURCE
#ifndef __WIN32__
#include "../../../opencore-amr-0.1.2/amrnb/interf_enc.h"
#include "../../../opencore-amr-0.1.2/amrnb/interf_dec.h"
#else
#include "../include/interf_enc.h"
#include "../include/interf_dec.h"
#endif
#endif


#ifdef SDKH264
#ifdef ENC_SOURCE
#include "x264.h"
#endif
#endif


#ifdef SDKMPEG4
#ifdef ENC_SOURCE
#include "xvid_interface.h"
#endif
#endif

#ifndef __WIN32__
#include <sys/time.h>
#endif

#include "GroupsockHelper.h"
#include "RTSPSdk.h"
#include "Debug.h"

#ifdef STARV_TEST
#define VIDEO_FILE		"../media/tempete.cif"
#define VIDEO_CIF_FILE		"../media/tempete.cif"
#define VIDEO_QCIF_FILE		"../media/carph.qcf"
#define AUDIO_FILE		"../media/qlx.pcm"
#endif

EncoderVideoSource* EncoderVideoSource::createNew(UsageEnvironment& env, unsigned int width, 
		unsigned int height, unsigned int framerate, 
		unsigned int bitrate, unsigned int keyinterval, char *type)
{
	Debug(ckite_log_message, "EncoderVideoSource::createNew width = %d, height = %d\n", width, height);
#ifdef SDKH264
	return new EncoderVideoSource(env, width, height, framerate, bitrate, keyinterval, VIDEO_H264, type);
#elif SDKMPEG4
	return new EncoderVideoSource(env, width, height, framerate, bitrate, keyinterval, VIDEO_MPEG4, type);
#endif
}

EncoderVideoSource::EncoderVideoSource(UsageEnvironment& env, unsigned int width, unsigned int height, unsigned int framerate,
		unsigned int bitrate, unsigned int keyinterval, VideoCodec codec, char *type):
	FramedSource(env), fp(NULL), fWidth(width), fHeight(height), fFramerate(framerate), fBitrate(bitrate), fKeyInterval(keyinterval),
	fCodec(codec), fConfigBytes(NULL), fStartTime(0), fEncoderHandle(NULL)
{
	memset(mediaType, 0, sizeof(mediaType));
	memcpy(mediaType, type, strlen(type));
#ifdef STARV_TEST
	if(strcmp(type, "live") == 0 || strcmp(type, "livehd") == 0)
		fp = fopen(VIDEO_FILE, "rb");
	if(strcmp(type, "mobile") == 0)
		fp = fopen(VIDEO_FILE, "rb");
#endif
	Debug(ckite_log_message, "type = %s\n", type);
	if(strcmp(type, "store") == 0)
	{
		Debug(ckite_log_message, "EncoderVideoSource store \n");
	}
	fBuffer = new unsigned char[fWidth*fHeight*3/2];

#ifdef SDKH264
#ifdef ENC_SOURCE
	x264_param_default(&m_param);
	x264_param_apply_profile(&m_param, "baseline");
	m_param.i_width = fWidth;
	m_param.i_height = fHeight;
	m_param.i_fps_num = 10; //
	m_param.i_fps_den = 1000;
	m_param.i_frame_reference = 1;
	//m_param.i_maxframes = 0;  // no find this parameter
	m_param.i_keyint_max = 250;
	m_param.i_bframe = 0;
	m_param.rc.i_bitrate = 1000; // the unit is kbps
	//m_param.rc.b_cbr = 0;
	m_param.rc.f_qcompress = 0;
	//m_param.rc.b_stat_write = 0;
	//m_param.analyse.inter = 0;
	m_param.analyse.b_psnr = 0;
	m_param.b_cabac = 0;
	m_param.rc.b_mb_tree = 0;
	//m_param.pf_log = NULL //if set NULL , it will segment fault
	x264_handle = x264_encoder_open(&m_param);
	//memset(p_nal, 0x0, sizeof (struct nal));
	fprintf(stderr, "x264_encoder_open x264_handle:%x\n", x264_handle);
#endif
	for(int i = 0; i < 4; i++)
	{
		more_nal[i] = NULL;
		more_nal_len[i] = 0;
	}
#endif

#ifdef SDKMPEG4
#ifdef ENC_SOURCE
	Debug(ckite_log_message, "xvid fWidth = %d, fHeight = %d\n", fWidth, fHeight);
	fEncoderHandle = xvid_enc_init(fWidth, fHeight, fBitrate, fFramerate, fKeyInterval, 1);
#endif
#endif
}

EncoderVideoSource::~EncoderVideoSource()
{
	Debug(ckite_log_message, "EncoderVideoSource::~EncoderVideoSource.\n");
#ifdef STARV_TEST
	if (fp)
		fclose(fp);
#endif

	if (fBuffer)
		delete [] fBuffer;
	if (fConfigBytes)
		delete fConfigBytes; 
#ifdef SDKH264 
#ifdef ENC_SOURCE
		x264_encoder_close(x264_handle);
#endif
#endif

#ifdef SDKMPEG4
#ifdef ENC_SOURCE
	if (fEncoderHandle)
		xvid_release(fEncoderHandle);
#endif
#endif
}

Boolean EncoderVideoSource::isMPEG4VideoStreamFramer() const 
{
	return True;
}
Boolean EncoderVideoSource::isH264VideoStreamFramer() const 
{
	return True;
}

void EncoderVideoSource::doGetNextFrame()
{
	// Copy an encoded video frame into buffer `fTo'
	if (fCodec == VIDEO_MPEG4)
		MPEG4_doGetNextFrame();
	else if (fCodec == VIDEO_H264)
		H264_doGetNextFrame();
	else
	{

	}
}

void EncoderVideoSource::MPEG4_doGetNextFrame()
{
	// Read a new frame into fBuffer, YUV420 format is assumed
	int size = (fWidth*fHeight*3/2);
	int videoType;
	unsigned int intervalTime = 0;

	Debug(ckite_log_message, "fMaxSize = %d\n", fMaxSize);
	intervalTime = computePresentationTime();

	if (strcmp(mediaType, "store") == 0)
	{
		if (fWidth == 720 && fHeight == 576)
		{
			videoType = getLivehdFrame();
		}
		else
		{
		  getStoreVideoFrame( fp, 0, (unsigned char *)fBuffer, &size, &videoType, false);
		}
	}
	else
	{
		if (fWidth == 720 && fHeight == 576)
		{
			videoType = getLivehdFrame();
		}
		else
		{  
			Debug(ckite_log_message, "cif video is ready. size is of = %d\n", size);
			getRealVideoFrame(fChannel, fp, mediaType, fBuffer, &size, &videoType);
		}
	}
	if(size <= 0)
		 size = 0;

	// Encode the frame
	int ret = 0;
	
#if ENC_SOUCE
	if (fEncoderHandle != NULL)
	{
			Debug(ckite_log_message, "xvid_encode_frame fWidth = %d, fHeight = %d\n", fWidth, fHeight);
			ret = xvid_encode_frame(fEncoderHandle, fBuffer, fWidth, fHeight, fTo, intervalTime);
			if (ret > 0)
			{
				Debug(ckite_log_message, "Frame length %d, header %02x, %02x, %02x, %02x\n", ret, fTo[0], fTo[1], fTo[2], fTo[3]);
				fFrameSize = ret;
			}
	}
#else
	if (videoType == VIDEO_MPEG4 || videoType == VIDEO_H264)
	{
		if (size != 0)
			memcpy(fTo, fBuffer, size);
		fFrameSize = size;
	}
#endif
	fPictureEndMarker = True;
	afterGetting(this);
}

int EncoderVideoSource::getLivehdFrame(void)
{

	int videoType = 0;
	static int tWidth = 352, tHeight = 288;
#if 1
	unsigned char *tmpBuf = new unsigned char[tWidth * tHeight *3 / 2];
	unsigned char *pic_Y = new unsigned char [tWidth * tHeight];
	unsigned char *pic_U = new unsigned char [tWidth * tHeight / 4];
	unsigned char *pic_V = new unsigned char [tWidth * tHeight / 4];
#endif
	int size =  (tWidth*tHeight*3/2);
	if (strcmp(mediaType, "store") == 0)
	{
		getStoreVideoFrame( fp, 0, (unsigned char *)tmpBuf, &size, &videoType, false);
	}
	else 
	{
		getRealVideoFrame(fChannel, fp, mediaType, tmpBuf, &size, &videoType);
	}

	size = (fWidth*fHeight *3/2);
	memcpy(pic_Y, tmpBuf, tWidth * tHeight);
	memcpy(pic_U, tmpBuf + tWidth * tHeight, tWidth * tHeight / 4);
	memcpy(pic_V, tmpBuf + tWidth * tHeight * 5 / 4, tWidth * tHeight / 4);

	memset(fBuffer, 0, fWidth*fHeight*3/2);

	for (int i=0; i<fHeight; i++)    
	{
		memcpy(fBuffer+i*fWidth, pic_Y+(i%tHeight)*tWidth, tWidth);
		memcpy(fBuffer+i*fWidth+tWidth, pic_Y+(i%tHeight)*tWidth, tWidth);
		memset(fBuffer+i*fWidth+2*tWidth, 0, (fWidth-2*tWidth));

		if (!(i&1)) // if even
		{
			memcpy(fBuffer+fWidth*fHeight+(i*fWidth)/4, pic_U+(i%tHeight)*tWidth/4, tWidth/2);
			memcpy(fBuffer+fWidth*fHeight+(i*fWidth)/4+tWidth/2, pic_U+(i%tHeight)*tWidth/4, tWidth/2);
			memset(fBuffer+fWidth*fHeight+(i*fWidth)/4+tWidth, 0, fWidth/2-tWidth);
			memcpy(fBuffer+fWidth*fHeight*5/4+(i*fWidth)/4, pic_V+(i%tHeight)*tWidth/4, tWidth/2);
			memcpy(fBuffer+fWidth*fHeight*5/4+(i*fWidth)/4+tWidth/2, pic_V+(i%tHeight)*tWidth/4, tWidth/2);
			memset(fBuffer+fWidth*fHeight*5/4+(i*fWidth)/4+tWidth, 0, fWidth/2-tWidth);
		}
	}
#if 0 
	FILE *stream_fp;
	char name[128];
	static int cnt = 0;
	sprintf(name, "stream%d.bin", cnt++);
	stream_fp = fopen(name, "wb+");
	fwrite(fBuffer, 1, fWidth * fHeight * 3 / 2, stream_fp);
	fclose(stream_fp);
#endif
#if 1 
	delete [] tmpBuf; tmpBuf = NULL;
	delete [] pic_Y;  pic_Y  = NULL;
	delete [] pic_U;  pic_U  = NULL;
	delete [] pic_V;  pic_V  = NULL;
#endif
	return videoType;
}

void EncoderVideoSource::H264_doGetNextFrame()
{
	int size = (fWidth*fHeight *3/2);
	int videoType;
	unsigned char nal_type;

	Debug(ckite_log_message, "EncoderVideoSource::H264_doGetNextFrame ENTRY\n");
	Debug(ckite_log_message, "fMaxSize = %d\n", fMaxSize);

	computePresentationTime();

#ifdef ENC_SOURCE
// handle per of nal
	for(int i = 0; i < 4; i++)
	{
		if(more_nal[i] != NULL)
		{
			Debug(ckite_log_message, "more_nal address %p\n", more_nal[i]);
			Debug(ckite_log_message, "more_nal len  %d\n", more_nal_len[i]);
			memcpy(fTo, more_nal[i], more_nal_len[i]);
			fFrameSize = more_nal_len[i];
			if(more_nal[i] != NULL)
			{
				delete [] more_nal[i];
				more_nal[i] = NULL;
				more_nal_len[i] = 0;
			}
			fPictureEndMarker = True;
			afterGetting(this);
			return ;
		}
	}
#endif

	if (strcmp(mediaType, "store") == 0)	
	{
		if (fWidth == 720 && fHeight == 576)
		{
			videoType = getLivehdFrame();
		}
		else
		{
			getStoreVideoFrame( fp, 0, (unsigned char *)fBuffer, &size, &videoType, false);
		}
	}
	else
	{
		if (fWidth == 720 && fHeight == 576)
		{
			videoType = getLivehdFrame();
		}
		else
		{
			getRealVideoFrame(fChannel, fp, mediaType, fBuffer, &size, &videoType);
		}
	}
	if(size <= 0)
		size = 0;
		
#ifdef ENC_SOURCE
		if( x264_picture_alloc(&m_pic, m_param.i_csp, m_param.i_width, m_param.i_height) < 0)
		{
			Debug(ckite_log_message, "x264_picture_alloc is failed \n");
			return;
		}
		memcpy(m_pic.img.plane[0], fBuffer, m_param.i_width * m_param.i_height);
		memcpy(m_pic.img.plane[1], fBuffer + m_param.i_width * m_param.i_height, m_param.i_width * m_param.i_height / 4);
		memcpy(m_pic.img.plane[2], fBuffer + m_param.i_width * m_param.i_height * 5 / 4, m_param.i_width * m_param.i_height / 4);   

		static x264_picture_t pic_out;
		x264_nal_t *nal = NULL;
		int i_nal, i;

		if(x264_handle != NULL)
		{
			if( x264_encoder_encode( x264_handle, &nal, &i_nal, &m_pic, &pic_out ) < 0 )
			{
				return;
			}
		}
		int offset = 0;
		static int t = 0;
		FILE *fout;

		Debug(ckite_log_message, "i_nal = %d\n", i_nal);
		for ( i = 0; i < i_nal; i++ )
		{
		#if 0
			if (t < 6)
			{
				char name[100] = {0};
				t++;
				snprintf(name, sizeof name, "nal%d.dat", t);
				fout = fopen(name, "wb+");
				size = fwrite(nal[i].p_payload,1,nal[i].i_payload,fout);
				fclose(fout);
				Debug(ckite_log_message, "size = %d\n",size);

			}
		#endif
			if(nal[i].p_payload[2] == 1)
			{
				offset = 3;
				nal_type = nal[i].p_payload[3];
			}
			else if (nal[i].p_payload[3] == 1)
			{
				offset = 4;
				nal_type = nal[i].p_payload[4];
			}
			if(i >= 1)
			{
				if(more_nal[i-1] == NULL)
				{
					more_nal_len[i-1] = nal[i].i_payload - offset;
					more_nal[i-1] = new char [more_nal_len[i-1] + 1];
					if (more_nal[i-1] != NULL)
					{
						memset(more_nal[i-1], 0x0, nal[i].i_payload - offset + 1);
						memcpy(more_nal[i-1], nal[i].p_payload + offset, nal[i].i_payload - offset);
						//Debug(ckite_log_message, "new sucess more_nal[%d], nal size %d\n", i-1, more_nal_len[i-1]);
					}
					else
					{
						Debug(ckite_log_message, "new failed with %d nal\n", i);
					}
				}
			}
			else 
			{
				memcpy(fTo, nal[i].p_payload + offset, nal[i].i_payload - offset);
				fFrameSize = nal[i].i_payload - offset;
			}
		}
#else
		if (size != 0)
			memcpy(fTo, fBuffer, size);
		fFrameSize = size;
#endif
	//Debug(ckite_log_message, "Deliver nal type %d with %d bytes.\n", nal_type, fFrameSize);
	fprintf(stderr, "Deliver nal type %d with %d bytes %x %x %x %x.\n", nal_type, fFrameSize, fTo[0], fTo[1], fTo[2], fTo[3]);
	fPictureEndMarker = True;
	afterGetting(this);

#ifdef ENC_SOURCE
	x264_picture_clean(&m_pic);
#endif
}

int EncoderVideoSource::computePresentationTime(void) 
{
	unsigned int curr_time;
	static unsigned int preTime = 0;
	static unsigned int nowTime = 0;
	static unsigned int val = 0;
	struct timeval tv;
//	struct timezone tz;
	int state = 0;
	int ret = 0;

	state = gettimeofday(&tv, NULL);
	if(state == 0)
	{
		if (fStartTime == 0)
		{
			fStartTime = tv.tv_sec * 1000 + tv.tv_usec/1000;
			fCurrTime = fStartTime;
			playTime = 0;
			pauseTime = 0;
		}
		else
		{
			fCurrTime = tv.tv_sec * 1000 + tv.tv_usec/1000;
		}
		fPresentationTime.tv_sec = (fCurrTime - playTime + pauseTime - fStartTime) / 1000;
		fPresentationTime.tv_usec = ((fCurrTime - playTime + pauseTime - fStartTime) % 1000) * 1000;
		Debug(ckite_log_message, "video fPresentationTime.tv_sec = %d\n", fPresentationTime.tv_sec);
		Debug(ckite_log_message, "video fPresentationTime.tv_usec = %d\n", fPresentationTime.tv_usec);
		fDurationInMicroseconds = 1000000 / fFramerate;
		
		if ((val %= 2 ) == 0)
		{
			Debug(ckite_log_message, "val is %d even\n", val);
			preTime = fPresentationTime.tv_sec * 1000 + fPresentationTime.tv_usec/1000;	
			ret = preTime - nowTime;
		}
		else if ((val %= 2) == 1)
		{
			Debug(ckite_log_message, "val is %d odd\n", val);
			nowTime = fPresentationTime.tv_sec * 1000 + fPresentationTime.tv_usec/1000;	
			ret = nowTime - preTime;
		}
		val ++;
	}
	Debug(ckite_log_message, "nowTime - preTime = %d\n", ret);
	return  ret;
}

void EncoderVideoSource::reset() 
{
	fStartTime = 0;
}

unsigned char* EncoderVideoSource::getConfigBytes(unsigned& numBytes) const 
{
	numBytes = fNumConfigBytes;
	return fConfigBytes;
}


EncoderAudioSource* EncoderAudioSource::createNew(UsageEnvironment& env, unsigned int samplerate, 
		unsigned int framelength, unsigned int bitrate, char *type)
{
	return new EncoderAudioSource(env, samplerate, framelength, bitrate, AUDIO_AMRNB, type);
}

EncoderAudioSource::EncoderAudioSource(UsageEnvironment& env, unsigned int samplerate, 
		unsigned int framelength, unsigned int bitrate, AudioCodec codec, char *type):
	AMRAudioSource(env, False, 1), fSampleRate(samplerate), fFrameLength(framelength*2), fBitrate(bitrate), fCodec(codec),fStartTime(0)
{
	memset(mediaType, 0, sizeof(mediaType));
	memcpy(mediaType, type, strlen(type));
	fBuffer = new unsigned char[framelength*2];
#ifdef STARV_TEST
	fp = fopen(AUDIO_FILE, "rb");
#endif

#ifdef ENC_SOURCE
	fAudioCodec = (int *)Encoder_Interface_init(0);
#endif
}

EncoderAudioSource::~EncoderAudioSource()
{
	if (fBuffer)
		delete [] fBuffer;
#ifdef STARV_TEST
	if (fp)
		fclose(fp);
#endif

#ifdef ENC_SOURCE
	if(fAudioCodec)
	{
		Encoder_Interface_exit(fAudioCodec);
	}
#endif
}

Boolean EncoderAudioSource::isAMRAudioSource() const
{
	return True;
}


int EncoderAudioSource::computeAudioPresentationTime(void)
{
	struct timeval tv;
//	struct timezone tz;
	int state = 0;
	unsigned int curr_time;
	state = gettimeofday(&tv, NULL);
	if(state == 0)
	{
		if (fStartTime == 0)
		{
			fStartTime = fCurrTime = tv.tv_sec * 1000 + tv.tv_usec/1000;
		}
		else
		{
			fCurrTime += 20;
		}
		fPresentationTime.tv_sec = (fCurrTime - fStartTime) / 1000;
		fPresentationTime.tv_usec = ((fCurrTime - fStartTime) % 1000) * 1000;
		Debug(ckite_log_message, "audio fPresentationTime.tv_sec = %d\n", fPresentationTime.tv_sec);
		Debug(ckite_log_message, "audio fPresentationTime.tv_usec = %d\n", fPresentationTime.tv_usec);
	}
	fDurationInMicroseconds = 20000;

	return 0;
}

void EncoderAudioSource::doGetNextFrame()
{
	// Copy an encoded audio frame into buffer `fTo'
	// Read a new frame into fBuffer, YUV420 format is assumed
	unsigned char frame[100];
	int size = fFrameLength;
	int audioType;
	Debug(ckite_log_message, "EncoderAudioSource::doGetNextFrame.\n");
	computeAudioPresentationTime(); 
	if (strcmp(mediaType, "store") == 0)
	{
		getStoreAudioFrame( fp, 0, (unsigned char*)fBuffer, &size, &audioType);
	}
	else
	{
		getRealAudioFrame(fp, mediaType,(char *)fBuffer, &size, &audioType);
	}
	if (size <= 0)
		size = 0;
	// Encode the frame
	
#ifdef ENC_SOURCE
	int ret = 0; 
	if (fAudioCodec != NULL)
	{
		ret = Encoder_Interface_Encode(fAudioCodec, MR122, (const short int*)fBuffer, frame, 0);
	}
	if (ret > 0)
	{
		fLastFrameHeader = frame[0];
		fFrameSize = ret-1;
		memcpy(fTo, frame+1, ret-1);
	}
#else
	if (size != 0)
		memcpy(fTo, fBuffer, size);
	fFrameSize = size;
#endif
	afterGetting(this);
}
