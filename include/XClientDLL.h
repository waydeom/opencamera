// XClientDLL.h
//

#include "XClient.h"

#define MAX_DISPLAY_REGION		16
//海康嵌入式硬盘录像机用
#define NET_DVR_DEFAULTBRIGHTNESS 6
#define NET_DVR_DEFAULTCONTRAST 6
#define NET_DVR_DEFAULTSATURATION  6
#define NET_DVR_DEFAULTHUE 6

/*
#define MAX_LINK 	6
#define MAX_DISKNUM 	16
#define MAX_CHANNUM 	16
#define MAX_ALARMIN 	16
#define MAX_ALARMOUT 	4
*/

//播放控制命令宏定义 NET_DVR_PlayBackControl,NET_DVR_PlayControlLocDisplay,NET_DVR_DecPlayBackCtrl的宏定义
#define NET_DVR_PLAYSTART		1//开始播放
#define NET_DVR_PLAYSTOP		2//停止播放
#define NET_DVR_PLAYPAUSE		3//暂停播放
#define NET_DVR_PLAYRESTART		4//恢复播放
#define NET_DVR_PLAYFAST		5//快放
#define NET_DVR_PLAYSLOW		6//慢放
#define NET_DVR_PLAYNORMAL		7//正常速度
#define NET_DVR_PLAYFRAME		8//单帧放
#define NET_DVR_PLAYSTARTAUDIO		9//打开声音
#define NET_DVR_PLAYSTOPAUDIO		10//关闭声音
#define NET_DVR_PLAYAUDIOVOLUME		11//调节音量
#define NET_DVR_PLAYSETPOS		12//改变文件回放的进度
#define NET_DVR_PLAYGETPOS		13//获取文件回放的进度
#define NET_DVR_PLAYGETTIME		14//获取当前已经播放的时间
#define NET_DVR_PLAYGETFRAME		15//获取当前已经播放的帧数
#define NET_DVR_GETTOTALFRAMES  	16//获取当前播放文件总的帧数
#define NET_DVR_GETTOTALTIME    	17//获取当前播放文件总的时间
#define NET_DVR_THROWBFRAME		20//丢B帧

#define XCLIENTDLL_API extern"C" __declspec(dllimport)

