// XClient.h
//

//need include
//hikclient.lib DS40xxSDK.lib hieclient.lib HCNetSDK.lib NVClientDLL.lib 

#define SERIALNO_LEN 	48
#define MAXDECCARDCHN	64

//For For HIK hie nv hikDVR dhDVR jpDVR

#define HIKCLIENT
//#define	HIECLIENT
#define NVCLIENT
#define HIKNETDVRCLIENT
#define DHNETDVRCLIENT
//#define JPNETDVRCLIENT	//金鹏嵌入式	//add by he 20071011

enum{HIK=0, HIE, NVINFO, HIKNETDVR, DHNETDVR, JPNETDVR};//modify by he 20071011
enum{NORMALMODE=0,OVERLAYMODE};
enum{UDPMODE=0, TCPMODE, MULTIMODE};

enum{LAN = 0, ADSL, ISDN, PSTN};

//海康嵌入式
#define NET_DVR_DEFAULTBRIGHTNESS 6
#define NET_DVR_DEFAULTCONTRAST 6
#define NET_DVR_DEFAULTSATURATION  6
#define NET_DVR_DEFAULTHUE 6

#define MAX_ALARMOUT 4
#define MAX_CHANNUM 16
#define MAX_DISKNUM 16
#define MAX_LINK 6
#define MAX_ALARMIN 16

//客户端收到的消息
//#define RECDATAERR	0 //接受网络数据异常
//#define PLAYERR     1 //客户端播放出现异常
//#define REFUSED     2 //客户端连接被拒绝
//#define SERVERCLOSE 3 //服务器退出
//#define NETFAILED   4 //与服务器通信出现异常

//客户端状态
#define INVALID			-1	//无效
#define CONNECTING	    1	//正在连接
#define RECEIVING		2	//开始接收图像
#define HALT			3	//异常退出
#define FINISH		    4	//接收完毕，推出
#define UNREACHABLE		5	//无法联系服务器
#define REFUSE		    6	//服务器拒绝访问

typedef struct{
	BYTE	m_bytRemoteChannel;	//接收通道号从0开始
	BYTE	m_bytSendMode;		//网络连接方式{UDPMODE=0,TCPMODE,MULTIMODE}
	BYTE	m_bytSendType;		//网络传输类型{LAN = 0, ADSL, ISDN, PSTN}
	BYTE	m_bytImgFormat;		//接收图像格式（0为服务端主通道图像；1为服务端子通道图像）
	UINT	m_nPort;			//接收端口号
	char    *m_szIPAddress;		//服务端IP地址
	char    *m_szUserName;		//用户名
	char    *m_szUserPassword;	//密码
	BOOL	m_bUserCheck;		//是否进行用户校验
	HWND	m_hShowVideo;		//显示区域
}XCLIENT_VIDEOINFO, *PXCLIENT_VIDEOINFO;

typedef struct{
	long bToScreen;		//输出到显示器
	long bToVideoOut;	//1
	long nLeft;
	long nTop;
	long nWidth;
	long nHeight;
	long nReserved;		//0
}XDISPLAY_PARA,*PXDISPLAY_PARA;

typedef struct{
	BYTE    m_bRemoteChannel;	//接收通道号从0开始
	BYTE    m_bSendMode;		//网络连接方式{UDPMODE=0,TCPMODE,MULTIMODE}
	BYTE    m_bRight;			//保留参数
	UINT	m_nPort;			//接收端口号
	char    *m_szIPAddress;		//服务端IP地址
	char    *m_szUserName;		//用户名
	char    *m_szUserPassword;	//密码
	BOOL    m_bUserCheck;		//是否进行用户校验
	XDISPLAY_PARA displaypara;	//显示区域
	LONG	lUserID;			//海康嵌入式用户ID	//add by he 20070402
}XCLIENT_CARDINFO, *PXCLIENT_CARDINFO;

typedef struct{
	char MRServerIPAddress[16];	//转发服务器IP地址
	WORD MRServerPort;			//转发服务器端口
	BYTE Priority;				//转发服务器控制权限(暂时只限制切换通道号)
	BYTE withhold;				//保留
}XCLIENT_MRSERVERINFO, *PXCLIENT_MRSERVERINFO;

/*
typedef struct  {
	BYTE sSerialNumber[SERIALNO_LEN];  //序列号
	BYTE byAlarmInPortNum;		//DVR报警输入个数
	BYTE byAlarmOutPortNum;		//DVR报警输出个数
	BYTE byDiskNum;				//DVR 硬盘个数
	BYTE byDVRType;				//DVR类型, 
	BYTE byChanNum;				//DVR 通道个数
	BYTE byStartChan;			//起始通道号,例如DVS-1,DVR - 1
}NET_DVR_DEVICEINFO, *LPNET_DVR_DEVICEINFO;
*/

typedef struct
{
	UINT left;
	UINT top;
	UINT width;
	UINT height;
	COLORREF color;
	UINT param;
}REGION_PARAM;


