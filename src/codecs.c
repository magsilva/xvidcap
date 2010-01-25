/*
 * Copyright (C) 2004-07 Karl, Frankfurt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libintl.h>
#include <math.h>
#include <locale.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "app_data.h"
#include "codecs.h"
#include "xvidcap-intl.h"

/*
 * fps and fps_rage arrays for use in the codecs array
 */
static const XVC_FpsRange fps_range[] = {
	{
		{1, 10},
		{100, 1}
	}
};

#define fps_range (sizeof(fps_range) / sizeof(XVC_FpsRange))


/**
 * Available video codecs.
 */
const XVC_VidCodec xvc_video_codecs[] = {
	{
		"NONE",
		N_("NONE"),
		CODEC_ID_NONE
	},
    {
		"PGM",
		N_("Portable Graymap"),
		CODEC_ID_PGM
	},
    {
		"PPM",
		N_("Portable Pixmap"),
		CODEC_ID_PPM
	},
    {
		"PNG",
		N_("Portable Network Graphics"),
		CODEC_ID_PNG
	},
    {
		"JPEG",
		N_("Joint Picture Expert Group"),
		CODEC_ID_MJPEG
	},
    {
		"MPEG1",
		N_("MPEG 1"),
		CODEC_ID_MPEG1VIDEO
	},
    {
		"MJPEG",
		N_("MJPEG"),
		CODEC_ID_MJPEG
	},
    {
		"MPEG4",
		N_("MPEG 4 (DIVX)"),
		CODEC_ID_MPEG4
	},
    {
		"MS_DIV2",
		N_("Microsoft DIVX 2"),
		CODEC_ID_MSMPEG4V2
	},
    {
		"MS_DIV3",
		N_("Microsoft DIVX 3"),
		CODEC_ID_MSMPEG4V3
	},
    {
		"FFV1",
		N_("FFmpeg Video 1"),
		CODEC_ID_FFV1
	},
    {
		"FLASH_VIDEO",
		N_("Flash Video"),
		CODEC_ID_FLV1
	},
    {
		"FLASH_SV",
		N_("Flash Screen Video"),
		CODEC_ID_FLASHSV
	},
    {
		"FLASH_SV2",
		N_("Flash Screen Video 2"),
		CODEC_ID_FLASHSV2
	},
    {
		"DV",
		N_("DV Video"),
		CODEC_ID_DVVIDEO
	},
    {
		"MPEG2",
		N_("MPEG2 Video"),
		CODEC_ID_MPEG2VIDEO
	},
    {
		"THEORA",
		N_("Ogg Theora"),
		CODEC_ID_THEORA
	},
	{
		"SVQ1",
		N_("Soerensen VQ 1"),
		CODEC_ID_SVQ1
	},
	{
		"VP6",
		N_("VP6"),
		CODEC_ID_VP6
	},
	{
		"H264",
		N_("MPEG-4 Part 10 (h. 264)"),
		CODEC_ID_H264
	}
};

/**
 * Available audio codecs.
 */
const XVC_AuCodec xvc_audio_codecs[] = {
    {
		"NONE",
	     N_("NONE"),
    	 CODEC_ID_NONE
	},
	{
		"MP2",
		N_("MPEG2"),
		CODEC_ID_MP2
	},
	{
		"MP3",
		N_("MPEG2 Layer 3"),
		CODEC_ID_MP3
	},
	{
		"VORBIS",
		N_("Ogg Vorbis"),
		CODEC_ID_VORBIS
	},
	{
		"AC3",
		N_("Dolby Digital AC-3"),
		CODEC_ID_AC3
	},
	{
		"PCM16",
		N_("PCM"),
		CODEC_ID_PCM_S16LE
	},
	{
		"WMA1",
		N_("Windows Media Audio v1"),
		CODEC_ID_WMAV1
	},
	{
		"WMA2",
		N_("Windows Media Audio v2"),
		CODEC_ID_WMAV2
	},
	{
		"AAC",
		N_("Advanced Audio Coding"),
		CODEC_ID_AAC
	},
	{
		"SPEEX",
		N_("Speex"),
		CODEC_ID_SPEEX
	}
};



/*
 * arrays with extensions, allowed video and audio codecs for use in
 * the global xvc_formats array
 */
static const XVC_VidCodecId *xwd_allowed_vid_codecs[] = { VID_CODEC_NONE };
static const char *xwd_extensions[] = { "xwd" };

static const XVC_VidCodecId *pgm_allowed_vid_codecs[] = { VID_CODEC_PGM };
static const char *pgm_extensions[] = { "pgm" };

static const XVC_VidCodecId *ppm_allowed_vid_codecs[] = { VID_CODEC_PPM };
static const char *ppm_extensions[] = { "ppm" };

