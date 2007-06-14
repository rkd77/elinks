/*
Simple non-interactive CD Player

Copyright (C) 2007 Witold Filipczyk
Email: witekfl@poczta.onet.pl
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.  See the file COPYING.

This program is distributed in hope that it will be useful,
but WITHOUT ANY WARRANTY; without even an implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include <cdaudio.h>
#include <string.h>

int drive;
struct disc_info disc;
struct disc_status status;

static inline int
getCurrentTrack(void)
{
	return status.status_current_track;
}

static void
prev(void)
{
	int track;
	
	track = getCurrentTrack();
	if (track > disc.disc_first_track)
		cd_play(drive, --track);
}

static void
next(void)
{
	int track;
	
	track = getCurrentTrack();
	if (track < disc.disc_total_tracks)
		cd_play(drive, ++track);
}
	
static
void pauseORcont()
{
	if (status.status_mode == CDAUDIO_PAUSED)
		cd_resume(drive);
	else
		cd_pause(drive);
}
	
int
main(int argc, char **argv)
{
	drive = cd_init_device("/dev/cdrom");
	if (drive >= 0) {
		cd_stat(drive, &disc);
		cd_poll(drive, &status);
		if (argc > 1) {
			char *t = strrchr(argv[1], '-');
			if (t) {
				switch ((char)*++t) {
				case 'F': /* First */
					cd_play(drive, disc.disc_first_track);
					break;
				case 'L': /* Last */
					cd_play(drive, disc.disc_total_tracks);
					break;
				case 'N': /* Next */
					next();
					break;
				case 'P': 
					if (t[1] == 'r') prev(); /* Prev */
					else pauseORcont(); /* Pause|Resume */
					break;
				default:
					break;
				}
				return cd_finish(drive);
			    }
			}
		cd_play(drive, disc.disc_first_track);
		return cd_finish(drive);
	}
	return 1;
}
