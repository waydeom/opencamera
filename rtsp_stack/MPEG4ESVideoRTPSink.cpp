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
// RTP sink for MPEG-4 Elementary Stream video (RFC 3016)
// Implementation

#include "MPEG4ESVideoRTPSink.h"
#include "EncoderSource.h"
#include "RTSPSdk.h"
//#define CIF_CONFIG "\x00\x00\x01\xb0\x08\x00\x00\x01\xb5\x09\x00\x00\x01\x00\x00\x00\x01\x20\x00\x84\x40\x7d\x32\x9c\x2c\x10\x90\x51\x8f" // default value
//#define QCIF_CONFIG "\x00\x00\x01\xb0\x08\x00\x00\x01\xb5\x09\x00\x00\x01\x00\x00\x00\x01\x20\x00\x84\x40\xfa\x31\xf6\x0b\x08\x24\x28\xc7" // default value

MPEG4ESVideoRTPSink
::MPEG4ESVideoRTPSink(UsageEnvironment& env, void* owner, Groupsock* RTPgs,
		      unsigned char rtpPayloadFormat,
		      u_int32_t rtpTimestampFrequency)
  : VideoRTPSink(env, owner, RTPgs, rtpPayloadFormat, rtpTimestampFrequency, "MP4V-ES"),
    fVOPIsPresent(False), fFmtpSDPLine(NULL) {
}

MPEG4ESVideoRTPSink::~MPEG4ESVideoRTPSink() {
  delete[] fFmtpSDPLine;
}

MPEG4ESVideoRTPSink*
MPEG4ESVideoRTPSink::createNew(UsageEnvironment& env, void* owner, Groupsock* RTPgs,
			       unsigned char rtpPayloadFormat,
			       u_int32_t rtpTimestampFrequency) {
  return new MPEG4ESVideoRTPSink(env, owner, RTPgs, rtpPayloadFormat,
				 rtpTimestampFrequency);
}

Boolean MPEG4ESVideoRTPSink::sourceIsCompatibleWithUs(MediaSource& source) {
  // Our source must be an appropriate framer:
  return source.isMPEG4VideoStreamFramer();
}

#define VOP_START_CODE                    0x000001B6

void MPEG4ESVideoRTPSink
::doSpecialFrameHandling(unsigned fragmentationOffset,
			 unsigned char* frameStart,
			 unsigned numBytesInFrame,
			 struct timeval frameTimestamp,
			 unsigned numRemainingBytes) {
  if (fragmentationOffset == 0) {
    // Begin by inspecting the 4-byte code at the start of the frame:
    if (numBytesInFrame < 4) return; // shouldn't happen
    unsigned startCode = (frameStart[0]<<24) | (frameStart[1]<<16)
      | (frameStart[2]<<8) | frameStart[3];

    fVOPIsPresent = startCode == VOP_START_CODE;
  }

  // Set the RTP 'M' (marker) bit iff this frame ends a VOP
  // (and there are no fragments remaining).
  // This relies on the source being a "MPEG4VideoStreamFramer".
  EncoderVideoSource* framerSource = (EncoderVideoSource*)fSource;
  if (framerSource != NULL && framerSource->pictureEndMarker()
      && numRemainingBytes == 0) {
    setMarkerBit();
    framerSource->pictureEndMarker() = False;
  }

  // Also set the RTP timestamp.  (We do this for each frame
  // in the packet, to ensure that the timestamp of the VOP (if present)
  // gets used.)
  setTimestamp(frameTimestamp);
}

Boolean MPEG4ESVideoRTPSink::allowFragmentationAfterStart() const {
  return True;
}

Boolean MPEG4ESVideoRTPSink
::frameCanAppearAfterPacketStart(unsigned char const* /*frameStart*/,
				 unsigned /*numBytesInFrame*/) const {
  // Once we've packed a VOP into the packet, then no other
  // frame can be packed into it:
  return !fVOPIsPresent;
}

char const* MPEG4ESVideoRTPSink::auxSDPLine() {
  // Generate a new "a=fmtp:" line each time, using parameters from
  // our framer source (in case they've changed since the last time that
  // we were called):
  EncoderVideoSource* framerSource = (EncoderVideoSource*)fSource;
//  if (framerSource == NULL) return NULL; // we don't yet have a source

  int profile_level_id = 0;
#if 0
  if (framerSource)
	  framerSource->profile_and_level_indication();
  if (profile_level_id == 0) 
	  profile_level_id = 8; // default value
#endif

  unsigned int configLength = 0;
  unsigned char configString[100] = {0};
  unsigned char *config = NULL;
  if (framerSource)
	  config = framerSource->getConfigBytes(configLength);
  if (config == NULL)
  {
	  getVideoCodecConfig(fWidth,fHeight,&profile_level_id,configString, &configLength); //
  	  if (profile_level_id == 0) 
	  	  profile_level_id = 8; // default value
	  config = configString;
      configLength = configLength;
  }
  char const* fmtpFmt =
    "a=fmtp:%d "
    "profile-level-id=%d;"
    "config=";
  unsigned fmtpFmtSize = strlen(fmtpFmt)
    + 3 /* max char len */
    + 3 /* max char len */
    + 2*configLength /* 2*, because each byte prints as 2 chars */
    + 2 /* trailing \r\n */
	+ 256;
  char* fmtp = new char[fmtpFmtSize];
  sprintf(fmtp, fmtpFmt, rtpPayloadType(), profile_level_id);
  char* endPtr = &fmtp[strlen(fmtp)];
  for (unsigned i = 0; i < configLength; ++i) {
    sprintf(endPtr, "%02X", config[i]);
    endPtr += 2;
  }
  endPtr += sprintf(endPtr, "\r\n");
  //endPtr += sprintf(endPtr, "a=framerate:10\r\n");
 // endPtr += sprintf(endPtr, "a=framesize:96 352-288\r\n");

  delete[] fFmtpSDPLine;
  fFmtpSDPLine = strDup(fmtp);
  delete[] fmtp;
  return fFmtpSDPLine;
}