static const XVC_VidCodecId *png_allowed_vid_codecs[] = { VID_CODEC_PNG };
static const char *png_extensions[] = { "png" };

static const XVC_VidCodecId *jpg_allowed_vid_codecs = { VID_CODEC_JPEG };
static const char *jpg_extensions[] = { "jpg", "jpeg" };


static const XVC_VidCodecId *avi_allowed_vid_codecs[] = {
	VID_CODEC_NONE, 
    VID_CODEC_MPEG1,
	VID_CODEC_MJPEG,
    VID_CODEC_MPEG4,
    VID_CODEC_MSDIV2,
    VID_CODEC_MPEG2,
    VID_CODEC_THEORA,
    VID_CODEC_DV,
    VID_CODEC_FFV1
};
static const XVC_AuCodecId *avi_allowed_au_codecs[] = {
	AU_CODEC_NONE,
	AU_CODEC_MP2,
	AU_CODEC_MP3,
    AU_CODEC_VORBIS,
    AU_CODEC_PCM16
};
static const char *avi_extensions[] = { "avi" };


static const XVC_VidCodecId *asf_allowed_vid_codecs[] = {
	VID_CODEC_MSDIV3
};
static const XVC_AuCodecId *asf_allowed_au_codecs[] = {
	AU_CODEC_NONE,
	AU_CODEC_WMA1,
	AU_CODEC_WMA2
};
static const char *asf_extensions[] = { "asf" };


static const XVC_VidCodecId *flv_allowed_vid_codecs[] = {
	VID_CODEC_FLV,
	VID_CODEC_MJPEG,
	VID_CODEC_VP6,
	VID_CODEC_FLASHSV,
	VID_CODEC_H264
};
static const XVC_AuCodecId *flv_allowed_au_codecs[] = {
	AU_CODEC_NONE,
	AU_CODEC_MP3,
    AU_CODEC_PCM16,
	AU_CODEC_SPEEX,
	AU_CODEC_AAC
};
static const char *flv_extensions[] = { "flv" };



static const XVC_VidCodecId *swf_allowed_vid_codecs[] = {
	VID_CODEC_FLV,
	VID_CODEC_MJPEG,
	VID_CODEC_VP6
};
static const XVC_AuCodecId *swf_allowed_au_codecs[] = {
	AU_CODEC_NONE,
	AU_CODEC_MP3
};
static const char *swf_extensions[] = { "swf" };



static const XVC_VidCodecId *dv_allowed_vid_codecs[] = {
	VID_CODEC_DV,
};
static const XVC_AuCodecId *dv_allowed_au_codecs[] = {
    AU_CODEC_NONE,
    AU_CODEC_MP2,
    AU_CODEC_PCM16
};
static const char *dv_extensions[] = { "dv" };



static const XVC_VidCodecId *mpeg1_allowed_vid_codecs[] = {
	VID_CODEC_MPEG1
};
static const XVC_AuCodecId *mpeg1_allowed_au_codecs[] = {
    AU_CODEC_NONE,
    AU_CODEC_MP2
};
static const char *mpeg1_extensions[] = { "m1v", "vcd", "mpeg", "mpg" };



static const XVC_VidCodecId *mpeg2_allowed_vid_codecs[] = {
	VID_CODEC_MPEG2
};
static const XVC_AuCodecId *mpeg2_allowed_au_codecs[] = {
	AU_CODEC_NONE,
	AU_CODEC_MP2,
	AU_CODEC_AC3
};
static const char *mpeg2_extensions[] = { "m2v", "svcd", "mpg", "mpeg" };



static const char *vob_extensions[] = { "vob", "dvd" };



static const XVC_VidCodecId *mov_allowed_vid_codecs[] = {
	VID_CODEC_NONE, 
	VID_CODEC_MPEG4,
    VID_CODEC_SVQ1,
	VID_CODEC_H264,
    VID_CODEC_DV
};
static const XVC_AuCodecId *mov_allowed_au_codecs[] = {
	AU_CODEC_NONE,
    AU_CODEC_PCM16,
	AU_CODEC_MP2,
	AU_CODEC_MP3,
    AU_CODEC_AAC
};
static const char *mov_extensions[] = { "mov", "qt", "mp4" };



static const XVC_VidCodecId *ogg_allowed_vid_codecs[] = {
	VID_CODEC_NONE, 
	VID_CODEC_MPEG4,
	VID_CODEC_H264,
	VID_CODEC_THEORA
};
static const XVC_AuCodecId *ogg_allowed_au_codecs[] = {
	AU_CODEC_NONE,
    AU_CODEC_PCM16,
	AU_CODEC_MP2,
	AU_CODEC_MP3,
	AU_CODEC_VORBIS,
    AU_CODEC_AAC
};
static const char *ogg_extensions[] = { "ogg" };



/**
 * Global array storing all available file formats and containers
 */
