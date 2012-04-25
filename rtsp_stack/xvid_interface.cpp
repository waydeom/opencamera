#ifdef SDKMPEG4ENCODER
#include <stdio.h>
#include <memory.h>
#include "../include/xvid.h"

#define ABS_MAXFRAMENR 9999
#define MAX_ZONES   64
static xvid_enc_zone_t ZONES[MAX_ZONES];
static int NUM_ZONES = 0;
static int ARG_STATS = 0;
static int ARG_DUMP = 0;
static int ARG_LUMIMASKING = 0;
static int ARG_BITRATE = 0;
static int ARG_SINGLE = 1;
//static char *ARG_PASS1 = "xvid.stat";
static char *ARG_PASS1 = 0;
//static char *ARG_PASS2 = "xvid.stat";
static char *ARG_PASS2 = 0;
static float ARG_FRAMERATE = 25.00f;
static int ARG_MAXFRAMENR = ABS_MAXFRAMENR;
static int ARG_MAXKEYINTERVAL = 0;
static char *ARG_INPUTFILE = NULL;
static int ARG_INPUTTYPE = 0;
static int ARG_SAVEMPEGSTREAM = 0;
static int ARG_SAVEINDIVIDUAL = 0;
static char *ARG_OUTPUTFILE = NULL;
static int ARG_BQRATIO = 150;
static int ARG_BQOFFSET = 100;
static int ARG_MAXBFRAMES = 0;
static int ARG_PACKED = 0;
static int ARG_DEBUG = 0;
static int ARG_VOPDEBUG = 0;
static int ARG_GREYSCALE = 0;
static int ARG_QTYPE = 0;
static int ARG_QMATRIX = 0;
static int ARG_GMC = 0;
static int ARG_INTERLACING = 0;
static int ARG_QPEL = 0;
static int ARG_TURBO = 0;
static int ARG_VHQMODE = 0;
static int ARG_BVHQ = 0;
static int ARG_CLOSED_GOP = 0;
xvid_plugin_single_t single;
xvid_plugin_2pass2_t rc2pass2;
xvid_plugin_2pass1_t rc2pass1;


