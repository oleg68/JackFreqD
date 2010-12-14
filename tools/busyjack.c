/**
 * simple JACK client that causes DSP load
 *
 * based on 
 * http://subversion.ardour.org/svn/ardour2/branches/3.0/tools/jacktest.c
 */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <jack/jack.h>

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;
int loopsize = 25000;
int xrun_occured = 0;
int consecutive_xruns = 0;
float first_xrun = 0.0f;
float last_load = 0.0f;
int at_loop = 0;
int at_loop_size;
char *thechunk;
float minload = 50.0;
float maxload = 100.0;
int chunksize = 1024 * 1024 * 10;
int loop = 1;

void
fooey (jack_nframes_t n)
{
	volatile int x;
	thechunk[random()%chunksize] = n;
}

int
process (jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t *in, *out;
	int i;

	in = jack_port_get_buffer (input_port, nframes);
	out = jack_port_get_buffer (output_port, nframes);

	memcpy (out, in, sizeof (jack_default_audio_sample_t) * nframes);
	
	for (i = 0; i < loopsize; ++i) {
		fooey (nframes);
	}
	
	last_load = jack_cpu_load (client);

	if ((at_loop += nframes) >= at_loop_size) {
		// TODO use a PID algorithm 
		if (last_load < minload/4) {
			loopsize *= 2;
		} else if (last_load < minload/2) {
			loopsize = (int) (1.1 * loopsize);
		} else if (last_load < minload*3/4) {
			loopsize += (int) (0.10 * loopsize);
		} else if (last_load < minload) {
			loopsize += (int) (0.03 * loopsize);
		}
		if (last_load > maxload) {
			loopsize -= (int) (0.01 * loopsize);
		  if (loopsize < 1) loopsize=1;
		}
#if 0
		if (last_load < 25.0f) {
			loopsize *= 2;
		} else if (last_load < 49.0f) {
			loopsize = (int) (1.1 * loopsize);
		} 
		else if (last_load < 90.0f) {
			loopsize += (int) (0.10 * loopsize);
		} else if (last_load < 95.0f) {
			loopsize += (int) (0.05 * loopsize);
		} else {
			loopsize += (int) (0.001 * loopsize);
		}
#endif
		at_loop = 0;
		printf ("loopsize = %d\n", loopsize);
	}

	if (xrun_occured) {
		if (consecutive_xruns == 0) {
			first_xrun = last_load;
		}
		consecutive_xruns++;
	}

	xrun_occured = 0;
#if 0
	if (consecutive_xruns >= 10) {
		fprintf (stderr, "Stopping with load = %f (first xrun at %f)\n", last_load, first_xrun);
		exit (0);
	}
#endif

	return 0;      
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
	fprintf (stderr, "shutdown with load = %f\n", last_load);
	exit (1);
}

int
jack_xrun (void *arg)
{
	fprintf (stderr, "xrun occured with loop size = %d\n", loopsize);
	xrun_occured = 1;
	return 0;
}

void
terminate (int signum)
{
  loop = 0;
}

int
main (int argc, char *argv[])
{
	const char **ports;
	const char *client_name;
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	client_name = "busyjack";
	if (argc > 1) {
		minload = atof (argv[1]);
	}
	if (argc > 2) {
		loopsize = atoi (argv[2]);
	}
	if (argc > 3) {
		chunksize = atoi (argv[3]);
	}
	
	if (   maxload <1   || maxload>100.0
			|| minload <0.0 || minload >100.0
			|| minload > maxload) {
		printf("invalid configuration.\n");
		exit(1);
	}
	maxload=minload+1;

	printf ("set DSP load = %.2f\n", minload);
	printf ("initial loopsize %d iterations\n", loopsize);
	printf ("using chunksize of %d bytes\n", chunksize);

	/* open a client connection to the JACK server */

	client = jack_client_open (client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "jack_client_open() failed, status = 0x%x\n",
			 status);
		if (status & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
		exit (1);
	}
	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

	jack_set_process_callback (client, process, 0);
	jack_set_xrun_callback (client, jack_xrun, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	/* create two ports */

	input_port = jack_port_register (client, "input",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput, 0);
	output_port = jack_port_register (client, "output",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);

	if ((input_port == NULL) || (output_port == NULL)) {
		fprintf(stderr, "no more JACK ports available\n");
		exit (1);
	}

	at_loop_size = jack_get_sample_rate (client) * 2;
	if ((thechunk = (char *) malloc (chunksize)) == NULL) {
		fprintf (stderr, "cannot allocate chunk\n");
		exit (1);
	}

	/* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		exit (1);
	}
#if 0
	/* connect the ports. Note: you can't do this before the
	   client is activated, because we can't allow connections to
	   be made to clients that aren't running.
	*/

	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsOutput);
	if (ports == NULL) {
		fprintf(stderr, "no physical capture ports\n");
		exit (1);
	}

	if (jack_connect (client, ports[0], jack_port_name (input_port))) {
		fprintf (stderr, "cannot connect input ports\n");
	}

	free (ports);
#endif
	
	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL) {
		fprintf(stderr, "no physical playback ports\n");
		exit (1);
	}

	if (jack_connect (client, jack_port_name (output_port), ports[0])) {
		fprintf (stderr, "cannot connect output ports\n");
	}

	free (ports);

	signal(SIGTERM, terminate);
	signal(SIGINT, terminate); 
	while (loop) {
		sleep (1);
	}

	jack_client_close (client);
	exit (0);
}
