/** jackxrun - simple instrumentation tool to report JACK x-runs
 *
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <jack/jack.h>

jack_client_t *client;
int xrun_occured = 0;
int loop = 1;

void terminate (int signum) {
	printf("total x-runs: %i\n", xrun_occured);
  loop = 0;
}

void jack_shutdown (void *arg) {
	terminate(0);
}

int jack_xrun (void *arg) {
	fprintf (stderr, "xrun occured.\n");
	xrun_occured++;
	return 0;
}

int main (int argc, char *argv[]) {
	const char *client_name = "jackxrun";
	const char *server_name = NULL;
	jack_options_t options = JackNoStartServer;
	jack_status_t status;

	client = jack_client_open (client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "jack_client_open() failed, status = 0x%x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
		exit (1);
	}
	jack_set_xrun_callback (client, jack_xrun, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		exit (1);
	}

	signal(SIGTERM, terminate);
	signal(SIGINT, terminate); 
	while (loop) {
		sleep (1);
	}

	jack_client_close (client);

	if (xrun_occured) exit (1);
	else exit (0);
}
