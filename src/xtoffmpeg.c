/**
 * \file xtoffmpeg.c
 *
 * This file contains the functions for encoding captured frames on-the-fly
 * using libavcodec/-format
 */
/*
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
 * EMail: khb@jarre-de-the.net
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif     // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <X11/Intrinsic.h>
#include <pthread.h>
#include <signal.h>

// xvidcap specific
#include "app_data.h"
#include "job.h"
#include "colors.h"
#include "frame.h"
#include "codecs.h"
#include "xvidcap-intl.h"

// ffmpeg stuff
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/fifo.h>
#define swscale_isRGB(x) ((x)==PIX_FMT_BGR32 || (x)==PIX_FMT_RGB24 \
                        || (x)==PIX_FMT_RGB565 || (x)==PIX_FMT_RGB555 \
                        || (x)==PIX_FMT_RGB8 || (x)==PIX_FMT_RGB4 \
                        || (x)==PIX_FMT_RGB4_BYTE || (x)==PIX_FMT_MONOBLACK)
#define swscale_isBGR(x) ((x)==PIX_FMT_RGB32 || (x)==PIX_FMT_BGR24 \
                        || (x)==PIX_FMT_BGR565 || (x)==PIX_FMT_BGR555 \
                        || (x)==PIX_FMT_BGR8 || (x)==PIX_FMT_BGR4 \
                        || (x)==PIX_FMT_BGR4_BYTE || (x)==PIX_FMT_MONOBLACK)
#define swscale_isSupportedIn(x) ((x)==PIX_FMT_YUV420P || (x)==PIX_FMT_YUYV422 \
                        || (x)==PIX_FMT_UYVY422 || (x)==PIX_FMT_RGB32 \
                        || (x)==PIX_FMT_BGR24 || (x)==PIX_FMT_BGR565 \
                        || (x)==PIX_FMT_BGR555 || (x)==PIX_FMT_BGR32 \
                        || (x)==PIX_FMT_RGB24|| (x)==PIX_FMT_RGB565 \
                        || (x)==PIX_FMT_RGB555 || (x)==PIX_FMT_GRAY8 \
                        || (x)==PIX_FMT_YUV410P || (x)==PIX_FMT_GRAY16BE \
                        || (x)==PIX_FMT_GRAY16LE || (x)==PIX_FMT_YUV444P \
                        || (x)==PIX_FMT_YUV422P || (x)==PIX_FMT_YUV411P \
                        || (x)==PIX_FMT_PAL8 || (x)==PIX_FMT_BGR8 \
                        || (x)==PIX_FMT_RGB8 || (x)==PIX_FMT_BGR4_BYTE \
                        || (x)==PIX_FMT_RGB4_BYTE)
// added jpeg stuff myself (yuvj*) because swscale actually DOES accept them
#define swscale_isSupportedOut(x) ((x)==PIX_FMT_YUV420P \
                        || (x)==PIX_FMT_YUYV422 || (x)==PIX_FMT_UYVY422 \
                        || (x)==PIX_FMT_YUV444P || (x)==PIX_FMT_YUV422P \
                        || (x)==PIX_FMT_YUV411P || swscale_isRGB(x) \
                        || swscale_isBGR(x) || (x)==PIX_FMT_NV12 \
                        || (x)==PIX_FMT_NV21 || (x)==PIX_FMT_GRAY16BE \
                        || (x)==PIX_FMT_GRAY16LE || (x)==PIX_FMT_GRAY8 \
                        || (x)==PIX_FMT_YUV410P \
                        || (x)==PIX_FMT_YUVJ420P || (x)==PIX_FMT_YUVJ422P \
                        || (x)==PIX_FMT_YUVJ444P)

#define PIX_FMT_ARGB32 PIX_FMT_RGB32   /* this is just my personal convenience */

#define MAX_AUDIO_PACKET_SIZE (128 * 1024)



/*
 * file globals
 */
/** \brief params for the codecs video */
static AVCodec *codec;

/** \brief an AVFrame as wrapper around the original image data */
static AVFrame *p_inpic;

/** \brief and one for the image converted to yuv420p */
static AVFrame *p_outpic;

/** \brief data buffer for output frame */
static uint8_t *outpic_buf;

/** \brief output buffer for encoded frame */
static uint8_t *outbuf;

/** \brief the size of the outbuf may be other than image_size */
static int outbuf_size;

/** \brief output file via avformat */
static AVFormatContext *output_file;

/** \brief ... plus related data */
static AVOutputFormat *file_oformat;
static AVStream *out_st = NULL;

/** \brief context for image resampling */
static struct SwsContext *img_resample_ctx;

/** \brief size of yuv image */
static int image_size;

/** \brief pix_fmt of original image */
static int input_pixfmt;

/** \brief store current video_pts for a/v sync */
static double video_pts;

/** \brief buffer memory used during 8bit palette conversion */
static uint8_t *scratchbuf8bit;

/** \brief pointer to the XVC_CapTypeOptions representing the currently
 * active capture mode (which certainly is mf here) */
static XVC_CapTypeOptions *target = NULL;


// FIXME: check if this all needs to be static global
/** \brief audio codec */
static AVCodec *au_codec = NULL;

/** \brief audio codec context */
static AVCodecContext *au_c = NULL;

/** \brief format context for audio input */
static AVFormatContext *ic = NULL;

typedef struct AVOutputStream {
    int file_index;          /* file index */
    int index;               /* stream index in the output file */
    int source_index;        /* AVInputStream index */
    AVStream *st;            /* stream in the output file */
    int encoding_needed;     /* true if encoding needed for this stream */
    int frame_number;
    /* input pts and corresponding output pts
       for A/V sync */
    //double sync_ipts;        /* dts from the AVPacket of the demuxer in second units */
    struct AVInputStream *sync_ist; /* input stream to sync against */
    int64_t sync_opts;       /* output frame counter, could be changed to some true timestamp */ //FIXME look at frame_number
    /* video only */
    int video_resample;
    AVFrame pict_tmp;      /* temporary image for resampling */
    struct SwsContext *img_resample_ctx; /* for image resampling */
    int resample_height;
    int resample_width;
    int resample_pix_fmt;

    /* full frame size of first frame */
    int original_height;
    int original_width;

    /* cropping area sizes */
    int video_crop;
    int topBand;
    int bottomBand;
    int leftBand;
    int rightBand;

    /* cropping area of first frame */
    int original_topBand;
    int original_bottomBand;
    int original_leftBand;
    int original_rightBand;

    /* padding area sizes */
    int video_pad;
    int padtop;
    int padbottom;
    int padleft;
    int padright;

    /* audio only */
    int audio_resample;
    ReSampleContext *resample; /* for audio resampling */
    int reformat_pair;
    AVFifoBuffer *fifo;     /* for compression: one audio fifo per codec */
    FILE *logfile;
} AVOutputStream;


