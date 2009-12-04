/**
 * \file capture.c
 *
 * This file contains routines used for capturing individual frames.
 * Theese routines are called from the capture thread spawned on start of
 * a capture session. Once started the capture is completely governed by
 * changes in the VC_* states. It will also be stopped on reaching a
 * configured maximum number of frames or maximum capture time.
 */

/*
 * Copyright (C) 1997-98 Rasca, Berlin
 * Copyright (C) 2003-07 Karl H. Beckers, Frankfurt
 * contains code written 2006 by Eduardo Gomez for the ffmpeg project
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


#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XWDFile.h>
#include <X11/extensions/Xfixes.h>
#include "colors.h"
#include <X11/Xregion.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/shmstr.h>
#include <X11/extensions/extutil.h>

#include <errno.h>

#include "capture.h"
#include "job.h"
#include "app_data.h"
#include "control.h"
#include "frame.h"

extern int xvc_led_time;

/** make error numbers accessible */
extern int errno;

/** vectors used for alpha masking of real mouse pointer */
static unsigned char top[65536];

/** vectors used for alpha masking of real mouse pointer */
static unsigned char bottom[65536];

/** \brief each captured frame saves its mouse pointer information for
    the next frame to find it. This is only required in the context
    of Xdamage */
static XRectangle pointer_area;

/**
 * Find out where the mouse pointer is
 *
 * @param x return pointer to write x coordinate to pre-existing int
 * @param y return pointer to write y coordinate to pre-existing int
 */
static void
getCurrentPointer(int *x, int *y)
{
    Window childwindow;
    int dummy;
    XVC_AppData *app = xvc_appdata_ptr ();

    if (! XQueryPointer(app->dpy, app->root_window, &(app->root_window), &childwindow, x, y, &dummy, &dummy, (unsigned int *) &dummy)) {
        fprintf (stderr,"Couldn't find mouse pointer for display: %p , rootwindow: %p\n", app->dpy, &(app->root_window));
        *x = -1;
        *y = -1;
    }
}

/**
 * Return mouse pointer shape.
 *
 * This is only used with XFixes. Previously we did this inside
 *      paintMousePointer. I've split this to be able to do the getting inside
 *      a lock and the painting outside it.
 *
 * @return a pointer to an XFixesCursorImage for the current pointer
 */
static XFixesCursorImage *
getCurrentPointerImage()
{
    XVC_AppData *app = xvc_appdata_ptr ();
    XFixesCursorImage *x_cursor = NULL;

    x_cursor = XFixesGetCursorImage (app->dpy);

    return x_cursor;
}

/**
 * Mouse painting helper function that applies an 'and' and 'or' mask pair to
 * '*dst' pixel. It actually draws a mouse pointer pixel to grabbed frame.
 *
 * @param dst Destination pixel
 * @param and Part of the mask that must be applied using a bitwise 'and'
 *            operator
 * @param or  Part of the mask that must be applied using a bitwise 'or'
 *            operator
 * @param bits_per_pixel Bits per pixel used in the grabbed image
 */
static void inline
apply_masks (uint8_t * dst, uint32_t and, uint32_t or, int bits_per_pixel)
{
    switch (bits_per_pixel) {
    case 32:
        *(uint32_t *) dst = (*(uint32_t *) dst & and) | or;
        break;
    case 16:
        *(uint16_t *) dst = (*(uint16_t *) dst & and) | or;
        break;
    case 8:
        *dst = !!or;
        break;
    }
}

/**
 * \brief Counts the bits set in a long word
 *
 * @param bits a long integer as a bitfield
 * @return the number of bits set
 */
static int
count_one_bits (long bits)
{
    int i, res = 0;
    for (i = 0; i < (sizeof (long) * 8); i++) {
        if ((bits & (0x1 << i)) > 0)
            res++;
    }
    return res;
}

/**
 * Paints a mouse pointer in an X11 image.
 *      This is the version for use without xfixes or xdamage
 *
 * @param image Image where to paint the mouse pointer
 * @param my_x_cursor pointer to the XFixesCursorImage of the captured mouse
 *      pointer. If a dummy mouse pointer is wanted, unset FLG_USE_XFIXES in
 *      app->flags, and pass x and y
 * @param x the x position of the pointer. Only needed if !FLG_USE_XFIXES.
 * @param y the y position of the pointer. Only needed if !FLG_USE_XFIXES.
 * @return an XRectangle for Xdamage to know which area to repair in the next
 *      frame.
 */
