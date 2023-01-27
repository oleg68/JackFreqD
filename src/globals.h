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
#define pprintf(level, ...) do { \
	if ((level) <= verbosity) { \
		if (daemonize) \
			syslog(LOG_INFO, __VA_ARGS__); \
		else \
			printf(__VA_ARGS__); \
	} \
} while(0)

typedef struct {
  int pid;
  int uid;
  int gid;
} ProcessInfo;

/**
 * Find a jack server process
 * @param filter_uid if != 0, then search only among processes owned by the user
 * @param filter_gid if != 0, then search only among processes owned by the group
 * @param jack_server_process. The structure with the process found. If not found then pid == 0
 * @return 0 - not found; 1 - found
 */
extern int get_jack_proc(
  int filter_uid, int filter_gid, ProcessInfo *jack_server_process
);
extern int get_xdg_runtime_dir (int pid, char *runtime_dir);

/* prototypes */
extern void drop_privileges(const ProcessInfo *jack_server_process);
extern void restore_privileges();
extern void get_jack_uid(
  char *filter_uid, char* filter_gid,
  char **jack_uid, char **jack_gid, char **jack_pid
);
extern int jjack_is_open();
extern int jjack_open(const ProcessInfo *jack_server_process);
extern void jjack_close();
extern float jjack_poll();


#ifdef __cplusplus
}
#endif

#endif /* GLOBALS_H */