const XVC_FFormat xvc_formats[] = {
	// Single image formats
    {
		"xwd",
		N_("X11 Window Dump"),
		NULL,
		VID_CODEC_NONE,
		xwd_allowed_vid_codecs,
		AU_CODEC_NONE,
		NULL,
		xwd_extensions
	},
	{
		"pgm",
		N_("Portable Graymap File"),
		NULL,
		VID_CODEC_PGM,
		pgm_allowed_vid_codecs,
		AU_CODEC_NONE,
		NULL,
		pgm_extensions
	},
    {
		"ppm",
		N_("Portable Anymap File"),
		NULL,
		VID_CODEC_PPM,
		ppm_allowed_vid_codecs,
		AU_CODEC_NONE,
		NULL,
		ppm_extensions
	},
    {
		"png",
		N_("Portable Network Graphics File"),
		NULL,
		VID_CODEC_PNG,
		png_allowed_vid_codecs,
		AU_CODEC_NONE,
		NULL,
		png_extensions
	},
    {
		"jpg",
		N_("Joint Picture Expert Group"),
		NULL,
		VID_CODEC_JPEG,
		png_allowed_vid_codecs,
		AU_CODEC_NONE,
		NULL,
		jpg_extensions
	},
	// Video formats
    {
		"avi",
		N_("Microsoft Audio Video Interleaved File"),
		"avi",
		VID_CODEC_MSDIV2,
		avi_allowed_vid_codecs,
		AU_CODEC_MP3,
		avi_allowed_au_codecs,
		avi_extensions
	},
    {
		"asf",
		N_("Microsoft Advanced Systems Format"),
		"asf",
		VID_CODEC_MSDIV3,
		asf_allowed_vid_codecs,
		AU_CODEC_WMA2,
		asf_allowed_au_codecs,
		asf_extensions
	},
    {
		"flv1",
		N_("Macromedia Flash Video Stream"),
		"flv",
		VID_CODEC_H264,
		swf_allowed_vid_codecs,
		AU_CODEC_MP3,
		swf_allowed_au_codecs,
		swf_extensions
	},
    {
    	"swf",
		N_("Macromedia Shockwave Flash File"),
		"swf",
		VID_CODEC_FLV,
		flv_allowed_vid_codecs,
		AU_CODEC_MP3,
		flv_allowed_au_codecs,
		flv_extensions
	},
    {
		"dv",
		N_("DV Video Format"),
		"dv",
		VID_CODEC_DV,
		dv_allowed_vid_codecs,
		AU_CODEC_MP2,
		dv_allowed_au_codecs,
		dv_extensions
	},
    {
		"mpeg",
		N_("MPEG-1 System Format (VCD)"),
		"mpeg",
		VID_CODEC_MPEG1,
		mpeg1_allowed_vid_codecs,
		AU_CODEC_MP2,
		mpeg1_allowed_au_codecs,
		mpeg1_extensions
	},
    {
		"mpeg2",
		N_("MPEG2 PS Format (SVCD)"),
		"svcd",
		VID_CODEC_MPEG2,
		mpeg2_allowed_vid_codecs,
		AU_CODEC_MP2,
		mpeg2_allowed_au_codecs,
		mpeg2_extensions
	},
    {
		"vob",
		N_("MPEG2 PS format (DVD VOB)"),
		"dvd",
		VID_CODEC_MPEG2,
		mpeg2_allowed_vid_codecs,
		AU_CODEC_AC3,
		mpeg2_allowed_au_codecs,
		vob_extensions
	},
    {
		"mov",
		N_("Quicktime Format"),
		"mov",
		VID_CODEC_MPEG4,
		mov_allowed_vid_codecs,
		AU_CODEC_MP2,
		mov_allowed_au_codecs,
		mov_extensions
	},
    {
		"ogg",
		N_("Ogg Format"),
		"ogg",
		VID_CODEC_THEORA,
		ogg_allowed_vid_codecs,
		AU_CODEC_VORBIS,
		ogg_allowed_au_codecs,
		ogg_extensions,
	}
};

/**
 * \brief finds libavcodec's codec id from xvidcap's
 *
 * @param xv_codec xvidcap's codec id
 * @return an integer codec id as understood by libavcodec. Since 0 is a valid
 *      codec id we return -1 on failure
 */
int
xvc_trans_codec(XVC_VidCodecId xv_codec)
{
    int ret = xvc_video_codecs[xv_codec].ffmpeg_id;
    return ret;
}

/**
 * \brief finds out if a codec is in the an array of valid video codec ids
 *      for a given format
 *
 * @param format the id of the format to check
 * @param codec the codec id to check for
 * @return 0 for not found, otherwise the position in the array where codec
 *      was found started at 1 (i. e. normal index + 1)
 */