static XRectangle
paintMousePointer(XImage * image, XFixesCursorImage * my_x_cursor, int x, int y)
{
    /* 16x20x1bpp bitmap for the black channel of the mouse pointer */
    static const uint16_t const mousePointerBlack[] = {
        0x0000, 0x0003, 0x0005, 0x0009, 0x0011,
        0x0021, 0x0041, 0x0081, 0x0101, 0x0201,
        0x03C1, 0x0049, 0x0095, 0x0093, 0x0120,
        0x0120, 0x0240, 0x0240, 0x0380, 0x0000
    };

    /* 16x20x1bpp bitmap for the white channel of the mouse pointer */
    static const uint16_t const mousePointerWhite[] = {
        0x0000, 0x0000, 0x0002, 0x0006, 0x000E,
        0x001E, 0x003E, 0x007E, 0x00FE, 0x01FE,
        0x003E, 0x0036, 0x0062, 0x0060, 0x00C0,
        0x00C0, 0x0180, 0x0180, 0x0000, 0x0000
    };

    int cursor_width = 16, cursor_height = 20;
    XVC_AppData *app = xvc_appdata_ptr ();
    Job *job = xvc_job_ptr ();
    XRectangle pArea = { 0, 0, 0, 0 };

    unsigned char topp, botp;
    unsigned long applied;

    // only paint a mouse pointer into the dummy frame if the position of
    // the mouse is within the rectangle defined by the capture frame

    // if we use xfixes, we want a cursor image
    if (!my_x_cursor) {
        return pArea;
    }
    // set cursor dimensions from cursor image
    cursor_width = my_x_cursor->width;
    cursor_height = my_x_cursor->height;
    y = my_x_cursor->y - my_x_cursor->yhot;
    x = my_x_cursor->x - my_x_cursor->xhot;

    // only paint a mouse pointer into the dummy frame if the position of
    // the mouse is within the rectangle defined by the capture frame
    if ((x - app->area->x + cursor_width) >= 0 && x < (app->area->width + app->area->x) &&
        (y - app->area->y + cursor_height) >= 0 && y < (app->area->height + app->area->y)) {
        uint8_t *im_data = (uint8_t *) image->data;
        int bytes_per_pixel = image->bits_per_pixel >> 3;
        int line;
        uint32_t masks = 0;
        int yoff = app->area->y - y;

        /* Select correct masks and pixel size */
        if (image->bits_per_pixel == 8) {
             masks = 1;
         } else {
             masks = (image->red_mask | image->green_mask | image->blue_mask);
        }

        // first: shift to right line
        im_data +=
            (image->bytes_per_line * XVC_MAX (0, (y - app->area->y)));

        // then: shift to right pixel
        im_data += (bytes_per_pixel * XVC_MAX (0, (x - app->area->x)));

        /* Draw the cursor - proper loop */
        for (line = XVC_MAX (0, yoff); line < XVC_MIN (cursor_height, (app->area->y + image->height) - y); line++) {
            int column;
            uint16_t bm_b;
            uint16_t bm_w;
            int xoff = app->area->x - x;
            unsigned long *pix_pointer = NULL;

            pix_pointer = (line * my_x_cursor->width) + my_x_cursor->pixels;

            if (app->mouseWanted == 1) {
                bm_b = mousePointerBlack[line];
                bm_w = mousePointerWhite[line];
            } else {
                bm_b = mousePointerWhite[line];
                bm_w = mousePointerBlack[line];
            }

            if (xoff > 0) {
                bm_b >>= xoff;
                bm_w >>= xoff;
            }

            for (column = XVC_MAX (0, xoff); column < XVC_MIN (cursor_width, (app->area->x + app->area->width) - x); column++) {
                    int count;
                    int rel_x = (x - app->area->x + column);
                    int rel_y = (y - app->area->y + line);

                    /** \brief alpha mask of the pointer pixel */
                    int mask = (*pix_pointer & job->c_info->alpha_mask) >> job->c_info->alpha_shift;
                    int shift, src_shift, src_mask;

                    /* The manpage of XGetPixel say the return value is
                     * "normalized". No idea what's meant by that, because
                     * the values returned are definetely exactly as in the
                     * XImage, i.e. a 8 bit palette element for PAL8 or a
                     * 16bit RGB value for RGB16 expanded to a long */
                    long pixel = XGetPixel (image, rel_x, rel_y);

                    /* alpha masking on 8bit palette seems to have issues with
                     * high transparencies. We just ignore those. */
                    if (image->depth == 8) {
                        if (mask < 10)
                            mask = 0;
                    }
                    // shortcut
                    if (mask == 0) {
                        applied = pixel;    // | job->c_info->alpha_mask;
                    } else {
                        applied = 0;   //job->c_info->alpha_mask;

                        /* if we're on PAL8 we want the actual RGB values from
                         * the palette rather than the palette element. We can
                         * safely ignore alpha, here. */
                        if (image->depth == 8) {
                            if (job->target != CAP_XWD) {
                                pixel = ((u_int32_t *) job->color_table)[(pixel & 0x00FFFFFF)];
                            } else {
                                long opixel = pixel;

                                pixel = (((XWDColor *) job->color_table)[(opixel & 0x00FFFFFF)].red & 0xFF00) << 16;
                                pixel |= (((XWDColor *) job->color_table)[(opixel & 0x00FFFFFF)].green & 0xFF00) << 8;
                                pixel = ((XWDColor *) job->color_table)[(opixel & 0x00FFFFFF)]. blue & 0xFF00;
                            }
                        }
                        // treat one color element at a time
                        for (count = 2; count >= 0; count--) {
                            shift = count * 8;

                            /* if we don't have PAL8, we have RGB and need to
                             * take native bit shifts and masks taken from X11
                             * into account.
                             * Otherwise we got the RGB values from the palette
                             * where they are always 8 bit per color. */
                            if (image->depth != 8) {
                                switch (count) {
                                case 2:
                                    src_shift = job->c_info->red_shift;
                                    src_mask = image->red_mask;
                                    break;
                                case 1:
                                    src_shift = job->c_info->green_shift;
                                    src_mask = image->green_mask;
                                    break;
                                default:
                                    src_shift = job->c_info->blue_shift;
                                    src_mask = image->blue_mask;
                                }
                            } else {
                                src_shift = shift;
                                src_mask = 0xFF << src_shift;
                            }

                            // alpha blending next
                            topp = top[(mask << 8) + ((*pix_pointer >> shift) & 0xFF)];
                            botp = bottom[(mask << 8) + (((pixel & src_mask) >> src_shift) & 0xFF)];
                            applied |= ((topp + botp) & (src_mask >> src_shift)) << src_shift;
                        }

                        /* if we have PAL8 we need to find the palette element
                         * closest to the result of the alpha blending before
                         * writing back to the XImage. We do this by calculating
                         * the number of bits matching between the blended RGB
                         * value and each palette entry until we find an exact
                         * match. */
                        if (image->depth == 8) {
                            int elem = 0;
                            int i = 0, delta = 0;
                            int elem_delta;
                            long ct_pixel;

                            if (job->target != CAP_XWD)
                                ct_pixel = ((u_int32_t *) job->color_table)[i];
                            else {
                                ct_pixel = (((XWDColor *) job->color_table)[i].red & 0xFF00) << 16;
                                ct_pixel |= (((XWDColor *) job->color_table)[i].green & 0xFF00) << 8;
                                ct_pixel = ((XWDColor *) job->color_table)[i].blue & 0xFF00;
                            }

                            elem_delta = count_one_bits((applied & ct_pixel) & 0x00FFFFFF);
                            elem_delta += count_one_bits ((~applied & ~ct_pixel) & 0x00FFFFFF);

                            if (elem != applied) {
                                for (i = 1; i < job->ncolors; i++) {
                                    if (job->target != CAP_XWD)
                                        ct_pixel = ((u_int32_t *) job->color_table)[i];
                                    else {
                                        ct_pixel = (((XWDColor *) job->color_table)[i].red & 0xFF00) << 16;
                                        ct_pixel |= (((XWDColor *) job->color_table)[i].green & 0xFF00) << 8;
                                        ct_pixel = ((XWDColor *) job->color_table)[i].blue & 0xFF00;
                                    }
                                    delta = count_one_bits ((applied & ct_pixel) & 0x00FFFFFF);
                                    delta += count_one_bits ((~applied & ~ct_pixel) & 0x00FFFFFF);
                                    if (delta > elem_delta) {
                                        elem_delta = delta;
                                        elem = i;
                                        if (elem_delta == 24)
                                            break;
                                    }
                                }
                                applied = elem;
                            }
                        }
                    }

                    // write pixel
                    XPutPixel (image, rel_x, rel_y, applied);

                    pix_pointer++;
            }
           im_data += image->bytes_per_line;
        }

        // return the are covered by the mouse pointer
        pArea.x = x;
        pArea.y = y;
        pArea.width = cursor_width;
        pArea.height = cursor_height;

		fprintf (stderr, "Mouse area (x, y, width, height) = %d, %d, %d, %d\n", pArea.x, pArea.y, pArea.width, pArea.height);
    } else {
        // otherwise return an empty area
        pArea.x = pArea.y = pArea.width = pArea.height = 0;
    }

    return pArea;
}

