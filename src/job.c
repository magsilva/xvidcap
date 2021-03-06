/**
 * \file job.c
 *
 * This file contains functions for setting up and manipulating Job structs.
 * One esp. important thing here is the setter functions for the recording
 * state machine. Because state changes should be atomic one MUST NOT set
 * job->state manually.
 */
/*
 *
 * Copyright (C) 1997,98 Rasca, Berlin
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
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
 *
 * This file defines the job data structure and some functions to
 * set/get info about a job (kinda tries to be OO ;S )
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>
#include <X11/Intrinsic.h>
#include <errno.h>

#include "job.h"
#include "capture.h"
#include "xtoxwd.h"
#include "frame.h"
#include "colors.h"
#include "codecs.h"
#include "control.h"
#include "app_data.h"
#include "xvidcap-intl.h"
# include "xtoffmpeg.h"

static Job *job = NULL;

static void job_set_capture (void);

/**
 * \brief set and check some parameters for the sound device
 *
 * @param snd the name of the audio input device or "-" for stdin
 * @param rate the sample rate
 * @param size the sample size
 * @param channels the number of channels to record
 */
static void
job_set_sound_dev (char *snd, int rate, int size, int channels)
{
    extern int errno;
    struct stat statbuf;
    int stat_ret;

    job->snd_device = snd;
    if (job->flags & FLG_REC_SOUND) {
        if (strcmp (snd, "-") != 0) {
            stat_ret = stat (snd, &statbuf);

            if (stat_ret != 0) {
                switch (errno) {
                case EACCES:
                    fprintf (stderr,
                             _
                             ("Insufficient permission to access sound input from %s\n"),
                             snd);
                    fprintf (stderr, _("Sound disabled!\n"));
                    job->flags &= ~FLG_REC_SOUND;
                    break;
                default:
                    fprintf (stderr,
                             _("Error accessing sound input from %s\n"), snd);
                    fprintf (stderr, _("Sound disabled!\n"));
                    job->flags &= ~FLG_REC_SOUND;
                    break;
                }
            }
        }
    }

    return;
}

/**
 * \brief create a new job
 *
 * Since this does a malloc, the job needs to be freed
 * @return a new Job struct with variables set to 0|NULL where possible
 */
static Job *
job_new ()
{
    job = (Job *) malloc (sizeof (Job));
    if (!job) {
        fprintf (stderr, "malloc failed?!?");
        exit (1);
    }

    job->file = NULL;
    job->flags = 0;
    job->state = 0;
    job->pic_no = 0;
    job->movie_no = 0;

    job->time_per_frame = 0;
    job->snd_device = NULL;

    job->get_colors = (void *(*)(XColor *, int)) NULL;
    job->save = (void (*)(FILE *, XImage *)) NULL;
    job->clean = (void (*)(void)) NULL;
    job->capture = (long (*)(void)) NULL;

    job->target = 0;
    job->targetCodec = 0;
    job->au_targetCodec = 0;
    job->ncolors = 0;

    job->color_table = NULL;
    job->colors = NULL;
    job->c_info = NULL;

    job->dmg_region = XCreateRegion ();

    job->capture_returned_errno = 0;
    job->frame_moved_x = 0;
    job->frame_moved_y = 0;

    return (job);
}

/**
 * \brief frees a Job
 */
void
xvc_job_free ()
{
    if (job != NULL) {
        if (job->color_table)
            free (job->color_table);

        XDestroyRegion (job->dmg_region);

        if (job->c_info)
            free (job->c_info);

        free (job);
        job = NULL;
    }
}

/**
 * \brief set the Job's color information
 */
void
xvc_job_set_colors ()
{
    XVC_AppData *app = xvc_appdata_ptr ();

    job->ncolors = xvc_get_colors (app->dpy, &(app->win_attr), &(job->colors));
    if (job->get_colors) {
        if (job->color_table)
            free (job->color_table);
        job->color_table = (*job->get_colors) (job->colors, job->ncolors);
    }
}

/**
 * \brief set the Job's contents from the current preferences
 *
 * @param app the preferences struct to set the Job from
 */