void* xvid_enc_init(int width, int height, int bitrate, int framerate, int key_interval, int use_assembler)
{
	int xerr=0;
	void * handle;
	//xvid_plugin_cbr_t cbr;
	//xvid_plugin_fixed_t rcfixed;
	xvid_enc_plugin_t plugins[7];
	xvid_gbl_init_t xvid_gbl_init;
	xvid_enc_create_t xvid_enc_create;

	/*------------------------------------------------------------------------
	 * XviD core initialization
	 *----------------------------------------------------------------------*/

	/* Set version -- version checking will done by xvidcore */
	memset(&xvid_gbl_init, 0, sizeof(xvid_gbl_init));
	xvid_gbl_init.version = XVID_VERSION;
    xvid_gbl_init.debug = 0;
	xvid_gbl_init.cpu_flags = 0;

	/* Initialize XviD core -- Should be done once per __process__ */
	xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL);

	/*------------------------------------------------------------------------
	 * XviD encoder initialization
	 *----------------------------------------------------------------------*/

	/* Version again */
	memset(&xvid_enc_create, 0, sizeof(xvid_enc_create));
	xvid_enc_create.version = XVID_VERSION;

	/* Width and Height of input frames */
	xvid_enc_create.width = width;
	xvid_enc_create.height = height;
	xvid_enc_create.profile = XVID_PROFILE_S_L0;

	/* init plugins  */
    xvid_enc_create.zones = ZONES;
    xvid_enc_create.num_zones = NUM_ZONES;

	xvid_enc_create.plugins = plugins;
	xvid_enc_create.num_plugins = 0;
	if (ARG_SINGLE) {
		memset(&single, 0, sizeof(xvid_plugin_single_t));
		single.version = XVID_VERSION;
		single.bitrate = bitrate * 1000;

		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_single;
		plugins[xvid_enc_create.num_plugins].param = &single;
		xvid_enc_create.num_plugins++;
	}
	if (ARG_PASS2) {
		memset(&rc2pass2, 0, sizeof(xvid_plugin_2pass2_t));
		rc2pass2.version = XVID_VERSION;
		rc2pass2.filename = ARG_PASS2;
		rc2pass2.bitrate = bitrate * 1000;

		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_2pass2;
		plugins[xvid_enc_create.num_plugins].param = &rc2pass2;
		xvid_enc_create.num_plugins++;
	}
	if (ARG_PASS1) {
		memset(&rc2pass1, 0, sizeof(xvid_plugin_2pass1_t));
		rc2pass1.version = XVID_VERSION;
		rc2pass1.filename = ARG_PASS1;
		
		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_2pass1;
		plugins[xvid_enc_create.num_plugins].param = &rc2pass1;
		xvid_enc_create.num_plugins++;
	}

	/* No fancy thread tests */
	xvid_enc_create.num_threads = 0;

	xvid_enc_create.fincr = 1000/framerate;  // framerate increment; set to zero for avriable framerate
	xvid_enc_create.fbase = 1000;

	/* Maximum key frame interval */
    if (key_interval > 0) {
        xvid_enc_create.max_key_interval = key_interval;
    }else {
	    xvid_enc_create.max_key_interval = framerate *3;
    }

	/* Bframes settings */
	xvid_enc_create.max_bframes = ARG_MAXBFRAMES;
	xvid_enc_create.bquant_ratio = ARG_BQRATIO;
	xvid_enc_create.bquant_offset = ARG_BQOFFSET;

	/* Dropping ratio frame -- we don't need that */
	xvid_enc_create.frame_drop_ratio = 0;

	/* Global encoder options */
	xvid_enc_create.global = 0;

	if (ARG_PACKED)
		xvid_enc_create.global |= XVID_GLOBAL_PACKED;

	if (ARG_CLOSED_GOP)
		xvid_enc_create.global |= XVID_GLOBAL_CLOSED_GOP;

	if (ARG_STATS)
		xvid_enc_create.global |= XVID_GLOBAL_EXTRASTATS_ENABLE;

	/* I use a small value here, since will not encode whole movies, but short clips */
	xerr = xvid_encore(NULL, XVID_ENC_CREATE, &xvid_enc_create, NULL, NULL);

	/* Retrieve the encoder instance from the structure */
	handle = xvid_enc_create.handle;

	return handle;
}

int xvid_encode_frame(void* handle, unsigned char* frame, int width, int height, unsigned char* bitstream, unsigned int incr) {
	xvid_enc_frame_t xvid_enc_frame;
	int ret;
	unsigned char *buf;
	
	buf = frame;
		
	/* Version for the frame and the stats */
	memset(&xvid_enc_frame, 0, sizeof(xvid_enc_frame));
	xvid_enc_frame.version = XVID_VERSION;

	/* Bind output buffer */
	xvid_enc_frame.bitstream = bitstream;
	xvid_enc_frame.length = -1;
		
	/* Initialize input image fields */
	xvid_enc_frame.input.plane[0] = buf;
	xvid_enc_frame.input.csp = XVID_CSP_I420;
	xvid_enc_frame.input.stride[0] = width;
		
	/* Set up core's general features */
	xvid_enc_frame.vol_flags = 0;
		
	/* Set up core's general features */
	xvid_enc_frame.vop_flags = XVID_VOP_HALFPEL | XVID_VOP_INTER4V;
		
	/* Frame type -- let core decide for us */
	xvid_enc_frame.type = XVID_TYPE_AUTO;
		
	/* Force the right quantizer -- It is internally managed by RC plugins */
	xvid_enc_frame.quant = 0;
		
	/* Set up motion estimation flags */
	xvid_enc_frame.motion = 	XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16;
		
	/* We don't use special matrices */
	xvid_enc_frame.quant_intra_matrix = NULL;
	xvid_enc_frame.quant_inter_matrix = NULL;

	xvid_enc_frame.fincr = 0;
	
	ret = xvid_encore(handle, XVID_ENC_ENCODE, &xvid_enc_frame, NULL, &incr);

	return ret;
}

void xvid_release(void* handle)
{
	xvid_encore(handle, XVID_ENC_DESTROY, NULL, NULL, NULL);
}

#endif