static
void drawCircleDisplay(Display *display, Drawable d, GC gc, int x, int y, int radius)
{
        int X = x - radius;
        int Y = y - radius;
        int angle = 360 * 64;
	
        XDrawArc(display, d, gc, X, Y, 2*radius, 2*radius, 0, angle);
        XFillArc(display, d, gc, X, Y, 2*radius, 2*radius, 0, angle);
}


/**
 * \brief reads data from the Xserver to a chunk of memory on the client
 *
 * @param dpy a pointer to the display to read from
 * @param d the drawable to use
 * @param data a pointer to the chunk of memory to store the image
 * @param x origin x coordinate
 * @param y origin y coordinate
 * @return we were successfull TRUE or FALSE
 */
static Boolean
XGetZPixmap (Display * dpy, Drawable d, char *data, int x, int y,
             int width, int height)
{
    register xGetImageReq *req = NULL;
    xGetImageReply rep;
    long nbytes;

    if (!data)
        return (False);

    LockDisplay (dpy);
    GetReq (GetImage, req);
    /*
     * first set up the standard stuff in the request
     */
    req->drawable = d;
    req->x = x;
    req->y = y;
    req->width = width;
    req->height = height;
    req->planeMask = AllPlanes;
    req->format = ZPixmap;
    if (_XReply (dpy, (xReply *) & rep, 0, xFalse) == 0 || rep.length == 0) {
        XUnlockDisplay (dpy);
        SyncHandle ();
        return (False);
    }

    nbytes = (long) rep.length << 2;

    _XReadPad (dpy, data, nbytes);
    UnlockDisplay (dpy);
    SyncHandle ();

    return (True);
}

/**
 * \brief reads new data in a pre-existing image structure.
 *
 * @param dpy a pointer to the display to read from
 * @param d the drawable to use
 * @param image a pointer to the image to return data to
 * @param x origin x coordinate
 * @param y origin y coordinate
 * @return we were successfull TRUE or FALSE
 */