XCLIENTDLL_API BOOL __stdcall X_ClientInit(UINT nMessage,HWND hWnd);
XCLIENTDLL_API BOOL __stdcall X_ClientSetMR(LPCTSTR strFirstMRIP,LPCTSTR strSecondMRIP,WORD wFirstMRPort, WORD wType);
XCLIENTDLL_API LONG __stdcall X_ClientStart(PXCLIENT_VIDEOINFO pXClientinfo, LONG lUserID, WORD wType);
XCLIENTDLL_API LONG __stdcall X_ClientStartEx(PXCLIENT_VIDEOINFO pXClientInfo, PXCLIENT_MRSERVERINFO pMRServerInfo, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientStop(LONG StockHandle, WORD wType);
XCLIENTDLL_API LONG __stdcall X_ClientGetState(LONG StockHandle, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientCapPicFile(LONG StockHandle, LPTSTR szFileName, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientAudioStart(LONG StockHandle, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientAudioStop(WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientAudioVolumeSet(WORD wValue, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientSetBright(LONG StockHandle, WORD wValue, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientSetContrast(LONG StockHandle, WORD wValue, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientSetSaturation(LONG StockHandle, WORD wValue, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientSetHue(LONG StockHandle, WORD wValue, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientStartCaptureFile(LONG StockHandle, LPTSTR FileName, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientStopCapture(LONG StockHandle, WORD wType);
XCLIENTDLL_API BOOL __stdcall X_ClientCleanup();
XCLIENTDLL_API BOOL __stdcall X_ClientRigisterDrawFun(LONG StockHandle,void (CALLBACK* DrawFun)(LONG StockHandle,HDC hDc,DWORD dwUser),DWORD dwUser, WORD wType);//add by he 20070419
XCLIENTDLL_API BOOL  __stdcall X_ClientSetQuality(LONG StockHandle,WORD wPicQuality, WORD wType);//add by he 20071228
XCLIENTDLL_API BOOL  __stdcall X_ClientThrowBFrame(LONG StockHandle,DWORD dNum, WORD wType);//add by he 20071228
// added by xl, 2010-10-13
XCLIENTDLL_API BOOL __stdcall X_ClientSetCapPicCallBack(void (CALLBACK *TCapPicCallBack)(long StockHandle,char *pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReceaved));
// add end;

//decode card
XCLIENTDLL_API int  __stdcall X_HW_InitDecDevice(long *pDeviceTotal);
XCLIENTDLL_API int  __stdcall X_HW_InitDirectDraw(HWND hParent,COLORREF colorKey);
XCLIENTDLL_API int  __stdcall X_HW_ClearSurface();
XCLIENTDLL_API int  __stdcall X_HW_ReleaseDirectDraw();
XCLIENTDLL_API int  __stdcall X_HW_ReleaseDecDevice();
XCLIENTDLL_API int  __stdcall X_HW_SetDisplayPara(long lChnNum,XDISPLAY_PARA *pPara);
XCLIENTDLL_API int  __stdcall X_HW_StartCapFile(long lChnNum,char *sFileName);
XCLIENTDLL_API int  __stdcall X_HW_RefreshSurface();
XCLIENTDLL_API int  __stdcall X_HW_SetDspDeadlockMsg(HWND hWnd, UINT nMessage);
XCLIENTDLL_API int  __stdcall X_HW_ResetDsp(long nDspNum);
XCLIENTDLL_API int  __stdcall X_HW_RestoreSurface();
XCLIENTDLL_API int  __stdcall X_HW_ChannelOpen(long lChnNum);
XCLIENTDLL_API int  __stdcall X_HW_GetDecOutCount();
XCLIENTDLL_API int  __stdcall X_HW_SetDisplayRegion(UINT nDisplayChannel,UINT nRegionCount,REGION_PARAM *pParam,UINT nReserved);
XCLIENTDLL_API int  __stdcall X_HW_SetDecoderVideoOutput(UINT nDecodeChannel,UINT nPort,BOOL bOpen,UINT nDisplayChannel,UINT nDisplayRegion,UINT nReserved);
XCLIENTDLL_API int  __stdcall X_HW_SetDecoderAudioOutput(UINT nDecodeChannel,BOOL bOpen,UINT nOutputChannel);
XCLIENTDLL_API int  __stdcall X_HW_PlaySound(long lChnNum);
XCLIENTDLL_API int  __stdcall X_HW_StopSound(long lChnNum);
XCLIENTDLL_API LONG __stdcall X_ClientStart_Card(PXCLIENT_CARDINFO pXClientCardinfo, PXCLIENT_MRSERVERINFO pMRServerInfo,long nChannelNum, WORD wType);

//嵌入式
XCLIENTDLL_API LONG __stdcall X_NET_DVR_Login(char *sDVRIP,WORD wDVRPort,char *sUserName,char *sPassword,LPNET_DVR_DEVICEINFO lpDeviceInfo, WORD wType);//modify by he 20061222
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_Logout(LONG lUserID, WORD wType);//modify by he 20061222
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_PTZControl(LONG lStockHandle, LONG lUserID, BYTE bytChn, char* cmdbuf, int iBuflen, WORD wType);//modify by he 20061231
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_SetMessCallBack(BOOL (CALLBACK *fMessCallBack)(LONG lCommand,LONG lUserID,char *pBuf,DWORD dwBufLen, WORD wType));//modify by he 20070104
XCLIENTDLL_API LONG __stdcall X_NET_DVR_SetAlarm(LONG lUserID);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_CloseAlarm(LONG lAlarmHandle);
XCLIENTDLL_API LONG __stdcall X_NET_DVR_FindFile(LONG lUserID, PSEARCH_RECINFO pSearchInfo, WORD wType);//modify by he 20070130
XCLIENTDLL_API LONG __stdcall X_NET_DVR_FindNextFile(LONG lFindHandle, PNET_RECFILE_INFO pNetRecFileInfo, WORD wType);//modify by he 20070426
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_FindClose(LONG lFindHandle, WORD wType);
XCLIENTDLL_API LONG __stdcall X_NET_DVR_GetFileByName(LONG lUserID, PNET_RECFILE_INFO pNetRecFileInfo, char *sSavedFileName);//modify by he 20070426
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_StopGetFile(LONG lFileHandle, WORD wType);//modify by he 20070426
XCLIENTDLL_API int __stdcall X_NET_DVR_GetDownloadPos(LONG lFileHandle, WORD wType);//modify by he 20070426
XCLIENTDLL_API LONG __stdcall X_NET_DVR_PlayBackByName(LONG lUserID, PNET_RECFILE_INFO pNetRecFileInfo, HWND hWnd, WORD wType);//modify by he 20070426
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_StopPlayBack(LONG lPlayHandle, WORD wType);//modify by he 20070426
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_PlayBackControl(LONG lPlayHandle,DWORD dwControlCode,DWORD dwInValue,DWORD *lpOutValue, WORD wType);//modify by he 20070426
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_PlayBackCaptureFile(LONG lPlayHandle,char *sFileName, WORD wType);//modify by he 20070426
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_GetDVRWorkState(LONG lUserID,LPNET_DVR_WORKSTATE lpWorkState);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_StartDVRRecord(LONG lUserID,LONG lChannel,LONG lRecordType, BOOL bRec);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_SetDVRConfig(LONG lUserID, DWORD dwCommand,LONG lChannel, LPVOID lpInBuffer,DWORD dwInBufferSize);
XCLIENTDLL_API LONG __stdcall X_NET_DVR_Upgrade(LONG lUserID,char *sFileName);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_CloseUpgradeHandle(LONG lUpgradeHandle);
XCLIENTDLL_API int __stdcall X_NET_DVR_GetUpgradeState(LONG lUpgradeHandle);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_RebootDVR(LONG lUserID);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_ShutDownDVR(LONG lUserID);

//格式化硬盘	//add by he 20070112
XCLIENTDLL_API LONG __stdcall X_NET_DVR_FormatDisk(LONG lUserID,LONG lDiskNumber);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_GetFormatProgress(LONG lFormatHandle,LONG *pCurrentFormatDisk,LONG *pCurrentDiskPos,LONG *pFormatStatic);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_CloseFormatHandle(LONG lFormatHandle);

//对讲	//add by he 20070116
XCLIENTDLL_API LONG __stdcall X_NET_DVR_StartVoiceCom(LONG lUserID);
XCLIENTDLL_API BOOL __stdcall X_NET_DVR_StopVoiceCom(LONG lVoiceComHandle);