/**
 * Utility functions for retrieving color information.
 */

/*
 * Copyright (C) 1997 Rasca Gmelch, Berlin
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

#ifndef _xvc_COLORS_H__
#define _xvc_COLORS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif     // HAVE_CONFIG_H

/**
 * Struct to contain various color properties, like masks, depths, shifts etc.
 */
typedef struct
{
    unsigned long red_shift;
    unsigned long green_shift;
    unsigned long blue_shift;
    unsigned long alpha_shift;

    unsigned long max_val;
    unsigned long bit_depth;

    unsigned long red_max_val;
    unsigned long green_max_val;
    unsigned long blue_max_val;
    unsigned long alpha_max_val;

    unsigned long red_bit_depth;
    unsigned long green_bit_depth;
    unsigned long blue_bit_depth;
    unsigned long alpha_bit_depth;

    u_int32_t alpha_mask;
} ColorInfo;

/**
 * Fills the ColorInfo struct with some useful color information, especially
 * the masks and shifts are relevant
 *
 * @param image pointer to the XImage to assimilate color information from
 * @param ci pointer to a ColorInfo struct
 */
ColorInfo *xvc_get_color_info(const XImage * image);

/**
 * Retrieves XColor array for a given window
 *
 * @param dpy pointer to the display to retrieve the information from
 * @param winfo pointer to window attributes for which to retrieve colors information
 * @param colors return pointer to an array of XColor elements
 * @return the number of colors retrieved
 */
int xvc_get_colors(Display * dpy, const XWindowAttributes * winfo, XColor ** colors);

#endif     // _xvc_COLORS_H__