static Boolean
XGetZPixmapToXImage (Display * dpy, Drawable d, XImage * image, int x, int y)
{
    return XGetZPixmap (dpy, d, (char *) image->data, x, y,
                        image->width, image->height);
}

/**
 * \brief reads data from the Xserver to a chunk of memory on the client.
 *      This version uses shared memory access to X11.
 *
 * @param dpy a pointer to the display to read from
 * @param d the drawable to use
 * @param shminfo shared segment info attached to the XImage
 * @param shm_opcode the major opcode for the shm extension
 * @param data a pointer to the chunk of memory to store the image
 * @param x origin x coordinate
 * @param y origin y coordinate
 * @return we were successfull TRUE or FALSE
 */
static Boolean
XGetZPixmapSHM (Display * dpy, Drawable d, XShmSegmentInfo * shminfo,
                int shm_opcode, char *data, int x, int y, int width, int height)
{
    register xShmGetImageReq *req = NULL;
    xShmGetImageReply rep;
    long nbytes;

    if (!data || !shminfo || !shm_opcode)
        return (False);
    LockDisplay (dpy);
    GetReq (ShmGetImage, req);
    /*
     * first set up the standard stuff in the request
     */
    req->reqType = shm_opcode;
    req->shmReqType = X_ShmGetImage;
    req->shmseg = shminfo->shmseg;

    req->drawable = d;
    req->x = x;
    req->y = y;
    req->width = width;
    req->height = height;
    req->planeMask = AllPlanes;
    req->format = ZPixmap;

    req->offset = data - shminfo->shmaddr;

    if (_XReply (dpy, (xReply *) & rep, 0, xFalse) == 0 || rep.length == 0) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return (False);
    }

    nbytes = (long) rep.length << 2;

    _XReadPad (dpy, data, nbytes);

    UnlockDisplay (dpy);
    SyncHandle ();

    return (True);
}

/**
 * \brief copies a small image into another larger image
 *
 * @param needle pointer to the memory containing the small image
 * @param needle_x the x position of the small image within the larger image
 * @param needle_y the y position of the small image within the larger image
 * @param needle_width the width of the small image
 * @param needle_bytes_pl number of bytes per line in the small image (may be
 *      padded.)
 * @param needle_height the height of the small image
 * @param haystack pointer to the memory holding the larger image
 * @param haystack_width the width of the larger image
 * @param haystack_bytes_pl number of bytes per line in the larger image (may
 *      be padded.)
 * @param haystack_height the height of the larger image
 * @param bytes_per_pixel the number of bytes a pixel requires for
 *      representation, e.g. 4 for rgba, 1 for pal8. This requires the value
 *      to be identical for needle and haystack.
 */
static void
placeImageInImage (char *needle, int needle_x, int needle_y,
                   int needle_width, int needle_bytes_pl, int needle_height,
                   char *haystack, int haystack_width, int haystack_bytes_pl,
                   int haystack_height, int bytes_per_pixel)
{
    char *h_cursor, *n_cursor;
    int i;

    h_cursor =
        haystack + ((needle_x * bytes_per_pixel) +
                    (needle_y * haystack_bytes_pl));
    n_cursor = needle;
    for (i = 0; i < needle_height; i++) {
        if (h_cursor + (needle_width * bytes_per_pixel) >
            haystack + (haystack_height * haystack_bytes_pl)) {
            fprintf (stderr, "Out of bounds ... clipped correctly?\n");
            break;
        }
        memcpy (h_cursor, n_cursor, needle_width * bytes_per_pixel);
        h_cursor += haystack_bytes_pl;
        n_cursor += needle_bytes_pl;
    }
}

/**
 * \brief compute the output filename depending on current capture mode and
 *      frame or movie number. Then open that file for writing.
 *
 * @return a pointer to a file handle or NULL
 */
static FILE *
getOutputFile ()
{
    char file[PATH_MAX + 1];
    FILE *fp = NULL;
    Job *job = xvc_job_ptr ();
    XVC_AppData *app = xvc_appdata_ptr ();

    if (app->current_mode > 0) {
        sprintf (file, job->file, job->movie_no);
    } else {
        sprintf (file, job->file, job->pic_no);
    }

    fp = fopen(file, "wb");
    return fp;
}

/**
 * \brief this captures into an existing XImage. This is the plain X11
 *      version.
 *
 * @param dpy a pointer to the display to read from
 * @param image a pointer to the image to write to
 * @return 1 on success, 0 on failure
 */
static int
captureFrameToImage(Display * dpy, XImage * image)
{
    int ret = 0;
    XVC_AppData *app = xvc_appdata_ptr ();

    Region damaged_region = NULL;

	// if we use Xdamage and are still capturing a complete frame (which is
    // what we're doing here, then we can discard the regions damaged up to
    // now

	// get the damage up to now
    // we're assuming we're on a locked and synched display
    damaged_region = xvc_get_damage_region();

    // get the image here
    if (XGetZPixmapToXImage(dpy, app->root_window, image, app->area->x, app->area->y)) {
        // paint the mouse pointer into the captured image if necessary
        ret = 1;
    }

    // and discard the damage
    XDestroyRegion (damaged_region);
 
    return ret;
}

