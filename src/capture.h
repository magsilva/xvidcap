/**
 * \file capture.h
 */

/*
 * Copyright (C) 1997 Rasca Gmelch, Berlin
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
 */

#ifndef _xvc_CAPTURE_H__
#define _xvc_CAPTURE_H__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <X11/Intrinsic.h>

/**
 * \brief since the capture functions have been merged, we need a way for the
 *      commonCapture() function to distinguish between the possible sources.
 */
enum captureFunctions
{
    /** \brief plain X11 */
    X11,
    /** \brief X11 with SHM extension */
    SHM,
    /** \brief element counter */
    NUMFUNCTIONS
};

/*
 * functions from capture.c
 */
long xvc_capture_x11 ();
long xvc_capture_shm ();


#endif     // _xvc_CAPTURE_H__
