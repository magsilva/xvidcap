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
#include <config.h>
#endif

#include <X11/Intrinsic.h>

/**
 * Function used for capturing. This one is used with source = x11,
 * i. e. when capturing from X11 display w/o SHM
 *
 * @return the number of msecs in which the next capture is due
 */
long xvc_capture_x11();

/**
 * Function used for capturing. This one is used with source = shm,
 *      i. e. when capturing from X11 with SHM support
 *
 * @return the number of msecs in which the next capture is due
 */
long xvc_capture_shm();


#endif     // _xvc_CAPTURE_H__