/**
 * \brief creates a new XImage. This is the plain X11 version.
 *
 * @param dpy pointer to an open Display
 * @param width width of the image to create
 * @param height height of the image to create
 * @return a pointer to the new XImage
 * \todo XCreateImage should work without getting the image data from the
 *      Xserver. Cannot seem to get this to work, though. Using XGetImage.
 */
static XImage *
createImage(Display * dpy, int width, int height)
{
    XImage *image = NULL;
    XVC_AppData *app = xvc_appdata_ptr();

    image = XGetImage(dpy, app->root_window, app->area->x, app->area->y, app->area->width, app->area->height, AllPlanes, ZPixmap);
    g_assert(image);

    return image;
}

/**
 * Captures without an existing XImage and creates one along the
 *  way. This is the plain X11 version.
 *
 * @param dpy a pointer to the display to read from
 * @return a pointer to an XImage or NULL
 */
static XImage *
captureFrameCreatingImage(Display * dpy)
{
    XImage *image = NULL;
    XVC_AppData *app = xvc_appdata_ptr();

    // get the image here
    image = XGetImage(dpy, app->root_window, app->area->x, app->area->y, app->area->width, app->area->height, AllPlanes, ZPixmap);
    if (! image) {
        fprintf(stderr, "Can't get image: %dx%d+%d+%d\n", app->area->width,  app->area->height, app->area->x, app->area->y);
        exit(1);
    }

    return image;
}


/**
 * Captures into an existing XImage. This is the SHM version.
 *
 * @param dpy a pointer to the display to read from
 * @param image a pointer to the image to write to
 
 * @return 1 on success, 0 on failure
 */
static int
captureFrameToImageSHM(Display * dpy, XImage * image)
{
    int ret = 0;
    XVC_AppData *app = xvc_appdata_ptr ();

    Region damaged_region = NULL;
    // if we use Xdamage and are still capturing a complete frame (which is
    // what we're doing here), then we can discard the regions damaged up to
    // now

	// get the damage up to now
    // we're assuming we're on a locked and synched display
    damaged_region = xvc_get_damage_region();

    // get the image here
    if (XShmGetImage(dpy, app->root_window, image, app->area->x, app->area->y, AllPlanes)) {
        // paint the mouse pointer into the captured image if necessary
        ret = 1;
    }
	
    // discard the damage
    XDestroyRegion (damaged_region);

    return ret;
}

/**
 * \brief creates a new XImage. This is the SHM version.
 *
 * @param dpy pointer to an open X11 Display
 * @param shminfo shared memory segment info to attach
 * @param width width of the image to create
 * @param height height of the image to create
 * @return pointer to the XImage created
 */
static XImage *
createImageSHM(Display * dpy, XShmSegmentInfo * shminfo, int width, int height)
{
    XVC_AppData *app = xvc_appdata_ptr();
    XImage *image = NULL;
    Visual *visual = app->win_attr.visual;
    unsigned int depth = app->win_attr.depth;

    // get the image here
    image = XShmCreateImage(dpy, visual, depth, ZPixmap, NULL, shminfo, width, height);
    if (image) {
        shminfo->shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT | 0777);
        if (shminfo->shmid == -1) {
            fprintf(stderr, "Failed to get shared memory.\n");
            exit(1);
        }
        shminfo->shmaddr = image->data = shmat(shminfo->shmid, 0, 0);
        shminfo->readOnly = False;

        if (XShmAttach(dpy, shminfo) == 0) {
            fprintf(stderr, "Failed to attach to shared memory.\n");
            exit (1);
        }

        image->data = shminfo->shmaddr;
        shmctl(shminfo->shmid, IPC_RMID, 0);
    }

    return image;
}

/**
 * Captures without an existing XImage and creates one along the way. This is
 * the SHM version.
 *
 * @param dpy a pointer to the display to read from
 * @param shminfo shared memory information
 *
 * @return a pointer to an XImage or NULL
 */
static XImage *
captureFrameCreatingImageSHM(Display *dpy, XShmSegmentInfo *shminfo)
{
    XVC_AppData *app = xvc_appdata_ptr();
    XImage *image = NULL;
	int captured = 0;

    // get the image here
    image = createImageSHM(dpy, shminfo, app->area->width, app->area->height);
    if (! image) {
        fprintf(stderr, "Can't get image: %d,%d,%d,%d\n", app->area->x, app->area->y, app->area->width, app->area->height);
        exit (1);
    }

    captured = captureFrameToImageSHM(dpy, image);
	if (captured) {
		int x;
		int y;
		getCurrentPointer(&x, &y);
		// TODO: Highlight the mouse position in the image. You should use XPutPixel(XImage *ximage, int x, int y, unsigned long pixel)
	}
	
    return image;
}


/**
 * Calculates in how many msecs the next capture is due based on fps
 * and the duration of the previous capture.
 *
 * @param time the time in msecs when the last capture started
 * @param time1 the time in msecs when the last capture ended
 * @return the number of msecs in which the next capture is due
 */
