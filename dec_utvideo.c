/*
 *			GPAC - Multimedia Framework C SDK
 *
 *  This file is part of GPAC / Ut Video decoder filter, based on the Ut Video
 *  Codec Suite.
 *
 *  Ut Video is a fast lossless intra codec, met mostly inside AVI files coming
 *  out of capture and editing software. It has no container of its own, and
 *  the repository already has a filter that reads AVI - so this one is a link
 *  in a chain rather than a whole-file decoder: `avidmx` parses the AVI and,
 *  for a compressor fourcc it does not recognise, emits a pid whose codec id
 *  is that fourcc verbatim (gf_4cc_parse) together with the BITMAPINFOHEADER
 *  extra data. Those fourccs - ULY0, ULY2, ULY4, their BT.709 twins ULH0,
 *  ULH2, ULH4, and ULRG - are what this filter declares as input.
 *
 *  The Ut Video decoder hands YUV back in YV12/YV16/YV24 order, which is Y
 *  then V then U. GPAC's GF_PIXEL_YUV family is Y then U then V, so the two
 *  chroma planes are exchanged on the way out. GF_PIXEL_YVU exists and would
 *  save the exchange, but only for 4:2:0 - there is no YVU422 or YVU444 - and
 *  one rule for all three subsamplings is worth one plane copy.
 */

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <string.h>
#include <stdlib.h>

#include "utvideo_decode.h"

typedef struct
{
	GF_FilterPid *ipid, *opid;
	UTVDecoder *dec;
	UTVOutput layout;
	u32 width, height, frame_size, codecid;
	u8 *swap;      /* one chroma plane, for the U/V exchange */
	u32 swap_size;
} GF_UTVideoDecCtx;

static void utvdec_reset(GF_UTVideoDecCtx *ctx)
{
	if (ctx->dec)
	{
		utv_decoder_close(ctx->dec);
		ctx->dec = NULL;
	}
	if (ctx->swap)
	{
		gf_free(ctx->swap);
		ctx->swap = NULL;
	}
	ctx->swap_size = 0;
}

static GF_Err utvdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	const u8 *dsi = NULL;
	u32 dsi_size = 0, pixfmt, stride;
	GF_UTVideoDecCtx *ctx = (GF_UTVideoDecCtx *)gf_filter_get_udta(filter);

	if (is_remove)
	{
		if (ctx->opid)
		{
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		utvdec_reset(ctx);
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p)
		return GF_NOT_SUPPORTED;
	ctx->codecid = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	ctx->width = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	ctx->height = p ? p->value.uint : 0;
	if (!ctx->width || !ctx->height)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[UTVideoDec] The pid carries no picture size\n"));
		return GF_NOT_SUPPORTED;
	}

	/* The BITMAPINFOHEADER extra data holds the version and the frame-info
	 * size; the decoder needs it to know how to read the stream. */
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p)
	{
		dsi = p->value.data.ptr;
		dsi_size = p->value.data.size;
	}

	utvdec_reset(ctx);
	ctx->dec = utv_decoder_open(ctx->codecid, ctx->width, ctx->height, dsi, dsi_size,
	                            &ctx->layout, &ctx->frame_size);
	if (!ctx->dec)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[UTVideoDec] Ut Video refused the stream (fourcc %s, %ux%u, %u bytes of config)\n",
		                                    gf_4cc_to_str(ctx->codecid), ctx->width, ctx->height, dsi_size));
		return GF_NOT_SUPPORTED;
	}

	switch (ctx->layout)
	{
	case UTV_OUT_YUV420:
		pixfmt = GF_PIXEL_YUV;
		stride = ctx->width;
		ctx->swap_size = (ctx->width / 2) * (ctx->height / 2);
		break;
	case UTV_OUT_YUV422:
		pixfmt = GF_PIXEL_YUV422;
		stride = ctx->width;
		ctx->swap_size = (ctx->width / 2) * ctx->height;
		break;
	case UTV_OUT_YUV444:
		pixfmt = GF_PIXEL_YUV444;
		stride = ctx->width;
		ctx->swap_size = ctx->width * ctx->height;
		break;
	default:
		pixfmt = GF_PIXEL_RGB;
		stride = ctx->width * 3;
		ctx->swap_size = 0;
		break;
	}
	if (ctx->swap_size)
	{
		ctx->swap = (u8 *)gf_malloc(ctx->swap_size);
		if (!ctx->swap)
		{
			utvdec_reset(ctx);
			return GF_OUT_OF_MEM;
		}
	}

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(pixfmt));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(stride));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);

	return GF_OK;
}

