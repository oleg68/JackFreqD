/*
 * Copyright (C) 2010 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <jack/jack.h>

/* prototypes */
void terminate(int signum);

jack_client_t *client = NULL;

void jack_shutdown (void *arg) {
	fprintf (stderr, "jack-shutdown received, exiting ...\n");
	terminate(0);
}

int jjack_open () {
	jack_options_t options = JackNoStartServer;
	jack_status_t status;

	client = jack_client_open ("jack_cpu_load", options, &status);
	if (!client) {
		fprintf (stderr, "jack_client_open() failed, "
		                 "status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
				fprintf (stderr, "Unable to connect to JACK server\n");
		}
		return(1);
	}
  
	jack_on_shutdown (client, jack_shutdown, 0);

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return (1);
	}
	return (0);
}

void jjack_close () {
	if (client) {
		jack_deactivate (client);
		jack_client_close (client);
	}
	client=NULL;
}

float jjack_poll () {
	//if (!client) return 0;
	return(jack_cpu_load(client));
}

