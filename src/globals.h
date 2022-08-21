#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>
#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

/* extern globals */
extern int run;
extern int jack_reconnect;
extern int shutdown;
extern pthread_cond_t jack_trigger_cond;
extern int daemonize;
extern int verbosity;
extern char *jack_uid;
extern char *jack_gid;

#define pprintf(level, ...) do { \
	if ((level) <= verbosity) { \
		if (daemonize) \
			syslog(LOG_INFO, __VA_ARGS__); \
		else \
			printf(__VA_ARGS__); \
	} \
} while(0)
  
/* prototypes */
extern void drop_privileges(char *setgid_group, char *setuid_user);
extern void restore_privileges();
extern void get_jack_uid();

#ifdef __cplusplus
}
#endif

#endif /* GLOBALS_H */

