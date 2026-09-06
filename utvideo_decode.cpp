/*
 *  Ut Video decoding, kept away from GPAC's headers - see utvideo_decode.h.
 */

#include "stdafx.h"
#include "utvideo.h"
#include "Codec.h"

#include <stdlib.h>
#include <string.h>

#include "utvideo_decode.h"

struct UTVDecoder
{
	CCodec *codec;
	unsigned int size;
};

/* Each compressed fourcc has exactly one natural raw counterpart: the YUV
 * codecs only ever hand back planar YUV, the RGB ones only ever RGB (their
 * m_utvfDecoderOutput lists say so). ULH* is the same coding as ULY* with
 * BT.709 coefficients rather than BT.601, so it decodes to the same layout. */
static bool utv_pick_output(unsigned int fourcc, utvf_t *outfmt, UTVOutput *layout)
{
	switch (fourcc)
	{
	case UTVF_ULY0:
	case UTVF_ULH0:
		*outfmt = UTVF_YV12;
		*layout = UTV_OUT_YUV420;
		return true;
	case UTVF_ULY2:
	case UTVF_ULH2:
		*outfmt = UTVF_YV16;
		*layout = UTV_OUT_YUV422;
		return true;
	case UTVF_ULY4:
	case UTVF_ULH4:
		*outfmt = UTVF_YV24;
		*layout = UTV_OUT_YUV444;
		return true;
	case UTVF_ULRG:
		/* Top-down 24-bit RGB. The alpha-carrying ULRA is deliberately absent:
		 * it only decodes to BGRA/ARGB, and this build has no path out of the
		 * filter graph for an RGBA pid. */
		*outfmt = UTVF_NFCC_RGB_TD;
		*layout = UTV_OUT_RGB;
		return true;
	default:
		return false;
	}
}

extern "C" UTVDecoder *utv_decoder_open(unsigned int fourcc,
                                        unsigned int width, unsigned int height,
                                        const void *extradata, unsigned int extradata_size,
                                        UTVOutput *out_layout, unsigned int *out_size)
{
	utvf_t outfmt;
	UTVOutput layout;
	size_t gross[4] = { CBGROSSWIDTH_NATURAL, CBGROSSWIDTH_NATURAL,
	                    CBGROSSWIDTH_NATURAL, CBGROSSWIDTH_NATURAL };

	if (!utv_pick_output(fourcc, &outfmt, &layout))
		return NULL;

	CCodec *codec = CCodec::CreateInstance((utvf_t)fourcc, "GPAC");
	if (!codec)
		return NULL;

	size_t size = codec->DecodeGetOutputSize(outfmt, width, height, gross);
	if (!size)
	{
		CCodec::DeleteInstance(codec);
		return NULL;
	}

	if (codec->DecodeBegin(outfmt, width, height, gross, extradata, extradata_size) != 0)
	{
		CCodec::DeleteInstance(codec);
		return NULL;
	}

	UTVDecoder *dec = (UTVDecoder *)calloc(1, sizeof(UTVDecoder));
	if (!dec)
	{
		codec->DecodeEnd();
		CCodec::DeleteInstance(codec);
		return NULL;
	}
	dec->codec = codec;
	dec->size = (unsigned int)size;

	*out_layout = layout;
	*out_size = (unsigned int)size;
	return dec;
}

extern "C" unsigned int utv_decode_frame(UTVDecoder *dec, void *dst, const void *src)
{
	if (!dec || !dst || !src)
		return 0;
	return (unsigned int)dec->codec->DecodeFrame(dst, src);
}

extern "C" void utv_decoder_close(UTVDecoder *dec)
{
	if (!dec)
		return;
	if (dec->codec)
	{
		dec->codec->DecodeEnd();
		CCodec::DeleteInstance(dec->codec);
	}
	free(dec);
}
