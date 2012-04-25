#ifndef _RTSP_SDK_H
#define _RTSP_SDK_h

#define FrameTime_t int 

void getVideoCodecConfig(int width, int height, int *profile_level_id, unsigned char *config, unsigned int *config_length);
void getServerInfo(char *host, unsigned short *port, int defaultType);
void getDeviceInfo(unsigned char channel, char *ip, unsigned short *port, char *mac);
void getHttpServerInfo(char *host, unsigned short *port);
void getUserAgent(char *ua_buf, int buf_len);
enum State{READY,KEYFAILED, DNSFAILED, SETUP, PLAY};
int setSessionState(char *session_name, int st);
int getSessionState(char *session_name);
void setSSRCMask(char *type, unsigned int mask);
void *getSSRCMask(void);
extern int httpRequestKey(char *recvBuffer, char *mac);
extern void realEntryMain(unsigned char channel);
extern void getListenPortByChannel(unsigned short *port, unsigned char channel);
extern void* getFileHandle( int camid, char* filename, char* sdp, int* sdpSize);
extern void releaseFileHandle( void* handle );
extern void getRecordDirectories( int camid, char** dirList, int* count, int* index, int* total_count);
extern void getRecordFiles( int camid, char* time, char** fileList, int* count, int* index, int* total_count);
extern void getStoreVideoFrame( void* handle, FrameTime_t* offset, unsigned char* framebuf, int* framesize, int* videoType, bool force_I_frame);
extern void getStoreAudioFrame( void* handle, FrameTime_t* offset, unsigned char* framebuf, int* framesize, int* audioType);
extern void getRealVideoFrame( unsigned char channel, void *handle, char* session_name, unsigned char *framebuf, int *framesize, int *videoType);
extern void getRealAudioFrame( void *handle, char* session_name, char *framebuf, int *framesize, int*audioType);
extern void getSubsessionParaConfig(char *session_name, int width, int height, unsigned int& video_bitrate, unsigned int& framerate, unsigned int& keyinterval,
																			   unsigned int& audio_bitrate, unsigned int& framelenght, unsigned int& samplerate);
extern void getStoreFileResolution(char *file_name, int& width, int &height);
extern void getUpgradeStatus(char *upgrade_version, char *upgrade_status);
extern void readkey(unsigned char channel, char *chCode);
extern void writekey(unsigned char channel, char const *chCode);
enum _camchannel
{
	CAMERA_CHANNEL_FALG,
	CAMERA_CHANNEL_1,
	CAMERA_CHANNEL_2,
	CAMERA_CHANNEL_3,
	CAMERA_CHANNEL_4,
	CAMERA_CHANNEL_5,
	CAMERA_CHANNEL_6,
	CAMERA_CHANNEL_7,
	CAMERA_CHANNEL_8,
	CAMERA_CHANNEL_9,
	CAMERA_CHANNEL_10,
	CAMERA_CHANNEL_11,
	CAMERA_CHANNEL_12,
	CAMERA_CHANNEL_13,
	CAMERA_CHANNEL_14,
	CAMERA_CHANNEL_15,
	CAMERA_CHANNEL_16,
};
#endif 	