void
xvc_job_set_from_app_data (XVC_AppData * app)
{
    XVC_CapTypeOptions *cto;
    char file[PATH_MAX + 1];

    // make sure we do have a job
    if (!job) {
        job_new ();
    }
    // switch sf or mf
    if (app->current_mode != 0)
        cto = &(app->multi_frame);
    else
        cto = &(app->single_frame);

    // various manual settings
    // need to have the flags set to smth. before other functions try to
    // do: flags |= or smth.
    job->flags = app->flags;
    if (app->current_mode == 0 || xvc_is_filename_mutable (cto->file))
        job->flags &= ~(FLG_AUTO_CONTINUE);

    job->time_per_frame = (int) (1000 /
                                 ((float) cto->fps.num / (float) cto->fps.den));

    job->state = VC_STOP;              // FIXME: better move this outta here?
    job->pic_no = cto->start_no;
    job->movie_no = 0;                 // FIXME: make this configurable
    if (cto->audioWanted != 0)
        job->flags |= FLG_REC_SOUND;
    else
        job->flags &= ~FLG_REC_SOUND;
    job_set_sound_dev (app->snddev, cto->sndrate, cto->sndsize,
                       cto->sndchannels);

    job->target = cto->target;
    if (job->target <= 0) {
        if (job->target == 0) {
            // we should be able to safely assume cto->filename is longer
            // smaller than 0 for the next bit because with target == 0
            // it would have been set to a default value otherwise
            job->target = xvc_codec_get_target_from_filename (cto->file);
            // we assume job->target can never be == 0 now, because a
            // sanity checking function should have checked before if
            // we have a valid specification  for a target either
            // through target itself or the filename extension
            if (job->target <= 0) {
                fprintf (stderr, "Unrecoverable error while initializing job from app_data.\n");
                fprintf (stderr, "targetCodec is still 0. This should never happen.\n");
                fprintf (stderr, "Please contact the xvidcap project team.\n");
                exit (1);
            }
        }
    }

    job->targetCodec = cto->targetCodec;
    if (job->targetCodec <= 0) {
        if (job->targetCodec == 0)
            job->targetCodec = xvc_formats[job->target].def_vid_codec;
        if (job->targetCodec < 0) {
            fprintf (stderr, "Unrecoverable error while initializing job from app_data.\n");
            fprintf (stderr, "targetCodec is still < 0. This should never happen.\n");
            fprintf (stderr, "Please contact the xvidcap project team.\n");
            exit (1);
        }
    }
    job->au_targetCodec = cto->au_targetCodec;
    if (job->au_targetCodec <= 0) {
        if (job->au_targetCodec == 0)
            job->au_targetCodec = xvc_formats[job->target].def_au_codec;
        // if 0 the format has no default audio codec. This should only be
        // the case if the format does not support audio or recording
        // without audio is encouraged
        if (job->au_targetCodec < 0) {
            fprintf (stderr, "Unrecoverable error while initializing job from app_data.\n");
            fprintf (stderr, "au_targetCodec is still < 0. This should never happen.\n");
            fprintf (stderr, "Please contact the xvidcap project team.\n");
            exit (1);
        }
    }

    // set temporary filename if set to "ask-user"
    if (strlen (cto->file) < 1) {
        char *home = NULL;
        int pid;

        home = getenv ("HOME");
        pid = (int) getpid ();

        snprintf (file, (PATH_MAX + 1), "%s/.xvidcap-tmp.%i.%s", home, pid,
                  xvc_formats[job->target].extensions[0]);
    } else {
        snprintf (file, (PATH_MAX + 1), "%s", cto->file);
    }

    /** \todo double I need a strdup here? */
    job->file = strdup (file);

    job_set_capture ();

    // the order of the following actions is key!
    // the default is to use the colormap of the root window
    xvc_job_set_save_function (job->target);
    xvc_job_set_colors ();
}

/**
 * \brief get a pointer to the current Job struct
 *
 * @return a pointer to the current job struct. If a Job had not been
 *      initialized before, it will be initialized now.
 */
Job *
xvc_job_ptr (void)
{
    if (job == NULL)
        job_new ();
    return (job);
}

/**
 * \brief selects functions to use for processing the captured frame
 *
 * @param type the id of the target file format used
 */
