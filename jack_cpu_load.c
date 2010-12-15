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
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sched.h>
#include <pthread.h>
#include <jack/jack.h>

/* prototypes */
void drop_privileges(char *setgid_group, char *setuid_user);
void restore_privileges();
void get_jack_uid();

/* extern globals */
extern int run;
extern int jack_reconnect;
extern pthread_cond_t jack_trigger_cond;
extern int daemonize;
extern int verbosity;
extern char *jack_uid;
extern char *jack_gid;

#define pprintf(level, ...) do { \
	if (level <= verbosity) { \
		if (daemonize) \
			syslog(LOG_INFO, __VA_ARGS__); \
		else \
			printf(__VA_ARGS__); \
	} \
} while(0)

jack_client_t *client = NULL;

void jack_shutdown (void *arg) {
	pprintf (1, "jack-shutdown received.\n");
	if (jack_reconnect) {
		client=NULL;
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

int jjack_open () {
	jack_options_t options = JackNoStartServer;
	jack_status_t status;

	// drop priv to jack-user
	if (!jack_uid) get_jack_uid();
	if (!jack_uid) return -1;

	pprintf(4, "DEBUG: uid:%i euid=%i gid:%i egid:%i\n", getuid(),geteuid(), getgid(), getegid());

	drop_privileges(jack_gid, jack_uid);

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
	pthread_yield();
	sched_yield();
	usleep(64000); /* guess: one jack period should be enough 1024*3/48k */
	sched_yield();
	pthread_yield();

	restore_privileges();
	pprintf (3, "connected to JACKd\n");
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
	if (!client) {
		// TODO limit retries to 1 per 3 sec or so.
		if (!jack_reconnect) return -1;
		if (jjack_open()) return -1;
	}
	return(jack_cpu_load(client));
}

