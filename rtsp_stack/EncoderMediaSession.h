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
// A data structure that represents a session that consists of
// potentially multiple (audio and/or video) sub-sessions
// (This data structure is used for media *streamers* - i.e., servers.
//  For media receivers, use "MediaSession" instead.)
// C++ header

#ifndef _ENCODER_MEDIA_SESSION_HH
#define _ENCODER_MEDIA_SESSION_HH

#ifndef _MEDIA_HH
#include "Media.h"
#endif
#ifndef _GROUPEID_HH
#include "GroupEId.h"
#endif
#ifndef _RTP_INTERFACE_HH
#include "RTPInterface.h" // for ServerRequestAlternativeByteHandler
#endif

class EncoderMediaSession: public Medium {
public:
  static EncoderMediaSession* createNew(UsageEnvironment& env,
				       char const* streamName = NULL,
					   char const* mac = NULL,
					   char const* sn = NULL,
					   const int cam_id = 1,
				       char const* info = NULL,
				       char const* description = NULL,
				       Boolean isSSM = False,
				       char const* miscSDPLines = NULL);

  virtual ~EncoderMediaSession();

  static Boolean lookupByName(UsageEnvironment& env,
                              char const* mediumName,
                              EncoderMediaSession*& resultSession);

  char* generateSDPDescription(); // based on the entire session
      // Note: The caller is responsible for freeing the returned string

  char const* streamName() const { return fStreamName; }

  Boolean addSubsession(EncoderMediaSession* subsession);

  void testScaleFactor(float& scale); // sets "scale" to the actual supported scale
  float duration() const;
    // a result == 0 means an unbounded session (the default)
    // a result < 0 means: subsession durations differ; the result is -(the largest)
    // a result > 0 means: this is the duration of a bounded session

  unsigned referenceCount() const { return fReferenceCount; }
  void incrementReferenceCount() { ++fReferenceCount; }
  void decrementReferenceCount() { if (fReferenceCount > 0) --fReferenceCount; }
  Boolean& deleteWhenUnreferenced() { return fDeleteWhenUnreferenced; }

protected:
  EncoderMediaSession(UsageEnvironment& env, char const* streamName,
		     char const* info, char const* description,
		     Boolean isSSM, char const* miscSDPLines);
  // called only by "createNew()"

private: // redefined virtual functions
  virtual Boolean isServerMediaSession() const;

private:
  Boolean fIsSSM;

  // Linkage fields:
  friend class ServerMediaSubsessionIterator;
  ServerMediaSubsession* fSubsessionsHead;
  ServerMediaSubsession* fSubsessionsTail;
  unsigned fSubsessionCounter;

  char* fStreamName;
  char* fInfoSDPString;
  char* fDescriptionSDPString;
  char* fMiscSDPLines;
  struct timeval fCreationTime;
  unsigned fReferenceCount;
  Boolean fDeleteWhenUnreferenced;
};


#endif
