/**
 * Contains all data types and functions related to video and audio
 * codecs and file formats.
 */

/*
 * Copyright (C) 2003-2007 Karl H. Beckers, Frankfurt
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

#ifndef _xvc_CODECS_H__
#define _xvc_CODECS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * \brief ffmpeg does fractional fps handling through AVRational structs
 *      which look exactly like this. Because we cannot otherwise hope to
 *      exactly match ffmpeg framerates with a float of our own, we mimic
 *      the AVRational struct ... and copy it because we might build without
 *      ffmpeg
 */
typedef struct _xvc_Fps
{
    int num;
    int den;
} XVC_Fps;


/**
 * \brief some codecs support ranges of framerates. The GUI needs to know
 *      about those, so we wrap the XVC_Fps struct in XVC_FpsRange structs
 *      with a defined start and end
 */
typedef struct _xvc_FpsRange
{
    XVC_Fps start;
    XVC_Fps end;
} XVC_FpsRange;


/**
 * Codec IDs used by xvidcap. Because these are in the same order as
 * the elements of the global xvc_video_codecs array, the elements of the array
 * can be referenced using these telling names.
 */
typedef enum
{
    VID_CODEC_NONE,
    VID_CODEC_PGM,
    VID_CODEC_PPM,
    VID_CODEC_PNG,
    VID_CODEC_JPEG,
    VID_CODEC_MPEG1,
    VID_CODEC_MJPEG,
    VID_CODEC_MPEG4,
    VID_CODEC_MSDIV2,
    VID_CODEC_MSDIV3,
    VID_CODEC_FFV1,
    VID_CODEC_FLV,
    VID_CODEC_FLASHSV,
    VID_CODEC_FLASHSV2,
    VID_CODEC_DV,
    VID_CODEC_MPEG2,
    VID_CODEC_THEORA,
    VID_CODEC_SVQ1,
	VID_CODEC_VP6,
	VID_CODEC_H264
} XVC_VidCodecId;

/**
 * Struct containing codec properties.
 */
typedef struct _xvc_Codec
{
    const char *name;
    const char *longname;
    const int ffmpeg_id;
} XVC_VidCodec;

extern const XVC_VidCodec xvc_video_codecs[];

/**
 * Audio codec IDs used by xvidcap. Because these are in the same
 * order as the elements of the global xvc_audio_codecs array, the
 * elements of the array can be referenced using these telling names.
 */
typedef enum
{
    AU_CODEC_NONE,
    AU_CODEC_MP2,
    AU_CODEC_MP3,
    AU_CODEC_VORBIS,
    AU_CODEC_AC3,
	AU_CODEC_PCM16,
	AU_CODEC_WMA1,
	AU_CODEC_WMA2,
	AU_CODEC_AAC,
	AU_CODEC_SPEEX
} XVC_AuCodecId;

/**
 * Struct containing audio codec properties.
 */
typedef struct _xvc_AuCodec
{
    const char *name;
    const char *longname;
    const int ffmpeg_id;
} XVC_AuCodec;

extern const XVC_AuCodec xvc_audio_codecs[];

/**
 * File format IDs used by xvidcap. Because these are in the same
 * order as the elements of the global xvc_formats array, the elements of
 * the array can be referenced using these telling names.
 */
typedef enum
{
    CAP_XWD,
    CAP_PGM,
    CAP_PPM,
    CAP_PNG,
    CAP_JPG,
    CAP_AVI,
    CAP_DIVX,
    CAP_ASF,
    CAP_FLV,
    CAP_SWF,
    CAP_DV,
    CAP_MPG,
    CAP_SVCD,
    CAP_DVD,
    CAP_MOV,
    CAP_OGG
} XVC_FFormatId;


/**
 * Struct containing file format properties.
 */
typedef struct _xvc_FFormat
{
    const char *name;
    const char *longname;
    const char *ffmpeg_name;
    const XVC_VidCodecId def_vid_codec;
    const XVC_VidCodecId *allowed_vid_codecs;
    const XVC_AuCodecId def_au_codec;
    const XVC_AuCodecId *allowed_au_codecs;
    const char *extensions;
} XVC_FFormat;

extern const XVC_FFormat xvc_formats[];



int xvc_trans_codec(XVC_VidCodecId xv_codec);

int xvc_is_valid_video_codec(XVC_FFormatId format, XVC_VidCodecId codec);


int xvc_is_valid_audio_codec(XVC_FFormatId format, XVC_AuCodecId codec);

int xvc_is_valid_format(XVC_FFormatId format);

XVC_FFormatId xvc_codec_get_target_from_filename(const char *file);

int xvc_codec_is_valid_fps(XVC_Fps fps, XVC_VidCodecId codec, int exact);

int xvc_get_index_of_fps_array_element(int size, const XVC_Fps *haystack, XVC_Fps needle, int exact);

XVC_Fps xvc_read_fps_from_string(const char *fps_string);

int xvc_count_audio_codecs();

int xvc_count_video_codecs();

int xvc_count_formats();

#endif     // _xvc_CODECS_H__
