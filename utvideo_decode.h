/*
 *  C face of the Ut Video decoder.
 *
 *  Ut Video's core is C++ (CCodec, a class with virtual methods) and its
 *  headers pull in utvideo.h, whose typedefs collide with GPAC's. Keeping the
 *  two apart in separate translation units is the same arrangement libpgf and
 *  libpsd use next door, and it is why this header exists: the filter never
 *  sees a Ut Video header, only these four functions.
 */

#ifndef UTVIDEO_DECODE_H
#define UTVIDEO_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

/* What the decoder was asked to produce, and therefore how the frame that
 * comes back is laid out. */
typedef enum
{
	UTV_OUT_YUV420 = 0, /* three planes, Y then V then U (YV12 order) */
	UTV_OUT_YUV422 = 1, /* three planes, Y then V then U (YV16 order) */
	UTV_OUT_YUV444 = 2, /* three planes, Y then V then U (YV24 order) */
	UTV_OUT_RGB    = 3  /* packed 24-bit, top-down */
} UTVOutput;

typedef struct UTVDecoder UTVDecoder;

/* Returns NULL when the fourcc is not a Ut Video one, or when the library
 * refuses the stream. On success *out_layout and *out_size describe the frame
 * buffer utv_decode_frame expects. */
UTVDecoder *utv_decoder_open(unsigned int fourcc,
                             unsigned int width, unsigned int height,
                             const void *extradata, unsigned int extradata_size,
                             UTVOutput *out_layout, unsigned int *out_size);

/* Decodes one frame into dst, which must hold out_size bytes. Returns the
 * number of bytes written, 0 on failure. */
unsigned int utv_decode_frame(UTVDecoder *dec, void *dst, const void *src);

void utv_decoder_close(UTVDecoder *dec);

#ifdef __cplusplus
}
#endif

#endif /* UTVIDEO_DECODE_H */
