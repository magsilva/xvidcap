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

#include "preferences.h"

/**
 * Saves the preferences to file.
 *
 * @return TRUE on success, FALSE otherwise
 */
Boolean
xvc_write_options_file ()
{
    int n, m;
    char *home;
    char file[PATH_MAX + 1];
    FILE *fp;
    XVC_AppData *app = xvc_appdata_ptr();

    // save it to $HOME/<OPS_FILE>
    home = getenv("HOME");
    sprintf(file, "%s/%s", home, OPS_FILE);
    fp = fopen(file, "w");
    if (! fp) {
		fprintf(stderr, "Could not write to configuration file %s", OPS_FILE);
        return FALSE;
    }

    fprintf (fp, _("#\n# xvidcap configuration file\n#\n\n"));

    // general options
    fprintf (fp, _("# default capture mode (0 = single-frame, 1 = multi-frame)\n"));
    fprintf (fp, "default_mode: %i\n", app->default_mode);

	fprintf (fp, _("# capture source\n"));
    fprintf (fp, "source: %s\n", app->source);

    fprintf (fp, _("# hide GUI\n"));
    fprintf (fp, "nogui: %d\n", ((app->flags & FLG_NOGUI) ? 1 : 0));
	
    fprintf (fp, _("# hide frame around capture area\n"));
    fprintf (fp, "noframe: %d\n", ((app->flags & FLG_NOFRAME) ? 1 : 0));

	fprintf (fp, _("# device to grab audio from\n"));
    fprintf (fp, "audio_in: %s\n", ((strcmp (app->snddev, "pipe:") == 0) ? "-" : app->snddev));

	// TODO: Add a third and fourth mode (yellow highlight and activation)
	fprintf (fp, _("# what kind of mouse pointer should be recorded? 0 = none, 1 = white, 2 = black\n"));
    fprintf (fp, "mouse_wanted: %i\n", app->mouseWanted);

	fprintf (fp, _("# should cap_geometry be saved to prefs?\n"));
    fprintf (fp, "save_geometry: %d\n", ((app->flags & FLG_SAVE_GEOMETRY) ? 1 : 0));
    if ((app->flags & FLG_SAVE_GEOMETRY) > 0) {
        fprintf (fp, _("# area to capture as in --capture_geometry\n"));
        fprintf (fp, "cap_geometry: %ix%i", app->area->width, app->area->height);
        if (app->area->x >= 0 && app->area->y >= 0)
            fprintf (fp, "+%i+%i\n", app->area->x, app->area->y);
        else
            fprintf (fp, "\n");
    }
	
    fprintf (fp, _("# locked frame follows mouse not window\n"));
    fprintf (fp, "follow_mouse: %d\n",  ((app->flags & FLG_LOCK_FOLLOWS_MOUSE) ? 1 : 0));

    fprintf (fp, _("# show time rather than frame count\n"));
    fprintf (fp, "show_time: %d\n", ((app->flags & FLG_SHOW_TIME) ? 1 : 0));

	fprintf (fp, _("# toggle autocontinue (0/1) \n"));
    fprintf (fp, "auto_continue: %i\n", ((app->flags & FLG_AUTO_CONTINUE) ? 1 : 0));
	
    fprintf (fp, _("# always show results dialog (0/1)\n"));
    fprintf (fp, "always_show_results: %i\n", ((app->flags & FLG_ALWAYS_SHOW_RESULTS) ? 1 : 0));
	
    fprintf (fp, _("# rescale the captured area to n percent of the original\n"));
    fprintf (fp, "rescale: %i\n", (app->rescale));

	fprintf (fp, _("# minimize the main control to the system tray while recording\n"));
    fprintf (fp, "minimize_to_tray: %i\n", ((app->flags & FLG_TO_TRAY) ? 1 : 0));

    fprintf (fp, _("# file pattern for single-frame capture\n(this defines the filetype to write via the extension provided)\n"));
	fprintf (fp, "\nsf_file: %s\n", app->single_frame.file);

	fprintf (fp, _("# file format - use AUTO to select format through file extension\n"));
    fprintf (fp, "\nsf_format: %s\n", xvc_formats[app->single_frame.target].name);

	fprintf (fp, _("# video codec used by ffmpeg - use AUTO to auto-detect codec\n"));
	fprintf (fp, "\n#sf_codec: %s\n", xvc_video_codecs[app->single_frame.targetCodec].name);
	
    fprintf (fp, _("# audio codec\n"));
    fprintf (fp, "sf_au_codec: %s\n", "NONE");
	
    fprintf (fp, _("# frames per second\n# put as normal decimal number or a fraction like \"30000/10001\"\n"));
    fprintf (fp, "sf_fps: %i/%i\n", app->single_frame.fps.num, app->single_frame.fps.den);

	fprintf (fp, _("# max time (0 = unlimited)\n"));
    fprintf (fp, "sf_max_time: %i\n", app->single_frame.time);

	fprintf (fp, _("# max frames (0 = unlimited)\n"));
    fprintf (fp, "sf_max_frames: %d\n", app->single_frame.frames);

	fprintf (fp, _("# number to start counting individual frames from\n"));
    fprintf (fp, "sf_start_no: %i\n", app->single_frame.start_no);

	fprintf (fp, _("# quality (JPEG/MPEG)\n"));
    fprintf (fp, "sf_quality: %d\n", app->single_frame.quality);

	fprintf (fp, _("# toggle audio capture (0/1)\n"));
    fprintf (fp, "sf_audio: 0\n");

	fprintf (fp, _("# sample rate for audio capture\n"));
    fprintf (fp, "sf_audio_rate: 0\n");

	fprintf (fp, _("# bit rate for audio capture\n"));
    fprintf (fp, "sf_audio_bits: 0\n");

	fprintf (fp, _("# number of channels to use in audio capture\n"));
    fprintf (fp, "sf_audio_channels: 0\n");
	
    fprintf (fp, _("# command to display captured frames as animation\n"));
    fprintf (fp, "sf_animate_cmd: %s\n", app->single_frame.play_cmd);

	fprintf (fp, _("# command to edit current frame\n"));
    fprintf (fp, "sf_edit_cmd: %s\n", app->single_frame.edit_cmd);

	fprintf (fp, _("# command to encode captured frames\n"));
    fprintf (fp, "sf_video_cmd: %s\n", app->single_frame.video_cmd);

    fprintf (fp, _("\n#options for multi-frame capture ...\n"));
    fprintf (fp, _("# file pattern\n# this defines the filetype to write via the extension provided\n"));
    fprintf (fp, "\nmf_file: %s\n", app->multi_frame.file);

	fprintf (fp, _("# file format - use AUTO to select format through file extension\n"));
    fprintf (fp, "\nmf_format: %s\n", xvc_formats[app->multi_frame.target].name);

	fprintf (fp, _("# video codec used by ffmpeg - use AUTO to auto-detect codec\n"));
    fprintf (fp, "\nmf_codec: %s\n", xvc_video_codecs[app->multi_frame.targetCodec].name);
	
    fprintf (fp, _("# audio codec used by ffmpeg - use AUTO to auto-detect audio codec\n"));
    fprintf (fp, "\nmf_au_codec: %s\n", xvc_audio_codecs[app->multi_frame.au_targetCodec].name);
	
    fprintf (fp, _("# frames per second\n# put as normal decimal number or a fraction like \"30000/10001\"\n"));
    fprintf (fp, "mf_fps: %i/%i\n", app->multi_frame.fps.num, app->multi_frame.fps.den);
	
    fprintf (fp, _("# max time (0 = unlimited)\n"));
    fprintf (fp, "mf_max_time: %i\n", app->multi_frame.time);

	fprintf (fp, _("# max frames (0 = unlimited)\n"));
    fprintf (fp, "mf_max_frames: %d\n", app->multi_frame.frames);

	fprintf (fp, _("# number to start counting individual frames from\n"));
    fprintf (fp, "mf_start_no: %i\n", app->multi_frame.start_no);
	
    fprintf (fp, _("# quality (JPEG/MPEG)\n"));
    fprintf (fp, "mf_quality: %d\n", app->multi_frame.quality);

	fprintf (fp, _("# toggle audio capture (0/1)\n"));
    fprintf (fp, "mf_audio: %i\n", ((app->multi_frame.audioWanted == 1) ? 1 : 0));

	fprintf (fp, _("# sample rate for audio capture\n"));
    fprintf (fp, "mf_audio_rate: %i\n", app->multi_frame.sndrate);

	fprintf (fp, _("# bit rate for audio capture\n"));
    fprintf (fp, "mf_audio_bits: %i\n", app->multi_frame.sndsize);

	fprintf (fp, _("# number of channels to use in audio capture\n"));
    fprintf (fp, "mf_audio_channels: %i\n", app->multi_frame.sndchannels);

	fprintf (fp, _("# command to display captured frames as animation\n"));
    fprintf (fp, "mf_animate_cmd: %s\n", app->multi_frame.play_cmd);

	fprintf (fp, _("# command to edit current movie\n"));
    fprintf (fp, "mf_edit_cmd: %s\n", app->multi_frame.edit_cmd);

	fprintf (fp, _("# command to encode captured frames (subject to change)\n"));
    fprintf (fp, "mf_video_cmd: %s\n", app->multi_frame.video_cmd);

    fclose (fp);

    return TRUE;
}