typedef struct
{
	FILETIME	FCreationTime;//文件开始时间
	FILETIME	FLastWriteTime;//文件结束时间
	__int64		iFLength;
	char		szFPath[MAX_PATH];
	BYTE		bytChn;//(1-MAXCHN)
	BYTE		bytType;//0:全部1:手动2:自动3:报警4:事件
}RECFILE_INFO, *PRECFILE_INFO;

//add by he 20070425
typedef struct
{
	DWORD dwYear;		//年
	DWORD dwMonth;		//月
	DWORD dwDay;		//日
	DWORD dwHour;		//时
	DWORD dwMinute;		//分
	DWORD dwSecond;		//秒
} NET_TIME,*LPNET_TIME;

//add by he 20070425
typedef struct {
    unsigned int     ch;              //通道号
    char             filename[128];   //文件名
    unsigned int     size;            //文件长度
    NET_TIME         starttime;       //开始时间
    NET_TIME         endtime;         //结束时间
    unsigned int     driveno;         //磁盘号
    unsigned int     startcluster;    //起始簇号
}NET_RECORDFILE_INFO, *LPNET_RECORDFILE_INFO;


//金鹏
typedef struct _tagOperateDay
{
	SYSTEMTIME st;
	DWORD dwDate;
}OperateDay;

typedef struct tagChannelFileInfo
{
	int			Disk;	//文件位于那一分区
	char		FileName[40];		//文件名
	int			nDiskSerial;	// 文件所在的卷号
}ChannelFileInfo,*PChannelFileInfo;

//add by he 20071121
typedef struct
{
	int nChannel;
	OperateDay ChnDayInfo;
	ChannelFileInfo ChnFileInfo;
}JP_FILEINFO, *PJP_FILEINFO;

//add by he 20070425
typedef struct
{
	WORD wFileType;//类型按照服务器类型
	union 
	{
		RECFILE_INFO NetFileinfo;//PC、海康嵌入式返回的文件结构
		NET_RECORDFILE_INFO dhFileinfo;//大华嵌入式返回的文件结构
		JP_FILEINFO jpFileInfo;//金鹏文件播放结构	//add by he 20071121
	}FileInfo;
}NET_RECFILE_INFO, *PNET_RECFILE_INFO;

typedef struct
{
	FILETIME	FFormTime;//开始时间
	FILETIME	FToTime;//结束时间
	BYTE		bytChn;//表示通道号
	BYTE		bytType;//录像类型
	char		szServerAddr[MAX_PATH];//需搜索的服务器地址（可以是域名或IP地址），为空则不进行判断
}SEARCH_RECINFO, *PSEARCH_RECINFO;

typedef struct  {
	BYTE sSerialNumber[SERIALNO_LEN];  //序列号
	BYTE byAlarmInPortNum;		//DVR报警输入个数
	BYTE byAlarmOutPortNum;		//DVR报警输出个数
	BYTE byDiskNum;				//DVR 硬盘个数
	BYTE byDVRType;				//DVR类型, 
	BYTE byChanNum;				//DVR 通道个数
	BYTE byStartChan;			//起始通道号,例如DVS-1,DVR - 1
}NET_DVR_DEVICEINFO, *LPNET_DVR_DEVICEINFO;

typedef struct {
	BYTE byRecordStatic; //通道是否在录像,0-不录像,1-录像
	BYTE bySignalStatic; //连接的信号状态,0-正常,1-信号丢失
	BYTE byHardwareStatic;//通道硬件状态,0-正常,1-异常,例如DSP死掉
	char reservedData;
	DWORD dwBitRate;//实际码率
	DWORD dwLinkNum;//客户端连接的个数
	DWORD dwClientIP[MAX_LINK];//客户端的IP地址
}NET_DVR_CHANNELSTATE,*LPNET_DVR_CHANNELSTATE;

typedef struct {
	DWORD dwVolume;//硬盘的容量
	DWORD dwFreeSpace;//硬盘的剩余空间
	DWORD dwHardDiskStatic; //硬盘的状态,休眠,活动,不正常等
}NET_DVR_DISKSTATE,*LPNET_DVR_DISKSTATE;


typedef struct{
	DWORD dwDeviceStatic; 	//设备的状态,0-正常,1-CPU占用率太高,超过85%,2-硬件错误,例如串口死掉
	NET_DVR_DISKSTATE  struHardDiskStatic[MAX_DISKNUM]; 
	NET_DVR_CHANNELSTATE struChanStatic[MAX_CHANNUM];//通道的状态
	BYTE  byAlarmInStatic[MAX_ALARMIN]; //报警端口的状态,0-没有报警,1-有报警
	BYTE  byAlarmOutStatic[MAX_ALARMOUT]; //报警输出端口的状态,0-没有输出,1-有报警输出
	DWORD  dwLocalDisplay;//本地显示状态,0-正常,1-不正常
}NET_DVR_WORKSTATE,*LPNET_DVR_WORKSTATE;