int
xvc_is_valid_video_codec (XVC_FFormatId format, XVC_VidCodecId codec)
{
	if (! xvc_is_valid_format(format)) {
		return 0;
	}

	if (xvc_formats[format].allowed_vid_codecs == NULL) {
		return 0;
	}
	
    for (int i = 0; i < (sizeof xvc_formats[format].allowed_vid_codecs / sizeof(XVC_VidCodec)); i++) {
        if (xvc_formats[format].allowed_vid_codecs[i] == codec) {
            return 1;
        }
    }

    return 0;
}

/**
 * \brief finds out if an audio codec is in the an array of valid audio codec
 *      ids for a given format
 *
 * @param format the id of the format to check
 * @param codec the audio codec id to check for
 * @return 0 for not found, otherwise the position in the array where codec
 *      was found started at 1 (i. e. normal index + 1)
 */
int
xvc_is_valid_audio_codec(XVC_FFormatId format, XVC_AuCodecId codec)
{
	if (! xvc_is_valid_format(format)) {
		return 0;
	}

	if (xvc_formats[format].allowed_au_codecs == NULL && codec != AU_CODEC_NONE) {
		return 0;
	}
	
	for (int i = 0; i < (sizeof xvc_formats[format].allowed_au_codecs / sizeof(XVC_AuCodec)); i++) {
        if (xvc_formats[format].allowed_au_codecs[i] == codec) {
            return 1;
        }
    }

	return 0;
}


int
xvc_is_valid_format(XVC_FFormatId format)
{
    if (format <= CAP_XWD) {
		return 0;
	}

	if (format >= (sizeof xvc_formats /sizeof(XVC_FFormat))) {
		return 0;
	}

	return 1;
}	


/**
 * \brief find target file format based on filename, i. e. the extension
 *
 * @param file a filename to test
 * @return the index number pointing to the array element for the format found
 *      in the global file xvc_formats array
 */
XVC_FFormatId
xvc_codec_get_target_from_filename (const char *file)
{
    char *ext = NULL;

    ext = rindex(file, '.');
    if (ext == NULL) {
        return 0;
    }
    ext++;

    for (int i = 0; i < (sizeof(xvc_formats) / sizeof(XVC_FFormat)); i++) {
        for (int j = 0; j < (sizeof(xvc_formats[i].extensions) / sizeof(char)); j++) {
			if (strcasecmp (ext, xvc_formats[i].extensions[j]) == 0) {
                return j;
            }
        }
    }

    return 0;
}


/**
 * \/**
 * \brief checks if fps rate is valid for given codec
 *
 * @param fps the fps rate to test
 * @param codec the codec to test the fps rate against specified as an index
 *      number pointing to a codec in the global codecs array
 * @param exact if 1 the function does an exact check, if 0 it will accept
 *      an fps value as valid, if there is a valid fps value that is
 *      not more than 0.01 larger or smaller than the fps given
 * @return 1 for fps is valid for given codec or 0 for not valid
 */
int
xvc_codec_is_valid_fps (XVC_Fps fps, XVC_VidCodecId codec, int exact)
{
    return 1;
}


/**
 * \brief reads a string and returns an fps value
 *
 * This accepts two forms of specifying an fps value: Either as a floating
 * point number with locale specific decimal point, or as a fraction like
 * "int/int"
 *
 * @param fps_string a string representation of an fps value
 * @return the fps value read from the string or { 0, 1}
 */
XVC_Fps
xvc_read_fps_from_string (const char *fps_string)
{
    struct lconv *loc = localeconv ();
    XVC_Fps fps = { 0, 1 };

    if ((index (fps_string, '/') &&
         !strstr (fps_string, loc->decimal_point)) ||
        (index (fps_string, '/') && strstr (fps_string, loc->decimal_point) &&
         index (fps_string, '/') < strstr (fps_string, loc->decimal_point))
        ) {
        char *slash, *endptr;

        fps.num = strtol (fps_string, &endptr, 10);
        slash = index (fps_string, '/');
        slash++;
        fps.den = strtol (slash, &endptr, 10);
    } else {
        char *endptr = NULL;

        fps.num = (int) (strtod (fps_string, &endptr) * pow (10,
                                                             xvc_get_number_of_fraction_digits_from_float_string
                                                             (fps_string)) +
                         0.5);
        fps.den =
            (int) (pow
                   (10,
                    xvc_get_number_of_fraction_digits_from_float_string
                    (fps_string)) + 0.5);
    }
    return fps;
}


int xvc_count_audio_codecs()
{
	return sizeof(xvc_audio_codecs) / sizeof(XVC_AuCodec);
}

int xvc_count_video_codecs()
{
	return sizeof(xvc_video_codecs) / sizeof(XVC_VidCodec);
}

int xvc_count_formats()
{
	return sizeof(xvc_formats) / sizeof(XVC_FFormat);
}
