//#define CKITE_DEBUG

#if defined(CKITE_DEBUG)
#define ckite_use_debug					1
#else
#define ckite_use_debug					0
#endif

#if ckite_use_debug == 1
#define showlog fprintf
#else
#define showlog
#endif

typedef enum
{
	ckite_log_message,        //message
	ckite_log_warning,        //warning
	ckite_log_error,          //error
	ckite_log_misc			//miscarriage
} ckite_log_level;

#ifndef __WIN32__
#define Debug(Level,fmt,...) \
        do{ \
			if (Level == ckite_log_message) { \
				showlog(stderr, "[info]:"fmt"", ##__VA_ARGS__);}\
			else if (Level == ckite_log_warning) { \
				showlog(stderr, "[warning]:"fmt"", ##__VA_ARGS__);}\
			else if (Level == ckite_log_warning) { \
				showlog(stderr, "[error]:"fmt"", #__VA_ARGS__);}\
			else if (Level == ckite_log_warning) { \
				showlog(stderr, "[misc]:"fmt"", ##__VA_ARGS__);}\
		}while(0)
#else
#define Debug
#endif
