/**
 * Contains common functions for saving and loading preferences.
 */

/*
 * Copyright (C) 1997,98 Rasca, Berlin
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

#ifndef _xvc_XVC_OPTIONS_H__
#define _xvc_XVC_OPTIONS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>

#include "job.h"
#include "app_data.h"
#include "codecs.h"
#include "xvidcap-intl.h"

#define OPS_FILE ".xvidcaprc"

/**
 * Saves the preferences to file.
 *
 * @return TRUE on success, FALSE otherwise
 */
Boolean xvc_write_options_file();

/**
 * Read options file
 *
 * @return TRUE on success, FALSE otherwise
 */
Boolean xvc_read_options_file();

#endif     // _xvc_XVC_OPTIONS_H__