/**
 * Read options file
 *
 * @return TRUE on success, FALSE otherwise
 */
Boolean
xvc_read_options_file()
{
    char line[260];
    char *home;
    char file[PATH_MAX + 1];
    FILE *fp;
    XVC_AppData *app = xvc_appdata_ptr();

    home = getenv("HOME");
    sprintf(file, "%s/%s", home, OPS_FILE);
    fp = fopen(file, "r");
    if (! fp) {
		fprintf(stderr, "Could not read the configuration file %s", OPS_FILE);
		return FALSE;	
	}

	while (fgets (line, 255, fp)) {
		char *token, *value;
		char *n_hash;
		char *c_hash = strchr(line, '#');

		// ignore comments
		if (c_hash != NULL)
		    sprintf (c_hash, "\0");
		
		// get rid of newlines
		n_hash = strchr (line, '\n');
		if (n_hash != NULL)
		    sprintf (n_hash, "\0");

		// if smth. is left, parse line
		if (strlen (line) > 0) {
		    char low_token[260];
		    int y;

		    token = strtok (line, " :=\"");
		    // this has found the first token
		    // for the value we need special treatment for the command
		    // parameters which consist of multiple words
		    // all other values are just a single token
		    for (y = 0; y < strlen (token); y++) {
		        low_token[y] = tolower (token[y]);
		    }
		    low_token[strlen (token)] = '\0';
		    if (strstr (low_token, "_animate_cmd") != NULL
		        || strstr (low_token, "_edit_cmd") != NULL
		        || strstr (low_token, "_video_cmd") != NULL
		        || strcasecmp (token, "help_cmd") == 0) {
		        int x = 1;

		        while (line[strlen (token) + x] == ' '
		               || line[strlen (token) + x] == ':'
		               || line[strlen (token) + x] == '=')
		            x++;
		        value = strdup (&line[strlen (token) + x]);
		    } else {
		        value = strtok (NULL, " :=\"");
		        if (!value)
		            value = "";
		    }

		    // general options first ...
		    if (strcasecmp (token, "default_mode") == 0) {
		        app->default_mode = atoi (value);
		    }
		    if (strcasecmp (token, "source") == 0) {
		        app->source = strdup (value);
		    }
			if (strcasecmp (token, "nogui") == 0) {
		        if (atoi (value) == 1)
		            app->flags |= FLG_NOGUI;
		        else if (atoi (value) == 0)
		            app->flags &= ~FLG_NOGUI;
		        else {
		            app->flags &= ~FLG_NOGUI;
		            fprintf (stderr, _("reading unsupported GUI preferences from options file\nresetting to GUI not hidden\n"));
		        }
		    }
			if (strcasecmp (token, "noframe") == 0) {
		        if (atoi (value) == 1)
		            app->flags |= FLG_NOFRAME;
		        else if (atoi (value) == 0)
		            app->flags &= ~FLG_NOFRAME;
		        else {
		            app->flags &= ~FLG_NOFRAME;
		            fprintf (stderr, _("reading unsupported noframe preferences from options file\nresetting to frame not hidden\n"));
		        }
		    }
			if (strcasecmp (token, "audio_in") == 0) {
		        app->snddev = strdup (value);
		    }
		    if (strcasecmp (token, "mouse_wanted") == 0) {
		        app->mouseWanted = atoi (value);
		    }
			if (strcasecmp (token, "save_geometry") == 0) {
		        if (atoi (value) == 1)
		            app->flags |= FLG_SAVE_GEOMETRY;
		        else if (atoi (value) == 0)
		            app->flags &= ~FLG_SAVE_GEOMETRY;
		        else {
		            app->flags &= ~FLG_SAVE_GEOMETRY;
		            fprintf (stderr, _("reading unsupported save_geometry preferences from options file\nresetting to capture geometry not saved\n"));
		        }
		    }
			if (strcasecmp (token, "follow_mouse") == 0) {
		        if (atoi (value) == 1)
		            app->flags |= FLG_LOCK_FOLLOWS_MOUSE;
		        else if (atoi (value) == 0)
		            app->flags &= ~FLG_LOCK_FOLLOWS_MOUSE;
		        else {
		            app->flags &= ~FLG_LOCK_FOLLOWS_MOUSE;
		            fprintf (stderr, _("reading unsupported follow_mouse preferences from options file\nresetting lock to follow control window\n"));
		        }
		    }
			if (strcasecmp (token, "cap_geometry") == 0) {
		        sscanf (value, "%hux%hu+%hi+%hi", &(app->area->width), &(app->area->height), &(app->area->x), &(app->area->y));
		    }
			if (strcasecmp (token, "show_time") == 0) {
		        // FIXME: this is not at all implemented, yet
		        if (atoi (value) == 1)
		            app->flags |= FLG_SHOW_TIME;
		        else if (atoi (value) == 0)
		            app->flags &= ~FLG_SHOW_TIME;
		        else {
		            app->flags &= ~FLG_SHOW_TIME;
		            fprintf (stderr, _("reading unsupported show_time value from options file\nresetting to display frame counter\n"));
		        }
		    }
			if (strcasecmp (token, "auto_continue") == 0) {
		        if (atoi (value) == 1)
		            app->flags |= FLG_AUTO_CONTINUE;
		        else if (atoi (value) == 0)
		            app->flags &= ~FLG_AUTO_CONTINUE;
		        else {
		            app->flags |= FLG_AUTO_CONTINUE;
		            fprintf (stderr, _("reading unsupported auto_continue value from options file\nresetting to autocontinue enabled.\n"));
		        }
		    }
			if (strcasecmp (token, "always_show_results") == 0) {
		        if (atoi (value) == 1)
		            app->flags |= FLG_ALWAYS_SHOW_RESULTS;
		        else if (atoi (value) == 0)
		            app->flags &= ~FLG_ALWAYS_SHOW_RESULTS;
		        else {
		            app->flags |= FLG_ALWAYS_SHOW_RESULTS;
		            fprintf (stderr, _("reading unsupported always_show_results value from options file\nresetting to always show results.\n"));
		        }
		    }
			if (strcasecmp (token, "rescale") == 0) {
		        if (value)
		            app->rescale = atoi (value);
		    }
			if (strcasecmp (token, "minimize_to_tray") == 0) {
		        if (atoi (value) == 1)
		            app->flags |= FLG_TO_TRAY;
		        else if (atoi (value) == 0)
		            app->flags &= ~FLG_TO_TRAY;
		        else {
		            app->flags &= ~FLG_TO_TRAY;
		            fprintf (stderr, _("reading unsupported minimize_to_tray value from options file\nresetting to not minimizing to tray.\n"));
		        }
		    }
		
			if (strcasecmp (token, "sf_file") == 0) {
		        app->single_frame.file = strdup (value);
		    }
			if (strcasecmp (token, "sf_format") == 0) {
		        int found_target = 0;
		        for (int i = 0; i < xvc_count_formats(); i++) {
		            if (strcasecmp (xvc_formats[i].name, token) == 0)
		                found_target = i;
		        }
		        app->single_frame.target = found_target;
		    }
			if (strcasecmp(token, "sf_codec") == 0) {
		        int found_codec = 0;
		        for (int i = VID_CODEC_NONE; i < xvc_count_video_codecs(); i++) {
		            if (strcasecmp (xvc_video_codecs[i].name, token) == 0)
		                found_codec = i;
		        }
		        app->single_frame.targetCodec = found_codec;
		    }
		    if (strcasecmp (token, "sf_au_codec") == 0) {
		        int found_auCodec = 0;

		        for (int i = AU_CODEC_NONE; i < xvc_count_audio_codecs(); i++) {
		            if (strcasecmp(xvc_audio_codecs[i].name, token) == 0)
		                found_auCodec = i;
		        }
		        app->single_frame.au_targetCodec = found_auCodec;
		    }
		    if (strcasecmp (token, "sf_fps") == 0) {
		        app->single_frame.fps = xvc_read_fps_from_string (value);
		    }
			if (strcasecmp (token, "sf_max_time") == 0) {
		        app->single_frame.time = atoi (value);
		        if (atof (value) > 0)
		            app->single_frame.frames = 0;
		    }
			if (strcasecmp (token, "sf_max_frames") == 0) {
		        app->single_frame.frames = atoi (value);
		        if (atoi (value) > 0)
		            app->single_frame.time = 0;
		    }
			if (strcasecmp (token, "sf_start_no") == 0) {
		        app->single_frame.start_no = atof (value);
		    }
			if (strcasecmp (token, "sf_quality") == 0) {
		        app->single_frame.quality = atoi (value);
		        if ((app->single_frame.quality < 0) || (app->single_frame.quality > 100))
					app->single_frame.quality = 75; /* reset to default */
		    }
			if (strcasecmp (token, "sf_audio_rate") == 0) {
		        // for future use
		    }
			if (strcasecmp (token, "sf_ audio_bits") == 0) {
		        // for future use
		    }
			if (strcasecmp (token, "sf_audio_channels") == 0) {
		        // for future use
		    }
			if (strcasecmp (token, "sf_animate_cmd") == 0) {
		        app->single_frame.play_cmd = strdup (value);
		    }
			if (strcasecmp (token, "sf_edit_cmd") == 0) {
		        app->single_frame.edit_cmd = strdup (value);
		    }
			if (strcasecmp (token, "sf_video_cmd") == 0) {
		        app->single_frame.video_cmd = strdup (value);
		    }

			// now multi-frame capture options
		    if (strcasecmp (token, "mf_file") == 0) {
		        app->multi_frame.file = strdup (value);
		    }
			if (strcasecmp (token, "mf_format") == 0) {
		        int n, a = -1;

		        for (n = CAP_AVI; n < xvc_count_formats(); n++) {
		            if (strcasecmp (value, xvc_formats[n].name) == 0)
		                a = n;
		        }
		        if (strcasecmp (value, "NONE") == 0 || strcasecmp (value, "AUTO") == 0) {
		            // flag set target by filename through setting
		            a = CAP_AVI;
		        }
		        if (a < 0) {
		            fprintf (stderr, _("reading unsupported target from options file\nresetting to target auto-detection\n"));
		            a = CAP_AVI;
		        }
	            app->multi_frame.target = a;
		    }
			if (strcasecmp (token, "mf_codec") == 0) {
		        int n, a = -1;

		        for (n = VID_CODEC_NONE; n < xvc_count_video_codecs(); n++) {
		            if (strcasecmp (value, xvc_video_codecs[n].name) == 0)
		                a = n;
		        }
		        if (strcasecmp (value, "AUTO") == 0)
		            a = VID_CODEC_NONE;
		        if (a < VID_CODEC_NONE) {
		            fprintf (stderr, _("reading unsupported target codec (%i) from options file\nresetting to codec auto-detection\n"), a);
		            a = VID_CODEC_NONE;
		        }
		        app->multi_frame.targetCodec = a;
		    }
		    if (strcasecmp (token, "mf_au_codec") == 0) {
		        int n, a = -1;

		        for (n = AU_CODEC_NONE; n < xvc_count_audio_codecs(); n++) {
		            if (strcasecmp (value, xvc_audio_codecs[n].name) == 0)
		                a = n;
		        }
		        if (strcasecmp (value, "AUTO") == 0)
		            a = AU_CODEC_NONE;
		        if (a < AU_CODEC_NONE) {
		            fprintf (stderr, _("reading unsupported target audio codec (%i) from options file\nresetting to audio codec auto-detection\n"), a);
		            a = AU_CODEC_NONE;
		        }
		        app->multi_frame.au_targetCodec = a;
		    }
			if (strcasecmp (token, "mf_fps") == 0) {
		        app->multi_frame.fps = xvc_read_fps_from_string (value);
		    }
			if (strcasecmp (token, "mf_max_time") == 0) {
		        app->multi_frame.time = atoi (value);
		        if (atof (value) > 0)
		            app->multi_frame.frames = 0;
		    }
			if (strcasecmp (token, "mf_max_frames") == 0) {
		        app->multi_frame.frames = atoi (value);
		        if (atoi (value) > 0)
		            app->multi_frame.time = 0;
		    }
			if (strcasecmp (token, "mf_start_no") == 0) {
		        app->multi_frame.start_no = atof (value);
		    }
			if (strcasecmp (token, "mf_quality") == 0) {
		        app->multi_frame.quality = atoi (value);
		        if ((app->multi_frame.quality < 0) || (app->multi_frame.quality > 100))
		            app->multi_frame.quality = 75;  /* reset to default */
		    }
		    if (strcasecmp (token, "mf_audio") == 0) {
		        if (strcasecmp (value, "0") == 0) {
		            app->multi_frame.audioWanted = 0;
		        } else if (strcasecmp (value, "1") == 0) {
		            app->multi_frame.audioWanted = 1;
		        } else {
		            fprintf (stderr, _("Invalid parameter 'audio: %s' in preferences file\n"), value);
		            fprintf (stderr, _("Disabling audio capture unless overridden by command line\n"));
		            app->multi_frame.audioWanted = 0;
		        }
		    }
			if (strcasecmp (token, "mf_audio_rate") == 0) {
		        if (value)
		            app->multi_frame.sndrate = atoi (value);
		    }
			if (strcasecmp (token, "mf_audio_bits") == 0) {
		        if (value)
		            app->multi_frame.sndsize = atoi (value);
		    }
			if (strcasecmp (token, "mf_audio_channels") == 0) {
		        if (value)
		            app->multi_frame.sndchannels = atoi (value);
		    }
		    if (strcasecmp (token, "mf_animate_cmd") == 0) {
		        app->multi_frame.play_cmd = strdup (value);
		    }
			if (strcasecmp (token, "mf_edit_cmd") == 0) {
		        app->multi_frame.edit_cmd = strdup (value);
		    }
			if (strcasecmp (token, "mf_video_cmd") == 0) {
		        app->multi_frame.video_cmd = strdup (value);
		    }
	    }
	}
    return TRUE;
}
