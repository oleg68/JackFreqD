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
#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sched.h>
#include <pthread.h>
#include <jack/jack.h>

#include "globals.h"

jack_client_t *client = NULL;

void jack_shutdown (void *arg) {
	pprintf (1, "jack-shutdown received.\n");
	if (jack_reconnect) {
		shutdown=1;
	} else {
		run=0;
	}
}

void jack_trigger_port (jack_port_id_t a, jack_port_id_t b, int connect, void *arg) {
	pprintf (4, "jack-port-connect trigger..\n");
	pthread_cond_signal(&jack_trigger_cond);
}

int jack_trigger_graph (void *arg) {
	pprintf (4, "jack-graph trigger..\n");
	pthread_cond_signal(&jack_trigger_cond);
	return 0;
}

int jjack_is_open() { return client != NULL; }

int jjack_open (const ProcessInfo *jack_server_process) {
	jack_options_t options = JackNoStartServer;
	jack_status_t status;

	// drop priv to jack-user
	pprintf(4, "DEBUG: uid:%i euid=%i gid:%i egid:%i\n", getuid(),geteuid(), getgid(), getegid());
	drop_privileges(jack_server_process);

	pprintf(4, "DEBUG: Connecting to a jack server\n");
	client = jack_client_open ("jack_cpu_load", options, &status);
	if (!client) {
		restore_privileges();
		pprintf (jack_reconnect?3:0, "jack_client_open() failed, "
		    "status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
				pprintf (jack_reconnect?3:0, "Unable to connect to JACK server\n");
		}
		client=NULL;
		return(1);
	}
	pprintf(1, "Connected to the jack server\n");

	jack_on_shutdown (client, jack_shutdown, 0);
	jack_set_graph_order_callback(client, jack_trigger_graph, NULL);
#if 0
	jack_set_port_connect_callback(client, jack_trigger_port, NULL);
#endif

	if (jack_activate (client)) {
		restore_privileges();
		pprintf (jack_reconnect?3:0, "cannot activate client\n");
		client=NULL;
		return (1);
	}

  /* workaround - let jack finish initialization
	 * before returning to root UID
	 */
	sched_yield();
	usleep(64000); /* guess: one jack period should be enough 1024*3/48k */
	sched_yield();

	restore_privileges();
	pprintf (3, "connected to JACKd\n");
	return (0);
}

void jjack_close () {
	if (client) {
		jack_deactivate (client);
		jack_client_close (client);
		pprintf(1, "Disconnected from the jack server\n");
	}
	client=NULL;
}

float jjack_poll (int filter_uid, int filter_gid, ProcessInfo *jack_server_process)
{
  return client ? jack_cpu_load(client) : 0.0;
}

