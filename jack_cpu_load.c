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
	jack_options_t options = JackNullOption;
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