static GF_Err utvdec_process(GF_Filter *filter)
{
	GF_FilterPacket *pck, *dst_pck;
	const u8 *data;
	u8 *output;
	u32 size, written;
	GF_UTVideoDecCtx *ctx = (GF_UTVideoDecCtx *)gf_filter_get_udta(filter);

	if (!ctx->dec)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck)
	{
		if (gf_filter_pid_is_eos(ctx->ipid))
		{
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = gf_filter_pck_get_data(pck, &size);
	if (!data || !size)
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->frame_size, &output);
	if (!dst_pck)
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OUT_OF_MEM;
	}

	written = utv_decode_frame(ctx->dec, output, data);
	if (!written)
	{
		gf_filter_pck_discard(dst_pck);
		gf_filter_pid_drop_packet(ctx->ipid);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[UTVideoDec] Failed to decode a frame\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/* Y is already where it belongs; exchange the two chroma planes that
	 * follow it, so YV12/YV16/YV24 becomes the GF_PIXEL_YUV order. */
	if (ctx->swap_size)
	{
		u8 *first = output + (ctx->frame_size - 2 * ctx->swap_size);
		u8 *second = first + ctx->swap_size;
		memcpy(ctx->swap, first, ctx->swap_size);
		memcpy(first, second, ctx->swap_size);
		memcpy(second, ctx->swap, ctx->swap_size);
	}

	/* Source first: gf_filter_pck_merge_properties(src, dst). This carries the
	 * timing avidmx put on the packet; without it the muxer refuses every
	 * sample with "no DTS/CTS". */
	gf_filter_pck_merge_properties(pck, dst_pck);
	gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1); /* Ut Video is intra only */
	gf_filter_pck_send(dst_pck);

	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static void utvdec_finalize(GF_Filter *filter)
{
	utvdec_reset((GF_UTVideoDecCtx *)gf_filter_get_udta(filter));
}

static const GF_FilterCapability UTVideoDecCaps[] =
	{
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_4CC('U', 'L', 'Y', '0')),
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_4CC('U', 'L', 'Y', '2')),
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_4CC('U', 'L', 'Y', '4')),
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_4CC('U', 'L', 'H', '0')),
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_4CC('U', 'L', 'H', '2')),
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_4CC('U', 'L', 'H', '4')),
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_4CC('U', 'L', 'R', 'G')),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister UTVideoDecoderRegister = {
	.name = "utvideodec",
	GF_FS_SET_DESCRIPTION("Ut Video lossless decoder")
		GF_FS_SET_HELP("This filter decodes Ut Video, the lossless intra codec found in AVI files. It is a chain link: avidmx parses the AVI and emits a pid whose codec id is the Ut Video fourcc, which this filter consumes.")
			.private_size = sizeof(GF_UTVideoDecCtx),
	SETCAPS(UTVideoDecCaps),
	.configure_pid = utvdec_configure_pid,
	.process = utvdec_process,
	.finalize = utvdec_finalize,
};

const GF_FilterRegister *EMSCRIPTEN_KEEPALIVE utvideodec_register(GF_FilterSession *session)
{
	return &UTVideoDecoderRegister;
}

#include "filter_register.h"
__attribute__((constructor))
void register_utvideodec(void) {
    gf_filter_auto_register("utvideodec", utvideodec_register);
}