/** \brief audio output stream */
static AVOutputStream *au_out_st = NULL;


typedef struct AVInputStream {
    int file_index;
    int index;
    AVStream *st;
    int discard;             /* true if stream data should be discarded */
    int decoding_needed;     /* true if the packets must be decoded in 'raw_fifo' */
    int64_t sample_index;      /* current sample */

    int64_t       start;     /* time when read started */
    int64_t       next_pts;  /* synthetic pts for cases where pkt.pts
                                is not defined */
    int64_t       pts;       /* current pts */
    int is_start;            /* is 1 at the start and after a discontinuity */
} AVInputStream;


/** \brief audio input stream */
static AVInputStream *au_in_st = NULL;

/** \brief buffer used during audio capture */
static uint8_t *audio_buf = NULL;

/** \brief buffer used during audio encoding */
static uint8_t *audio_out = NULL;

/** \brief thread coordination variables for interleaving audio and video
 *      capture. This is the thread's attributes */
static pthread_attr_t tattr;

/** \brief thread coordination variables for interleaving audio and video
 *      capture. This is the mutex lock */
static pthread_mutex_t mp = PTHREAD_MUTEX_INITIALIZER;

/** \brief thread coordination variables for interleaving audio and video
 *      capture. This is the thread's id */
static pthread_t tid = 0;

static int audio_thread_running = FALSE;

/** \brief store current audio_pts for a/v sync */
static double audio_pts;

/*
 * functions ...
 *
 */

/**
 * \brief adds an audio stream to AVFormatContext output_file
 *
 * @param job the current job
 * @return 0 on success or smth. else on failure
 */
static int
add_audio_stream (Job * job)
{
    AVInputFormat *grab_iformat = NULL;
    Boolean grab_audio = TRUE;
    AVFormatParameters params, *ap = &params;   // audio stream params
    int err, ret;

    if (!strcmp (job->snd_device, "-")) {
        job->snd_device = "pipe:";
        grab_audio = FALSE;
    } else {
        grab_audio = TRUE;
    }

    // prepare input stream
    memset (ap, 0, sizeof (*ap));
//    ap->device = job->snd_device;

    if (grab_audio) {
        ap->sample_rate = target->sndrate;
        ap->channels = target->sndchannels;

        grab_iformat = av_find_input_format ("oss");
    }

    err =
        av_open_input_file (&ic, job->snd_device,
                            (grab_audio ? grab_iformat : NULL), 0, ap);
    if (err < 0) {
        fprintf (stderr, _("error opening input file %s: %i\n"), job->snd_device, err);
        return 1;
    }
    au_in_st = av_mallocz (sizeof (AVInputStream));
    if (!au_in_st || !ic) {
        fprintf (stderr, _("Could not alloc input stream ... aborting\n"));
        return 1;
    }
    au_in_st->st = ic->streams[0];

    // If not enough info to get the stream parameters, we decode
    // the first frames to get it. (used in mpeg case for example)
    ret = av_find_stream_info (ic);
    if (ret < 0) {
        fprintf (stderr, _("could not find codec parameters\n"));
        if (au_in_st) {
            av_free (au_in_st);
            au_in_st = NULL;
        }
        return 1;
    }
    // init pts stuff
    au_in_st->next_pts = 0;
    au_in_st->is_start = 1;

    // OUTPUT
    // setup output codec
    au_c = avcodec_alloc_context ();
    if (!au_c) {
        fprintf (stderr, _("Could not allocate audio output codec context\n"));
        if (au_in_st) {
            av_free (au_in_st);
            au_in_st = NULL;
        }
        return 1;
    }
    // put sample parameters
    au_c->codec_id = xvc_audio_codecs[job->au_targetCodec].ffmpeg_id;
    au_c->codec_type = CODEC_TYPE_AUDIO;
    au_c->bit_rate = target->sndsize;
    au_c->sample_rate = target->sndrate;
    au_c->channels = target->sndchannels;
    // au_c->debug = 0x00000FFF;

    // prepare output stream
    au_out_st = av_mallocz (sizeof (AVOutputStream));
    if (!au_out_st) {
        fprintf (stderr, _("Could not alloc stream ... aborting\n"));
        if (au_in_st) {
            av_free (au_in_st);
            au_in_st = NULL;
        }
        return 1;
    }
    au_out_st->st = av_new_stream (output_file, 1);
    if (!au_out_st->st) {
        fprintf (stderr, _("Could not alloc stream\n"));
        if (au_in_st) {
            av_free (au_in_st);
            au_in_st = NULL;
        }
        return 1;
    }
    au_out_st->st->codec = au_c;

	au_out_st->fifo = av_fifo_alloc(2 * MAX_AUDIO_PACKET_SIZE);
    if (! au_out_st->fifo) {
        fprintf(stderr, _("Can't initialize fifo for audio recording\n"));
        if (au_in_st) {
            av_free (au_in_st);
            au_in_st = NULL;
        }
        return 1;
    }
    // This bit is important for inputs other than self-sampled.
    // The sample rates and no of channels a user asks for
    // are the ones he/she wants in the encoded mpeg. For self-
    // sampled audio, these are also the values used for sampling.
    // Hence there is no resampling necessary (case 1).
    // When dubbing from a pipe or a different
    // file, we might have different sample rates or no of
    // channels
    // in the input file.....
    if (au_c->channels == au_in_st->st->codec->channels &&
        au_c->sample_rate == au_in_st->st->codec->sample_rate) {
        au_out_st->audio_resample = 0;
    } else {
        if (au_c->channels != au_in_st->st->codec->channels &&
            au_in_st->st->codec->codec_id == CODEC_ID_AC3) {
            // Special case for 5:1 AC3 input
            // and mono or stereo output
            // Request specific number of channels
            au_in_st->st->codec->channels = au_c->channels;
            if (au_c->sample_rate == au_in_st->st->codec->sample_rate)
                au_out_st->audio_resample = 0;
            else {
                au_out_st->audio_resample = 1;
                au_out_st->resample =
                    audio_resample_init (au_c->channels,
                                         au_in_st->st->codec->
                                         channels,
                                         au_c->sample_rate,
                                         au_in_st->st->codec->sample_rate);
                if (!au_out_st->resample) {
                    fprintf(stderr, _("%s %s: Can't resample. Aborting.\n"));
                    if (au_in_st) {
                        av_free (au_in_st);
                        au_in_st = NULL;
                    }
                    return 1;
                }
            }
            // Request specific number of channels
            au_in_st->st->codec->channels = au_c->channels;
        } else {
            au_out_st->audio_resample = 1;
            au_out_st->resample =
                audio_resample_init (au_c->channels,
                                     au_in_st->st->codec->channels,
                                     au_c->sample_rate,
                                     au_in_st->st->codec->sample_rate);
            if (!au_out_st->resample) {
                fprintf(stderr, _("%s %s: Can't resample. Aborting.\n"));
                if (au_in_st) {
                    av_free (au_in_st);
                    au_in_st = NULL;
                }
                return 1;
            }
        }
    }
    au_in_st->decoding_needed = 1;
    au_out_st->encoding_needed = 1;

    // open encoder
    au_codec = avcodec_find_encoder (au_out_st->st->codec->codec_id);
    if (avcodec_open (au_out_st->st->codec, au_codec) < 0) {
        fprintf(stderr,_("Error while opening codec for output stream\n"));
        if (au_in_st) {
            av_free (au_in_st);
            au_in_st = NULL;
        }
        return 1;
    }
    // open decoder
    au_codec = avcodec_find_decoder (ic->streams[0]->codec->codec_id);
    if (!au_codec) {
        fprintf(stderr, _("Unsupported codec (id=%d) for input stream\n"), ic->streams[0]->codec->codec_id);
        if (au_in_st) {
            av_free (au_in_st);
            au_in_st = NULL;
        }
        return 1;
    }
    if (avcodec_open (ic->streams[0]->codec, au_codec) < 0) {
        fprintf(stderr, _("Error while opening codec for input stream\n"));
        if (au_in_st) {
            av_free (au_in_st);
            au_in_st = NULL;
        }
        return 1;
    }
    return 0;
}

