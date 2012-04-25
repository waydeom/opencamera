#ifndef XVID_INTERFACE_H_
#define XVID_INTERFACE_H_

void* xvid_enc_init(int width, int height, int bitrate, int framerate, int key_interval, int use_assembler);

int xvid_encode_frame(void* handle, unsigned char* frame, int width, int height, unsigned char* bitstream, unsigned int inrc);

void xvid_release(void* handle);

#endif