void
xvc_job_set_save_function (XVC_FFormatId type)
{
    if (type >= CAP_AVI) {
        job->clean = xvc_ffmpeg_clean;
        if (job->targetCodec == VID_CODEC_NONE) {
            job->targetCodec = VID_CODEC_MJPEG;
        }
        job->get_colors = xvc_ffmpeg_get_color_table;
        job->save = xvc_ffmpeg_save_frame;
    } else if (type >= CAP_AVI) {
        job->clean = xvc_ffmpeg_clean;
        if (job->targetCodec == VID_CODEC_NONE) {
            job->targetCodec = VID_CODEC_PGM;
        }
        job->get_colors = xvc_ffmpeg_get_color_table;
        job->save = xvc_ffmpeg_save_frame;
    } else
    {
        job->save = xvc_xwd_save_frame;
        job->get_colors = xvc_xwd_get_color_table;
        job->clean = NULL;
    }
}

/**
 * \brief find the correct capture function
 */
static void
job_set_capture (void)
{
    int input = job->flags & FLG_USE_SHM;

    switch (input) {
    case FLG_USE_SHM:
        job->capture = xvc_capture_shm;
        break;
    default:
        job->capture = xvc_capture_x11;
        break;
    }
}


/**
 * \brief signal the recording thread for certain state changes so it will
 *      be unpaused.
 *
 * @param orig_state previous state
 * @param new_state new state
 */
void
job_state_change_signals_thread (int orig_state, int new_state)
{
    XVC_AppData *app = xvc_appdata_ptr ();

    if (((orig_state & VC_PAUSE) > 0 && (new_state & VC_PAUSE) == 0) ||
        ((orig_state & VC_STOP) == 0 && (new_state & VC_STOP) > 0) ||
        ((orig_state & VC_STEP) == 0 && (new_state & VC_STEP) > 0)
        ) {
        // signal potentially paused thread
        pthread_cond_broadcast (&(app->recording_condition_unpaused));
    }
}

/**
 * \brief set the state overwriting any previous state information
 *
 * @param state state to set
 */
void
xvc_job_set_state (int state)
{
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

    pthread_mutex_lock (&(app->capturing_mutex));
    job->state = state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
}

/**
 * \brief merge the state with present state information
 *
 * @param state the state to merge (OR) with the current state
 */
void
xvc_job_merge_state (int state)
{
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

    pthread_mutex_lock (&(app->capturing_mutex));
    job->state |= state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
}

/**
 * \brief removes a certain state from the current state information
 *
 * @param state the state to remove from the current state
 */
void
xvc_job_remove_state (int state)
{
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

    pthread_mutex_lock (&(app->capturing_mutex));
    job->state &= ~(state);
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
}

/**
 * \brief merge one state with current state information and remove another
 *
 * @param merge_state the state flag to merge with the current state
 * @param remove_state the state flag to remove from the current state
 */
void
xvc_job_merge_and_remove_state (int merge_state, int remove_state)
{
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

    pthread_mutex_lock (&(app->capturing_mutex));
    job->state |= merge_state;
    job->state &= ~(remove_state);
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
}

/**
 * \brief keeps the specified state flag if set
 *
 * @param state the state flag to keep if set (all others are unset)
 */
void
xvc_job_keep_state (int state)
{
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

    pthread_mutex_lock (&(app->capturing_mutex));
    job->state &= state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
}

/**
 * \brief keeps one state and merges with another
 *
 * @param keep_state the state flag to keep
 * @param merge_state the state to merge with the kept
 * @see xvc_job_keep_state
 */
void
xvc_job_keep_and_merge_state (int keep_state, int merge_state)
{
    XVC_AppData *app = xvc_appdata_ptr ();
    int orig_state = job->state;

    pthread_mutex_lock (&(app->capturing_mutex));
    job->state &= keep_state;
    job->state |= merge_state;
    job_state_change_signals_thread (orig_state, job->state);
    pthread_mutex_unlock (&(app->capturing_mutex));
}

//XserverRegion
Region
xvc_get_damage_region ()
{
    //XserverRegion region, dmg_region;
    Region region, dmg_region;
    XVC_AppData *app = xvc_appdata_ptr ();

    pthread_mutex_lock (&(app->damage_regions_mutex));
    //region = XFixesCreateRegion (app->dpy, 0, 0);
    region = XCreateRegion ();
    dmg_region = job->dmg_region;
    job->dmg_region = region;
    pthread_mutex_unlock (&(app->damage_regions_mutex));
    return dmg_region;
}