/**
 * \brief encode and write audio samples
 *
 * @param s output format context (output_file)
 * @param ost pointer to audio output stream
 * @param ist input stream information
 * @param buf the actual data sampled
 * @param size the size of the data sampled
 */
static void
do_audio_out (AVFormatContext * s, AVOutputStream * ost,
              const AVInputStream * ist, unsigned char *buf, int size)
{
    uint8_t *buftmp;
    const int audio_out_size = 4 * MAX_AUDIO_PACKET_SIZE;
    int size_out, frame_bytes;
    AVCodecContext *enc;

    // SC: dynamic allocation of buffers
    if (!audio_buf)
        audio_buf = av_malloc (2 * MAX_AUDIO_PACKET_SIZE);
    if (!audio_out)
        audio_out = av_malloc (audio_out_size);
    if (!audio_out || !audio_buf)
        return;                        // Should signal an error !

    enc = ost->st->codec;

    // resampling is only used for pipe input here
    if (ost->audio_resample) {
        buftmp = audio_buf;
        size_out =
            audio_resample (ost->resample, (short *) buftmp, (short *) buf,
                            size / (ist->st->codec->channels * 2));
        size_out = size_out * enc->channels * 2;
    } else {
        buftmp = buf;
        size_out = size;
    }

    // now encode as many frames as possible
    if (enc->frame_size > 1) {
        // output resampled raw samples
        av_fifo_generic_write (&ost->fifo, buftmp, size_out, NULL);
        frame_bytes = enc->frame_size * 2 * enc->channels;

        while (av_fifo_generic_read (&ost->fifo, audio_buf, frame_bytes, NULL) == 0) {
            AVPacket pkt;

            // initialize audio output packet
            av_init_packet (&pkt);

            pkt.size =
                avcodec_encode_audio (enc, audio_out, size_out,
                                      (short *) audio_buf);

            if (enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE) {
                pkt.pts =
                    av_rescale_q (enc->coded_frame->pts, enc->time_base,
                                  ost->st->time_base);
            }
            pkt.flags |= PKT_FLAG_KEY;
            pkt.stream_index = ost->st->index;

            pkt.data = audio_out;
            if (pthread_mutex_trylock (&mp) == 0) {
                // write the compressed frame in the media file
                if (av_interleaved_write_frame (s, &pkt) != 0) {
                    fprintf(stderr, _("Error while writing audio frame\n"));
                    // exit (1);
                    return;
                }

                if (pthread_mutex_unlock (&mp) > 0) {
                    fprintf(stderr, _("Couldn't unlock mutex lock for writing audio frame\n"));
                }
            }
        }
    } else {
        AVPacket pkt;

        av_init_packet (&pkt);

        /* output a pcm frame */
        /* XXX: change encoding codec API to avoid this ? */
        switch (enc->codec->id) {
        case CODEC_ID_PCM_S32LE:
        case CODEC_ID_PCM_S32BE:
        case CODEC_ID_PCM_U32LE:
        case CODEC_ID_PCM_U32BE:
            size_out = size_out << 1;
            break;
        case CODEC_ID_PCM_S24LE:
        case CODEC_ID_PCM_S24BE:
        case CODEC_ID_PCM_U24LE:
        case CODEC_ID_PCM_U24BE:
        case CODEC_ID_PCM_S24DAUD:
            size_out = size_out / 2 * 3;
            break;
        case CODEC_ID_PCM_S16LE:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_PCM_U16LE:
        case CODEC_ID_PCM_U16BE:
            break;
        default:
            size_out = size_out >> 1;
            break;
        }
        pkt.size =
            avcodec_encode_audio (enc, audio_out, size_out, (short *) buftmp);
        pkt.stream_index = ost->st->index;
        pkt.data = audio_out;
        if (enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
            pkt.pts =
                av_rescale_q (enc->coded_frame->pts, enc->time_base,
                              ost->st->time_base);
        pkt.flags |= PKT_FLAG_KEY;
        av_interleaved_write_frame (s, &pkt);
    }
}

/**
 * \brief signal handler for stopping the audio capture thread which
 *      runs till sent a SIGUSR1 signal
 */
static void
cleanup_thread_when_stopped ()
{
    int ret;
    AVPacket pkt;
    int fifo_bytes;
    AVCodecContext *enc;
    int bit_buffer_size = 1024 * 256;
    uint8_t *bit_buffer = NULL;
    short *samples = NULL;


    av_init_packet (&pkt);
//    pkt.stream_index= ost->index;

    enc = au_out_st->st->codec;
    samples = av_malloc (AVCODEC_MAX_AUDIO_FRAME_SIZE);
    bit_buffer = av_malloc (bit_buffer_size);
    fifo_bytes = av_fifo_size (&au_out_st->fifo);
    ret = 0;

    /* encode any samples remaining in fifo */
    if (fifo_bytes > 0 &&
        enc->codec->capabilities & CODEC_CAP_SMALL_LAST_FRAME &&
        bit_buffer && samples) {
        int fs_tmp = enc->frame_size;

        enc->frame_size = fifo_bytes / (2 * enc->channels);
        if (av_fifo_generic_read (&au_out_st->fifo, (uint8_t *) samples, fifo_bytes, NULL) ==
            0) {
            ret =
                avcodec_encode_audio (enc, bit_buffer, bit_buffer_size,
                                      samples);
        }
        enc->frame_size = fs_tmp;
    }
    if (ret <= 0) {
        ret = avcodec_encode_audio (enc, bit_buffer, bit_buffer_size, NULL);
    }
    pkt.flags |= PKT_FLAG_KEY;

    if (samples) {
        av_free (samples);
        samples = NULL;
    }
    if (bit_buffer) {
        av_free (bit_buffer);
        bit_buffer = NULL;
    }
    if (audio_out) {
        av_free (audio_out);
        audio_out = NULL;
    }
    if (audio_buf) {
        av_free (audio_buf);
        audio_buf = NULL;
    }
    if (au_in_st) {
        av_free (au_in_st);
        au_in_st = NULL;
    }

    av_close_input_file (ic);


    audio_thread_running = FALSE;
    pthread_exit (&ret);
}

/**
 * \brief this function implements the thread doing the audio capture and
 *      interleaving the captured audio frames with the video output
 *
 * @param job the current job
 */
static void
capture_audio_thread (Job * job)
{
    XVC_AppData *app = xvc_appdata_ptr ();
    unsigned long start, stop, start_s, stop_s;
    struct timeval thr_curr_time;
    long sleep;
    int ret, len, data_size;
    uint8_t *ptr, *data_buf;
    static unsigned int samples_size = 0;
    static short *samples = NULL;
    AVPacket pkt;

    audio_thread_running = TRUE;
    signal (SIGUSR1, cleanup_thread_when_stopped);

    while (TRUE) {
        // get start time
        gettimeofday (&thr_curr_time, NULL);
        start_s = thr_curr_time.tv_sec;
        start = thr_curr_time.tv_usec;

        if ((job->state & VC_PAUSE) && !(job->state & VC_STEP)) {
            pthread_mutex_lock (&(app->recording_paused_mutex));
            pthread_cond_wait (&(app->recording_condition_unpaused),
                               &(app->recording_paused_mutex));
            pthread_mutex_unlock (&(app->recording_paused_mutex));
        } else if (job->state == VC_REC) {

            audio_pts = (double)
                au_out_st->st->pts.val *
                au_out_st->st->time_base.num / au_out_st->st->time_base.den;
            video_pts =
                (double) out_st->pts.val * out_st->time_base.num /
                out_st->time_base.den;

            // sometimes we need to pause writing audio packets for a/v
            // sync (when audio_pts >= video_pts)
            // now, if we're reading from a file/pipe, we stop sampling or
            // else the audio track in the video would become choppy (packets
            // missing where they were read but not written)
            // for real-time sampling we can't do that because otherwise
            // the input packets queue up and will eventually be sampled
            // (only later) and lead to out-of-sync audio (video faster)
            if (audio_pts < video_pts) {
                // read a packet from it and output it in the fifo
                if (av_read_frame (ic, &pkt) < 0) {
                    fprintf(stderr, _("%s %s: error reading audio packet\n"));
                }
                len = pkt.size;
                ptr = pkt.data;
                while (len > 0) {
                    // decode the packet if needed
                    data_buf = NULL;   /* fail safe */
                    data_size = 0;

                    if (au_in_st->decoding_needed) {
                        samples = av_fast_realloc (samples, &samples_size,
                                                   FFMAX (pkt.size,
                                                          AVCODEC_MAX_AUDIO_FRAME_SIZE));
                        data_size = samples_size;
                        /* XXX: could avoid copy if PCM 16 bits with same
                         * endianness as CPU */
                        ret =
                            avcodec_decode_audio2 (au_in_st->st->codec, samples,
                                                   &data_size, ptr, len);
                        if (ret < 0) {
                            fprintf(stderr, _("Couldn't decode captured audio packet\n"));
                            break;
                        }
                        ptr += ret;
                        len -= ret;
                        /* Some bug in mpeg audio decoder gives */
                        /* data_size < 0, it seems they are overflows */
                        if (data_size <= 0) {
                            /* no audio frame */
                            fprintf(stderr, _("No audio frame\n"));
                            continue;
                        }
                        data_buf = (uint8_t *) samples;
                        au_in_st->next_pts +=
                            ((int64_t) AV_TIME_BASE / 2 * data_size) /
                            (au_in_st->st->codec->sample_rate *
                             au_in_st->st->codec->channels);
                    } else {
                        // FIXME: dunno about the following
                        au_in_st->next_pts +=
                            ((int64_t) AV_TIME_BASE *
                             au_in_st->st->codec->frame_size) /
                            (au_in_st->st->codec->sample_rate *
                             au_in_st->st->codec->channels);
                        data_buf = ptr;
                        data_size = len;
                        ret = len;
                        len = 0;
                    }

                    do_audio_out (output_file, au_out_st, au_in_st,
                                  data_buf, data_size);
                }
                // discard packet
                av_free_packet (&pkt);
            }                          // end outside if pts ...
            else {
                if (strcmp (job->snd_device, "pipe:") < 0)
                    if (av_read_frame (ic, &pkt) < 0) {
                        fprintf (stderr, _("error reading audio packet\n"));
                    }
                printf (_("Dropping audio frame %f %f\n"), audio_pts, video_pts);
            }
        }                              // end if VC_REC
        // get end time
        gettimeofday (&thr_curr_time, NULL);
        stop_s = thr_curr_time.tv_sec;
        stop = thr_curr_time.tv_usec;
        stop += ((stop_s - start_s) * 1000000);
        sleep = (1000000 / target->sndrate) - (stop - start);
        /**
         * \todo perhaps this whole stuff is irrelevant. Haven't really
         *      seen a situation where the encoding was faster than the time
         *      needed for a decent frame-rate. need to look into more
         *      details about audio/video sync in libavcodec/-format
         *      the strange thing, however is: If I leave away the getting of
         *      start/end time and the usleep stuff, normal audio capture
         *      works, piped audio doesn't *BLINK*
         */
        if (sleep < 0)
            sleep = 0;

        usleep (sleep);
    }                                  // end while(TRUE) loop
    ret = 1;
    audio_thread_running = FALSE;
    pthread_exit (&ret);
}


/**
 * \brief write encoded video data
 *
 * @param s pointer to format context (output_file)
 * @param ost video output stream
 * @param buf buffer with actual data
 * @param size size of encoded data
 */
static void
do_video_out (AVFormatContext * s, AVStream * ost, unsigned char *buf, int size)
{
    AVCodecContext *enc;
    AVPacket pkt;

	enc = ost->codec;

    // initialize video output packet
    av_init_packet (&pkt);

    if (enc->coded_frame) {
        if (enc->coded_frame->pts != AV_NOPTS_VALUE) {
            pkt.pts =
                av_rescale_q (enc->coded_frame->pts, enc->time_base,
                              ost->time_base);
        }
        if (enc->coded_frame->key_frame)
            pkt.flags |= PKT_FLAG_KEY;
    }

    pkt.stream_index = ost->index;
    pkt.data = buf;
    pkt.size = size;

    if (av_interleaved_write_frame (s, &pkt) != 0) {
        fprintf (stderr, _("Error while writing video frame\n"));
        // exit (1);
        return;
    }
}

/**
 * \brief convert bgra32 to rgba32
 *
 * needed on Solaris/SPARC because the ffmpeg version used doesn't know
 * PIX_FMT_ABGR32, i.e. byte ordering is not taken care of
 * @param image the XImage to convert
 */
static void
myABGR32toARGB32 (XImage * image)
{
    char *pdata, *counter;


    pdata = image->data;

    for (counter = pdata;
         counter < (pdata + (image->width * image->height * 4)); counter += 4) {
        char swap;

        if (image->byte_order) {       // MSBFirst has order argb -> abgr
            // = rgba32
            swap = *(counter + 1);
            *(counter + 1) = *(counter + 3);
            *(counter + 3) = swap;
        } else {                       // LSBFirst has order bgra -> rgba
            swap = *counter;
            *counter = *(counter + 2);
            *(counter + 2) = swap;
        }
    }

}

/**
 * \brief convert pal8 to rgba32
 *
 * libswscale does not support pal8 input atm
 * @param image the XImage to convert
 * @param p_inpic pointer to a frame to where the converted output is written
 * @param job the current job
 * \todo very current ffmpeg versions seem to make this unneccessary
 */
static void
myPAL8toRGB24 (XImage * image, AVFrame * p_inpic, Job * job)
{
    u_int32_t *color_table = job->color_table;
    int y = 0, x = 0;
    uint8_t *out_cursor = NULL;
    uint8_t *out = (uint8_t *) p_inpic->data[0];

    /**
     * 8bit pseudo-color images may have lines padded by excess bytes
     * these need to be removed before conversion
     * \todo other formats might also have this problem
     */
    for (y = 0; y < image->height; y++) {
        out_cursor = (uint8_t *) image->data + (y * image->bytes_per_line);
        for (x = 0; x < image->width; x++) {
            *out++ = ((color_table[*out_cursor] & 0x00FF0000) >> 16);
            *out++ = ((color_table[*out_cursor] & 0x0000FF00) >> 8);
            *out++ = (color_table[*out_cursor] & 0x000000FF);
            out_cursor++;
        }
    }
}

/**
 * \brief prepare the color table for pseudo color input to libavcodec's
 *      imgconvert
 *
 * I'm downsampling the 16 bit color entries to 8 bit as it expects
 * 32 bit (=4 * 8) from looking at imgconvert_template.c I'd say libavcodec
 * expects argb logically, not byte-wise
 * @param colors a pointer to the colors as contained in the captured XImage
 * @param ncolors the number of colors present
 * @return a pointer to the converted color table
 */
u_int32_t *
xvc_ffmpeg_get_color_table (XColor * colors, int ncolors)
{
    u_int32_t *color_table, *pixel;
    int i; // , n;


    color_table = malloc (256 * 4);
    if (!color_table)
        return (NULL);

    pixel = color_table;
    for (i = 0; i < ncolors; i++) {
        u_int32_t color_table_entry, swap;

        color_table_entry = 0;
        // alpha alway zero

        swap = colors[i].red;
        swap &= 0x0000FF00;            // color is 16 bits, delete ls 8 bits
        swap <<= 8;                    // then shift ms 8 bits into position
        color_table_entry = (color_table_entry | swap);

        swap = colors[i].green;
        swap &= 0x0000FF00;            /* color is 16 bits, ms 8 bits already
                                        * in position, delete ls 8 bits */
        color_table_entry = (color_table_entry | swap);

        swap = colors[i].blue;
        swap >>= 8;
        color_table_entry = (color_table_entry | swap);

        *pixel = color_table_entry;
        pixel++;
    }

    return color_table;
}

/**
 * \brief prepare the output file or pipe
 *
 * need to determine which first and do things like making the filename
 * absolute etc.
 * @param jFileName the filename as contained in the current job
 * @param oc output format context (output_file)
 * @param number the movie number
 */
static void
prepareOutputFile (char *jFileName, AVFormatContext * oc, int number)
{
    // prepare output file for format context
    if ((strcasecmp (jFileName, "-") == 0)
        || (strcasecmp (jFileName, "pipe:") == 0)) {
        jFileName = "pipe:";
        snprintf (oc->filename, sizeof (oc->filename), "pipe:");
        // register_protocol (&pipe_protocol);
    } else {
        char first;
        char tmp_buf[PATH_MAX + 1];

        first = jFileName[0];
        sprintf (tmp_buf, jFileName, number);

        // if the filename's first char is a / we have an absolute path
        // and we want one for the file URL. If we don't have one, we
        // construct one
        if (first != '/') {
            sprintf (oc->filename, "file://%s/%s", getenv ("PWD"), tmp_buf);
        } else {
            sprintf (oc->filename, "file://%s", tmp_buf);
        }
        // register_protocol (&file_protocol);
    }
}

/**
 * \brief add a video output stream to the output format
 *
 * @param oc output format context (output_file)
 * @param image captured XImage
 * @param input_pixfmt picture format of the input picture
 * @param codec_id libavcodec's codec id of the codec to use for encoding
 * @param job pointer to the current job
 * @return pointer to the AVStream that has been added to the output format
 */
static AVStream *
add_video_stream (AVFormatContext * oc, const XImage * image,
                  int input_pixfmt, int codec_id, Job * job)
{
    AVStream *st;
    int pix_fmt_mask = 0, i = 0;
    int quality = target->quality, qscale = 0;
    XVC_AppData *app = xvc_appdata_ptr ();


    st = av_new_stream (oc, 0);
    if (! st) {
        fprintf (stderr, _("Could not alloc output stream\n"));
        exit (1);
    }

    st->codec->codec_id = codec_id;
    st->codec->codec_type = CODEC_TYPE_VIDEO;

    // find the video encoder
    codec = avcodec_find_encoder (st->codec->codec_id);
    if (!codec) {
        fprintf (stderr, _("Video codec not found\n"));
        exit (1);
    }
    // put sample parameters
    // resolution must be a multiple of two ... this is taken care of
    // elsewhere but needs to be ensured for rescaled dimensions, too
    if (app->rescale != 100) {
        int n;
        double r;

        r = sqrt ((double) app->rescale / 100.0);

        n = image->width * r;
        if (n % 2 > 0)
            n--;
        st->codec->width = n;

        n = image->height * r;
        if (n % 2 > 0)
            n--;
        st->codec->height = n;
    } else {
        st->codec->width = image->width;
        st->codec->height = image->height;
    }

    // mt init
    if (codec_id == CODEC_ID_MPEG4 || codec_id == CODEC_ID_MPEG1VIDEO ||
        codec_id == CODEC_ID_MPEG2VIDEO) {
        // the max threads is taken from ffmpeg's mpegvideo.c
        avcodec_thread_init (st->codec, XVC_MIN (4,
                                                 (st->codec->height +
                                                  15) / 16));
    }
    // time base: this is the fundamental unit of time (in seconds) in
    // terms of which frame timestamps are represented. for fixed-fps
    // content, timebase should be 1/framerate and timestamp increments
    // should be identically 1.
    st->codec->time_base.den = target->fps.num;
    st->codec->time_base.num = target->fps.den;
    // emit one intra frame every fifty frames at most
    st->codec->gop_size = 50;
    st->codec->mb_decision = 2;
    st->codec->me_method = 1;

    // find suitable pix_fmt for codec
    st->codec->pix_fmt = -1;
    if (codec->pix_fmts != NULL) {
        for (i = 0; codec->pix_fmts[i] != -1; i++) {
            if (0 <= codec->pix_fmts[i] &&
                codec->pix_fmts[i] < (sizeof (int) * 8))
                pix_fmt_mask |= (1 << codec->pix_fmts[i]);
        }
        st->codec->pix_fmt =
            avcodec_find_best_pix_fmt (pix_fmt_mask, input_pixfmt, FALSE, NULL);
    }

    if (!swscale_isSupportedIn (input_pixfmt)) {
        fprintf (stderr, _("The picture format you are grabbing (%i) is not supported by libswscale ... aborting\n"), input_pixfmt);
        exit (1);
    }
    if (!swscale_isSupportedOut (st->codec->pix_fmt)) {
        st->codec->pix_fmt = -1;
    }
    // fallback pix fmts
    if (st->codec->pix_fmt < 0) {
        if (job->target >= CAP_AVI) {
            st->codec->pix_fmt = PIX_FMT_YUV420P;
        } else {
            st->codec->pix_fmt = PIX_FMT_RGB24;
        }
    }
    // flags
    st->codec->flags |= CODEC_FLAG2_FAST;
    // there is no trellis quantiser in libav* for mjpeg
    if (st->codec->codec_id != CODEC_ID_MJPEG)
	st->codec->trellis = 1;
    st->codec->flags &= ~CODEC_FLAG_OBMC;
    // some formats want stream headers to be seperate
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        st->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

    // quality through VBR
    st->codec->flags |= CODEC_FLAG_QSCALE;
    qscale = (100.0 - quality + 1.0) / 3.3;
    st->codec->global_quality = st->quality = FF_QP2LAMBDA * qscale;
    // 0.0 = default qscale
    if (qscale > 0)
        st->codec->qmin = st->codec->qmax = qscale;

    return st;
}

/**
 * \brief guess the picture format of the captured image
 *
 * @param image captured XImage
 * @param c_info needed for alpha channel info
 * @return libavcodec's picture format
 */
static int
guess_input_pix_fmt (const XImage * image, const ColorInfo * c_info)
{
    int input_pixfmt;

    switch (image->bits_per_pixel) {
    case 8:
        input_pixfmt = PIX_FMT_PAL8;
        break;
    case 16:
        if (image->red_mask == 0xF800 && image->green_mask == 0x07E0
            && image->blue_mask == 0x1F) {
            input_pixfmt = PIX_FMT_BGR565;
        } else if (image->red_mask == 0x7C00
                   && image->green_mask == 0x03E0 && image->blue_mask == 0x1F) {
            input_pixfmt = PIX_FMT_BGR555;

        } else {
            fprintf(stderr, _("rgb ordering at image depth %i not supported ... aborting\n"), image->bits_per_pixel);
            fprintf(stderr, "Color masks: r 0x%.6lX g 0x%.6lX b 0x%.6lX\n", image->red_mask, image->green_mask, image->blue_mask);
            exit (1);
        }
        break;
    case 24:
        if (image->red_mask == 0xFF0000 && image->green_mask == 0xFF00
            && image->blue_mask == 0xFF) {
            input_pixfmt = PIX_FMT_BGR24;
        } else if (image->red_mask == 0xFF
                   && image->green_mask == 0xFF00
                   && image->blue_mask == 0xFF0000) {
            input_pixfmt = PIX_FMT_RGB24;
        } else {
            fprintf (stderr, _("rgb ordering at image depth %i not supported ... aborting\n"), image->bits_per_pixel);
            fprintf (stderr, "color masks: r 0x%.6lX g 0x%.6lX b 0x%.6lX\n", image->red_mask, image->green_mask, image->blue_mask);
            exit (1);
        }
        break;
    case 32:
        if ((c_info->alpha_mask == 0xFF000000 || c_info->alpha_mask == 0) && 
                (image->red_mask == 0xFF0000 || image->red_mask == 0xFF) &&
                image->green_mask == 0xFF00) {
            // byte order is relevant here, not endianness endianness is
            // handled by avcodec, but atm no such thing as having ABGR,
            // instead of ARGB in a word. Since we need this for
            // Solaris/SPARC, but need to do the conversion
            // for every frame we do it outside of this loop, cf.
            // below this matches both ARGB32 and ABGR32
            input_pixfmt = PIX_FMT_ARGB32;
        } else {
            fprintf(stderr, _("image depth %i not supported ... aborting\n"), image->bits_per_pixel);
            exit (1);
        }
        break;
    default:
        fprintf(stderr, _("%s %s: image depth %i not supported ... aborting\n"), image->bits_per_pixel);
        exit (1);
    }

    return input_pixfmt;
}

/**
 * \brief main function to write ximage as video to 'fp'
 *
 * @param fp file handle, this, however, is not really used with xtoffmpeg
 * @param image the captured XImage to save
 * \todo remove fp from outside the save function. It is only needed in
 *      xwd and should reside there
 */
void
xvc_ffmpeg_save_frame (FILE * fp, XImage * image)
{
    Job *job = xvc_job_ptr ();
    XVC_AppData *app = xvc_appdata_ptr ();

    /* size of the encoded frame to write to file */
    int out_size = -1;

    // encoder needs to be prepared only once ..
    if (job->state & VC_START) {       // it's the first call


        if (app->current_mode > 0)
            target = &(app->multi_frame);
        else
            target = &(app->single_frame);

        // determine input picture format
        input_pixfmt = guess_input_pix_fmt (image, job->c_info);

        // register all libav* related stuff
        avdevice_register_all ();
        av_register_all ();

        // guess AVOutputFormat
        if (job->target >= CAP_AVI)
            file_oformat = guess_format (xvc_formats[job->target].ffmpeg_name, NULL, NULL);
        else {
            char tmp_fn[30];
            snprintf (tmp_fn, 29, "test-%%d.%s", xvc_formats[job->target].extensions[0]);
            file_oformat = guess_format (NULL, tmp_fn, NULL);
        }
        if (!file_oformat) {
            fprintf(stderr, _("Couldn't determin output format ... aborting\n"));
            exit (1);
        }

		// prepare AVFormatContext
        output_file = av_alloc_format_context ();
        if (!output_file) {
            fprintf (stderr, _("Error allocating memory for format context ... aborting\n"));
            exit (1);
        }
        output_file->oformat = file_oformat;
        if (output_file->oformat->priv_data_size > 0) {
            output_file->priv_data =
                av_mallocz (output_file->oformat->priv_data_size);
            // FIXME: do I need to free this?
            if (!output_file->priv_data) {
                fprintf (stderr, _("Error allocating private data for format context ... aborting\n"));
                exit (1);
            }
        }
        // output_file->packet_size= mux_packet_size;
        // output_file->mux_rate= mux_rate;
        output_file->preload = (int) (0.5 * AV_TIME_BASE);
        output_file->max_delay = (int) (0.7 * AV_TIME_BASE);
        // output_file->loop_output = loop_output;

        // add the video stream and initialize the codecs
        //
        // prepare stream
        fprintf(stderr, "The current pixfmt is %d, but the choosen one is %d\n", input_pixfmt, (input_pixfmt == PIX_FMT_PAL8 ? PIX_FMT_RGB24 : input_pixfmt));
        out_st = add_video_stream (output_file, image,
                     (input_pixfmt == PIX_FMT_PAL8 ? PIX_FMT_RGB24 : input_pixfmt),
                      xvc_video_codecs[job->targetCodec].ffmpeg_id, job);

        // FIXME: set params
        // memset (p_fParams, 0, sizeof(*p_fParams));
        // p_fParams->image_format = image_format;
        // p_fParams->time_base.den = out_st->codec->time_base.den;
        // p_fParams->time_base.num = out_st->codec->time_base.num;
        // p_fParams->width = out_st->codec->width;
        // p_fParams->height = out_st->codec->height;
        // if (av_set_parameters (output_file, p_fParams) < 0) {
        if (av_set_parameters (output_file, NULL) < 0) {
            fprintf (stderr, _("Invalid encoding parameters ... aborting\n"));
            exit (1);
        }
        // open the codec
        if (avcodec_open (out_st->codec, codec) < 0) {
            fprintf (stderr, _("Could not open video codec\n"));
            exit (1);
        }

		if ((job->flags & FLG_REC_SOUND) && (job->au_targetCodec > 0)) {
            int au_ret = add_audio_stream (job);

            // initialize a mutex lock to its default value
            pthread_mutex_init (&mp, NULL);

            if (au_ret == 0) {
                int tret;

                // create and start capture thread
                // initialized with default attributes
                tret = pthread_attr_init (&tattr);

                // create the thread
                tret =
                    pthread_create (&tid, &tattr,
                                    (void *) capture_audio_thread, job);
            }
        }

        /*
         * prepare pictures
         */
        // input picture
        p_inpic = avcodec_alloc_frame ();

        if (input_pixfmt == PIX_FMT_PAL8) {
            scratchbuf8bit =
                malloc (avpicture_get_size
                        (PIX_FMT_RGB24, image->width, image->height));
            if (!scratchbuf8bit) {
                fprintf(stderr, "Could not allocate buffer for 8bit palette conversion\n");
                exit (1);
            }

            avpicture_fill ((AVPicture *) p_inpic, scratchbuf8bit,
                            input_pixfmt, image->width, image->height);
            p_inpic->data[0] = scratchbuf8bit;
            p_inpic->linesize[0] = image->width * 3;
        } else {
            avpicture_fill ((AVPicture *) p_inpic, (uint8_t *) image->data,
                            input_pixfmt, image->width, image->height);
        }

        // output picture
        p_outpic = avcodec_alloc_frame ();

        image_size =
            avpicture_get_size (out_st->codec->pix_fmt,
                                out_st->codec->width, out_st->codec->height);
        outpic_buf = av_malloc (image_size);
        if (!outpic_buf) {
            fprintf (stderr, _("Could not allocate buffer for output frame! ... aborting\n"));
            exit (1);
        }
        avpicture_fill ((AVPicture *) p_outpic, outpic_buf,
                        out_st->codec->pix_fmt, out_st->codec->width,
                        out_st->codec->height);

        /*
         * prepare output buffer for encoded frames
         */
        if ((image_size + 20000) < FF_MIN_BUFFER_SIZE)
            outbuf_size = FF_MIN_BUFFER_SIZE;
        else
            outbuf_size = image_size + 20000;
        outbuf = malloc (outbuf_size);
        if (!outbuf) {
            fprintf (stderr, _("Could not allocate buffer for encoded frame (outbuf)! ... aborting\n"));
            exit (1);
        }
        // img resampling
        if (!img_resample_ctx) {
            img_resample_ctx = sws_getContext (image->width,
                                               image->height,
                                               (input_pixfmt ==
                                                PIX_FMT_PAL8 ? PIX_FMT_RGB24
                                                : input_pixfmt),
                                               out_st->codec->width,
                                               out_st->codec->height,
                                               out_st->codec->pix_fmt, 1,
                                               NULL, NULL, NULL);
            // sws_rgb2rgb_init(SWS_CPU_CAPS_MMX*0);
        }
        // file preparation needs to be done once for multi-frame capture
        // and multiple times for single-frame capture
        if (job->target >= CAP_AVI) {
            // prepare output filenames and register protocols
            // after this output_file->filename should have the right
            // filename
            prepareOutputFile (job->file, output_file, job->movie_no);

            // open the file
            if (url_fopen
                (&output_file->pb, output_file->filename, URL_WRONLY) < 0) {
                fprintf (stderr, _("Could not open '%s' ... aborting\n"), output_file->filename);
                exit (1);
            }

            if (av_write_header (output_file) < 0) {
                dump_format (output_file, 0, output_file->filename, 1);
                fprintf (stderr, _("Could not write header for output file (incorrect codec paramters ?) ... aborting\n"));
                exit (1);
            }

        }
    }

    if (job->target < CAP_AVI) {
        // prepare output filenames and register protocols
        // after this output_file->filename should have the right filename
        prepareOutputFile (job->file, output_file, job->pic_no);

        // open the file
        if (url_fopen (&output_file->pb, output_file->filename, URL_WRONLY)
            < 0) {
            fprintf (stderr, _("Could not open '%s' ... aborting\n"), output_file->filename);
            exit (1);
        }

        if (av_write_header (output_file) < 0) {
            dump_format (output_file, 0, output_file->filename, 1);
            fprintf (stderr, _("Could not write header for output file (incorrect codec paramters ?) ... aborting\n"));
            exit (1);
        }
    }

    /*
     * convert input pic to pixel format the encoder expects
     */

    /** \todo test if the special image conversion for Solaris is still
     *      necessary */
    if (input_pixfmt == PIX_FMT_ARGB32 && 
        (job->c_info->alpha_mask == 0xFF000000 || job->c_info->alpha_mask == 0)
        && image->red_mask == 0xFF && image->green_mask == 0xFF00
        && image->blue_mask == 0xFF0000) {
        myABGR32toARGB32 (image);
    } else if (input_pixfmt == PIX_FMT_PAL8) {
        myPAL8toRGB24 (image, p_inpic, job);
    }
    // img resampling and conversion
    if (sws_scale (img_resample_ctx, p_inpic->data, p_inpic->linesize,
                   0, image->height, p_outpic->data, p_outpic->linesize) < 0) {
        fprintf (stderr, _("Error converting or resampling frame: context %p, iwidth %i, iheight %i, owidth %i, oheight %i, inpfmt %i opfmt %i\n"),
                 img_resample_ctx, image->width,
                 image->height, out_st->codec->width, out_st->codec->height,
                 input_pixfmt, out_st->codec->pix_fmt);
        exit (1);
    }

    /*
     * encode the image
     */

    out_size =
        avcodec_encode_video (out_st->codec, outbuf, outbuf_size, p_outpic);
    if (out_size < 0) {
        fprintf (stderr,
                 _
                 ("error encoding frame: c %p, outbuf %p, size %i, frame %p\n"),
                 out_st->codec, outbuf,
                 outbuf_size, p_outpic);
        exit (1);
    }
    if (job->flags & FLG_REC_SOUND) {
        if (pthread_mutex_lock (&mp) > 0) {
            fprintf (stderr,
                     _
                     ("mutex lock for writing video frame failed ... aborting\n"));
            exit (1);
        }
    }

    /*
     * write frame to file
     */
    if (out_size > 0) {
        do_video_out (output_file, out_st, outbuf, out_size);
    }

    if (job->target < CAP_AVI)
        url_fclose (output_file->pb);

    /*
     * release the mutex
     */
    if (job->flags & FLG_REC_SOUND) {
        if (pthread_mutex_unlock (&mp) > 0) {
            fprintf (stderr,
                     _
                     ("couldn't release the mutex for writing video frame ... aborting\n"));
        }
    }
}

/**
 * \brief cleanup capture session
 */
void
xvc_ffmpeg_clean ()
{
    Job *job = xvc_job_ptr ();


    if (job->flags & FLG_REC_SOUND && tid != 0) {
        {
            int tret;

            // wait till audio thread is actually running, or else
            // the signal might kill xvidcap. we also cannot just
            // drop sending the signal, because the audio thread
            // might just be starting
            while (!audio_thread_running) {
                usleep (10);
            }
            tret = pthread_kill (tid, SIGUSR1);

            pthread_join (tid, NULL);
            tid = 0;
        }
    }

    if (output_file) {
        /*
         * write trailer
         */
        av_write_trailer (output_file);
    }

    if (output_file) {
        int i;

        /*
         * close file if multi-frame capture ... otherwise closed already
         */
        if (job->target >= CAP_AVI)
            url_fclose (output_file->pb);
        /*
         * free streams
         */
        for (i = 0; i < output_file->nb_streams; i++) {
            if (output_file->streams[i]->codec) {
                avcodec_close (output_file->streams[i]->codec);
                av_free (output_file->streams[i]->codec);
                output_file->streams[i]->codec = NULL;
            }
            av_free (output_file->streams[i]);
        }
        if (out_st)
            out_st = NULL;
        if (au_out_st)
            au_out_st = NULL;
        /*
         * free format context
         */
        av_free (output_file->priv_data);
        output_file->priv_data = NULL;
        av_free (output_file);
        output_file = NULL;
    }

    if (img_resample_ctx) {
        sws_freeContext (img_resample_ctx);
        img_resample_ctx = NULL;
    }

    if (outpic_buf) {
        av_free (outpic_buf);
        outpic_buf = NULL;
    }
    av_free (outbuf);                  /* avcodec seems to do that job */
    outbuf = NULL;
    av_free (p_inpic);
    p_inpic = NULL;
    av_free (outpic_buf);
    outpic_buf = NULL;
    av_free (p_outpic);
    p_outpic = NULL;

    if (input_pixfmt == PIX_FMT_PAL8 && scratchbuf8bit) {
        free (scratchbuf8bit);
        scratchbuf8bit = NULL;
    }

    codec = NULL;
    au_codec = NULL;
}