static long
checkCaptureDuration(long time, long time1)
{
    Job *job = xvc_job_ptr();
    struct timeval curr_time; /* for measuring the duration of a frame capture */

    // update monitor widget, here time is the time the capture took
    // time == 0 resets led_meter
    if (time1 < 1) {
        time1 = 1;
	}
    // this sets the frame monitor widget
    xvc_led_time = time1;

    // calculate the remaining time we have till capture of next frame
    time1 = job->time_per_frame - time1;

    if (time1 > 0) {
        // get time again because updating frame drop meter took some time
        // we're only doing this if wait time for next frame hasn't been
        // negative already before
        gettimeofday(&curr_time, NULL);
        time = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;
        time = job->time_per_frame - time;
    } else {
        time = time1;
    }

	if (time < 0) {
        fprintf(stderr, "missing %ld milli secs (%d needed per frame), pic no %d\n", time, job->time_per_frame, job->pic_no);
        time = 0;
    }

    return time;
}

/**
 * \brief this is the merged capture function that handles all sources.
 *
 * @param capfunc passes the source as specified in captureFunctions
 * @return the number of msecs in which the next capture is due
 * @see captureFunctions
 */
static long
commonCapture (enum captureFunctions capfunc)
{
#define DEBUGFUNCTION "commonCapture()"
    static XImage *image = NULL;
    static FILE *fp = NULL; // file handle to write the frame to
    long time = 0, time1;   /* for measuring the duration of a frame capture */
    struct timeval curr_time;   /* for measuring the duration of a frame
                                 * capture */
    static int shm_opcode = 0, shm_event_base = 0, shm_error_base = 0;

//    static XRectangle pointer_area;

    XVC_AppData *app = xvc_appdata_ptr ();
    XVC_CapTypeOptions *target;
    Job *job = xvc_job_ptr ();
    int full_cleanup = TRUE;
    int frame_moved = FALSE;

    XFixesCursorImage *x_cursor = NULL;

	Region damaged_region;
    static XImage *dmg_image = NULL;

    static XShmSegmentInfo dmg_shminfo;

    static XShmSegmentInfo shminfo;


    if (app->current_mode != 0)
        target = &(app->multi_frame);
    else
        target = &(app->single_frame);

    // we really cannot have external state changes
    // while we're reacting on state
    // frame moves, too, are evil
    pthread_mutex_lock (&(app->capturing_mutex));

    // if the frame has moved during capture, move it here
    if (job->frame_moved_x != 0 || job->frame_moved_y != 0) {
        xvc_frame_change (app->area->x + job->frame_moved_x,
                          app->area->y + job->frame_moved_y,
                          app->area->width, app->area->height, FALSE, FALSE);
        frame_moved = TRUE;
        job->frame_moved_x = job->frame_moved_y = 0;
        // make sure the frame is actually moved before continuing with the
        // capture to avoid capturing the frame (this is not a guarantee with
        // compositing window managers, though)
        XSync (app->dpy, False);
    } else if ((app->flags & FLG_LOCK_FOLLOWS_MOUSE) != 0 &&
               xvc_is_frame_locked ()) {
        int px = 0, py = 0;
        int x = app->area->x, y = app->area->y;
        int width = app->area->width, height = app->area->height;

        getCurrentPointer (&px, &py);

        if (px > (app->area->x + ((app->area->width * 9) / 10))) {
            x = px - ((app->area->width * 9) / 10);
            frame_moved = TRUE;
        } else if (px < (app->area->x + (app->area->width / 10))) {
            x = px - (app->area->width / 10);
            frame_moved = TRUE;
        }
        if (py > (app->area->y + ((app->area->height * 9) / 10))) {
            y = py - ((app->area->height * 9) / 10);
            frame_moved = TRUE;
        } else if (py < (app->area->y + (app->area->height / 10))) {
            y = py - (app->area->height / 10);
            frame_moved = TRUE;
        }

        if (frame_moved) {
            xvc_frame_change (x, y, width, height, FALSE, FALSE);
            XSync (app->dpy, False);
        }
    }
    // wait for next iteration if pausing
    if (job->state & VC_REC) {         // if recording ...


        // the next bit is true if we have configured max_frames and we're
        // reaching the limit with this captured frame
        if (target->frames && ((job->pic_no - target->start_no) >
                               target->frames - 1)) {

            // we need to stop the capture to go through the necessary
            // cleanup routines for writing a correct file. If we have
            // autocontinue on we're setting a flag to let the cleanup
            // code know we need
            // to restart again afterwards
            if (job->flags & FLG_AUTO_CONTINUE) {
                job->state |= VC_CONTINUE;
            }

            goto CLEAN_CAPTURE;
        }
        // take the time before starting the capture
        gettimeofday (&curr_time, NULL);
        time = curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000;

        // open the output file we need to do this for every frame for
        // individual frame
        // capture and only once for capture to movie
        if ((app->current_mode == 0) ||
            (app->current_mode > 0 && job->state & VC_START)) {


            fp = getOutputFile ();
            if (!fp) {
                // we react on errno when cleaning up
                job->capture_returned_errno = errno;
                // we may or may not need a full cleanup (for movie we need
                // one only if we have actually started, i. e. proceeded
                // beyond frame 0
                if (app->current_mode > 0 || job->pic_no == target->start_no)
                    full_cleanup = FALSE;
                goto CLEAN_CAPTURE;
            }
        }
        // if this is the first frame for the current job ...
        if (job->state & VC_START) {

            // the next bit is true if we have configured max_frames and we're
            // reaching the limit with this captured frame
            // this should not at all be possible with this, the first frame
            if (target->frames && ((job->pic_no - target->start_no) > target->frames - 1)) {
                full_cleanup = FALSE;
                goto CLEAN_CAPTURE;
            }

            // if we use xfixes, we need this for alpha blending of the mouse pointer
            if (app->mouseWanted > 0) {
                unsigned int mask, color;

                for (mask = 0; mask <= 255; mask++) {
                    for (color = 0; color <= 255; color++) {
                        top[(mask << 8) + color] = (color * (mask + 1)) >> 8;
                        bottom[(mask << 8) + color] =
                            (color * (256 - mask)) >> 8;
                    }
                }
            }

            // lock the display for consistency
            XLockDisplay (app->dpy);

            // capture the start frame with whatever function applicable
            switch (capfunc) {
            case SHM:
                image = captureFrameCreatingImageSHM (app->dpy, &shminfo);
                // also initialize xdamage stuff
                XQueryExtension(app->dpy, "MIT-SHM", &shm_opcode, &shm_event_base, &shm_error_base);
                dmg_image = createImageSHM(app->dpy, &dmg_shminfo, app->area->width, app->area->height);
                break;
            case X11:
            default:
                image = captureFrameCreatingImage (app->dpy);
                // also initialize xdamage stuff
                dmg_image = createImage (app->dpy, app->area->width, app->area->height);
            }

            if (app->mouseWanted > 0) {
                    x_cursor = getCurrentPointerImage ();
            }
            // now, we have captured all we need and can unlock the display
            XUnlockDisplay (app->dpy);

            // need to determine c_info from image FIRST
            if (!(job->c_info))
                job->c_info = xvc_get_color_info (image);

            // now we can draw the mouse pointer
            if (image) {
                if (app->mouseWanted > 0) {
                    pointer_area = paintMousePointer(image, x_cursor, 0, 0);
                }
                // we can allow state or frame changes after this
                pthread_mutex_unlock (&(app->capturing_mutex));
                // call the necessary XtoXYZ function to process the image
                (*job->save) (fp, image);
                job->state &= ~(VC_START);
            } else {
                // we can allow state or frame changes after this
                pthread_mutex_unlock (&(app->capturing_mutex));
                printf ("Could not acquire pixmap!\n");
            }
        } else {
            // we're recording and not in the first frame ....
            // so we just read new data into the present image structure
            if (! frame_moved) {
                int num_dmg_rects, rcount;
                Box *dmg_rects;

                // then lock the display so we capture a consitent state
                XLockDisplay (app->dpy);
                // sync the display
                XSync (app->dpy, False);
                // first get the consolidated region where stuff was damaged
                // since the last frame
                damaged_region = xvc_get_damage_region ();
                // add the last position of the mouse pointer to the damaged
                // region
                if (app->mouseWanted > 0) {
                    // clip pointer_area to capture area
                    // this needs to be done here, because the captuer area
                    // might have been moved since the last frame
                    if (pointer_area.x < app->area->x &&
                        (pointer_area.x + pointer_area.width) > app->area->x) {
                        pointer_area.width -= (app->area->x - pointer_area.x);
                        pointer_area.x = app->area->x;
                    }
                    if ((pointer_area.x + pointer_area.width) >
                        (app->area->x + app->area->width)) {
                        pointer_area.width =
                            (app->area->x + app->area->width) - pointer_area.x;
                    }
                    if (pointer_area.y < app->area->y &&
                        (pointer_area.y + pointer_area.height) > app->area->y) {
                        pointer_area.height -= (app->area->y - pointer_area.y);
                        pointer_area.y = app->area->y;
                    }
                    if ((pointer_area.y + pointer_area.height) >
                        (app->area->y + app->area->height)) {
                        pointer_area.height =
                            (app->area->y + app->area->height) - pointer_area.y;
                    }
                    if (!
                        ((pointer_area.x + pointer_area.width) < app->area->x
                         || pointer_area.x > (app->area->x + app->area->width)
                         || (pointer_area.y + pointer_area.height) <
                         app->area->y
                         || pointer_area.y >
                         (app->area->y + app->area->height))) {
                        XUnionRectWithRegion (&(pointer_area), damaged_region,
                                              damaged_region);
                    }

                }
                // get individual rectangles from the damaged region
                dmg_rects = damaged_region->rects;
                num_dmg_rects = damaged_region->numRects;

                // then iterate across them and capture the content of the
                // rectangles
                for (rcount = 0; rcount < num_dmg_rects; rcount++) {
                    int bpl;
                    int x =
                        XVC_MIN (dmg_rects[rcount].x1, dmg_rects[rcount].x2);
                    int y =
                        XVC_MIN (dmg_rects[rcount].y1, dmg_rects[rcount].y2);
                    int width =
                        abs (dmg_rects[rcount].x1 - dmg_rects[rcount].x2);
                    int height =
                        abs (dmg_rects[rcount].y1 - dmg_rects[rcount].y2);

                    // either x11 or shm source
                    switch (capfunc) {
                    case SHM:
                        XGetZPixmapSHM (app->dpy,
                                        app->root_window,
                                        &dmg_shminfo, shm_opcode,
                                        dmg_image->data, x, y, width, height);
                        break;
                    case X11:
                    default:
                        XGetZPixmap (app->dpy,
                                     app->root_window,
                                     dmg_image->data, x, y, width, height);
                    }
                    /* the following assumes lines in images retrieved from
                     * X11 will always be aligned to 4-byte boundaries. You
                     * can determine the alignment from the bytes_per_line
                     * member of an XImage. However, for performance reasons,
                     * we do not always retrieve a complete XImage. */
                    bpl =
                        ((width * (image->bits_per_pixel >> 3)) % 4 > 0 ?
                         ((width * (image->bits_per_pixel >> 3)) / 4) * 4 + 4 :
                         width * (image->bits_per_pixel >> 3));
                    // update the content of the damaged areas in the frame
                    // to encode
                    placeImageInImage (dmg_image->data,
                                       x - app->area->x,
                                       y - app->area->y,
                                       width,
                                       bpl,
                                       height, image->data,
                                       image->width,
                                       image->bytes_per_line,
                                       image->height,
                                       image->bits_per_pixel >> 3);
                }
                // save the current mouse pointer location for further
                // reference during capture of next frame, only get it here
                // for minimizing locking
                if (app->mouseWanted > 0) {
                    x_cursor = getCurrentPointerImage ();
                }
                // now we can release the lock on the display again
                XUnlockDisplay (app->dpy);

                // paint the mouse pointer here, outside the lock
                pointer_area = paintMousePointer (image, x_cursor, 0, 0);
                XDestroyRegion (damaged_region);
            } else {

                // lock the display for consistency
                XLockDisplay (app->dpy);

                switch (capfunc) {
                case SHM:
                    captureFrameToImageSHM(app->dpy, image);
                    break;
                case X11:
                default:
                    captureFrameToImage (app->dpy, image);
                }
                if (app->mouseWanted > 0) {
                    x_cursor = getCurrentPointerImage ();
                }
                // unlock display again
                XUnlockDisplay (app->dpy);

                if (app->mouseWanted > 0) {
                    pointer_area = paintMousePointer (image, x_cursor, 0, 0);
                }
            }

            // we can allow state or frame changes after this
            pthread_mutex_unlock (&(app->capturing_mutex));

            // call the necessary XtoXYZ function to process the image
            (*job->save) (fp, image);
        }

        // this again is for recording, no matter if first frame or any
        // other close the image file if single frame capture mode
        if (app->current_mode == 0) {

            fclose (fp);
        }
        // substract the time we needed for creating and saving the frame
        // to the file
        gettimeofday (&curr_time, NULL);
        time1 = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - time;

        time = checkCaptureDuration (time, time1);
        // time the next capture

        job->pic_no += target->step;

        // this might be a single step. If so, remove the state flag so we
        // don't keep single stepping
        if (job->state & VC_STEP) {
            job->state &= ~(VC_STEP);
            // the time is the pause between this and the next snapshot
            // for step mode this makes no sense and could be 0. Setting
            // it to 50 is just to give the led meter the chance to flash
            // before being set blank again
            time = 50;
        }
        // the following means VC_STATE != VC_REC
        // this can only happen in capture.c if we were capturing and are
        // just stopping
    } else {
        int orig_state;

        // clean up
      CLEAN_CAPTURE:

        time = 0;
        orig_state = job->state;       // store state here, esp. VC_CONTINUE
        job->state = VC_STOP;
        // we can allow state or frame changes after this
        pthread_mutex_unlock (&(app->capturing_mutex));

        if (full_cleanup) {
            if (image) {
                XDestroyImage (image);
                image = NULL;
            }
            if (capfunc == SHM)
                XShmDetach (app->dpy, &shminfo);

            if (capfunc == SHM)
                 XShmDetach (app->dpy, &dmg_shminfo);
            if (dmg_image)
                 XDestroyImage (dmg_image);

            // clean up the save routines in xtoXXX.c
            if (job->clean)
                (*job->clean) ();
        }
        // set the sensitive stuff for the control panel if we don't
        // autocontinue
        if ((orig_state & VC_CONTINUE) == 0)
            xvc_idle_add (xvc_capture_stop, job);

        // if we're recording to a movie, we must close the file now
        if (app->current_mode > 0)
            if (fp)
                fclose (fp);
        fp = NULL;

        if ((orig_state & VC_CONTINUE) == 0) {
            // after this we're ready to start recording again
            job->state |= VC_READY;
        } else if (job->capture_returned_errno == 0) {

            // prepare autocontinue
            job->movie_no += 1;
            job->pic_no = target->start_no;
            job->state |= (VC_START | VC_REC);
            job->state &= ~(VC_STOP);

            return time;
        }
    }

    return time;
}

/**
 * \brief function used for capturing. This one is used with source = x11,
 *      i. e. when capturing from X11 display w/o SHM
 *
 * @return the number of msecs in which the next capture is due
 */
long
xvc_capture_x11 ()
{
    return commonCapture(X11);
}

/**
 * \brief function used for capturing. This one is used with source = shm,
 *      i. e. when capturing from X11 with SHM support
 *
 * @return the number of msecs in which the next capture is due
 */
long
xvc_capture_shm ()
{
    return commonCapture (SHM);
}
