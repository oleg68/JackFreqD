/* CPU frequency scaling by JACK-DSP load
 *
 * Copyright (C) 2010 Robin Gareus <robin@gareus.org>
 * based on powernowd 
 * (c) 2003-2008 John Clemens <clemej@alum.rpi.edu>
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <pthread.h>
#include <sys/fsuid.h>

#define pprintf(level, ...) do { \
	if (level <= verbosity) { \
		if (daemonize) \
			syslog(LOG_INFO, __VA_ARGS__); \
		else \
			printf(__VA_ARGS__); \
	} \
} while(0)

enum modes {
	LOWER,
	SAME,
	RAISE
};

typedef struct cpustats {
	unsigned long long user;
	unsigned long long mynice;
	unsigned long long system;
	unsigned long long idle;
	unsigned long long iowait;
	unsigned long long irq;
	unsigned long long softirq;
} cpustats_t;

typedef struct cpuinfo {
	unsigned int cpuid;
	unsigned int nspeeds;
	unsigned int max_speed;
	unsigned int min_speed;
	unsigned int current_speed;
	unsigned int speed_index;
	cpustats_t *last_reading;
	cpustats_t *reading;
	int fd;
	int wfd;
	char *sysfs_dir;
	int in_mhz; /* 0 = speed in kHz, 1 = speed in mHz */
	unsigned long *freq_table;
	int table_size;
	int threads_per_core;
	int scalable_unit;
} cpuinfo_t;


/** globals */
cpuinfo_t **all_cpus;
static char buf[1024];
int run = 1;

/* options */
int daemonize = 0;
int verbosity = 0;
int ignore_nice = 1;
int use_cpu_load = 0;
unsigned int poll = 1000; /* in msecs */
char *jack_uid = NULL;
char *jack_gid = NULL;
int jack_reconnect = 0;
unsigned int highwater_dsp = 50;
unsigned int lowwater_dsp = 10;
unsigned int highwater_cpu = 80;
unsigned int lowwater_cpu = 20;
unsigned int cores_specified = 0;
unsigned int step_specified = 0;
unsigned int step = 100000;  /* in kHz */

pthread_mutex_t poll_wait_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  jack_trigger_cond = PTHREAD_COND_INITIALIZER;

/* statistics */
unsigned int change_speed_count = 0;
time_t start_time = 0;

/** prototypes */
int jjack_open();
void jjack_close();
float jjack_poll();

int get_jack_proc (int *pid, int *gid);

#define SYSFS_TREE "/sys/devices/system/cpu/"
#define SYSFS_SETSPEED "scaling_setspeed"

#define VERSION	"0.1.0"

void help(void) {

	printf("JACKfreq Daemon v%s, (c) 2010 Robin Gareus\n", VERSION);
	printf("\nAvailable Options:\n");
	printf(" -h        Print this help message\n");
	printf(" -d        detach from terminal - daemonize\n");
	printf(" -v        Increase output verbosity, can be used more than once.\n");
	printf(" -q        Quiet mode, only emergency output.\n");
	printf("\n");
	printf(" -p #      Polling frequency in msecs (default = 1000)\n");
	printf(" -s #      Frequency step in kHz (default = 100000)\n");
	printf(" -c #      Specify number of threads per power-managed core\n");
	printf("\n");
	printf(" -P        Combine DSP and CPU load.\n");
	printf(" -n        Include 'nice'd processes in calculations (only with -P)\n");
	printf(" -U #      CPU usage upper limit percentage [0 .. 100, default 80]\n");
	printf(" -L #      CPU usage lower limit percentage [0 .. 100, default 20]\n");
	printf("\n");
	printf(" -u #      DSP usage upper limit percentage [0 .. 100, default 50]\n");
	printf(" -l #      DSP usage lower limit percentage [0 .. 100, default 10]\n");
	printf(" -w        wait for and re-connect to jackd.\n");
	printf(" -j <uid>  user-name or UID of jackd process (default: autodetect)\n");
	printf(" -J <gid>  group-name or GID of jackd process (default: autodetect)\n");
	printf("\n");
	return;
}

/**
 * Open a file and copy it's first 1024 bytes into the global "buf".
 * Zero terminate the buffer. 
 */
int read_file(const char *file, int fd, int new) {
	int n, err;
	
	if (new) {
		if ((fd = open(file, O_RDONLY)) == -1) {
			err = errno;
			perror(file);
			return err;
		}
	}
	
	lseek(fd, 0, SEEK_SET);
	if ((n = read(fd, buf, sizeof(buf)-1)) < 0) {
		err = errno;
		perror(file);
		close(fd);
		return err;
	}
	buf[n] = '\0';

	if (new)
		close(fd);
	
	return 0;
}

int set_speed(cpuinfo_t *cpu) {
	cpuinfo_t *save;
	int len, err, i;
	char writestr[100];
	/* 
	 * We need to set the current speed on all virtual CPUs that fall
	 * into this CPU's scalable unit.
	 */
	save = cpu;
	for (i = save->cpuid; i < (save->cpuid + save->threads_per_core); i++) {
		cpu = all_cpus[i];
		cpu->current_speed = save->freq_table[save->speed_index];
	}
	cpu = save;

	pprintf(3,"Setting speed to %d\n", cpu->current_speed);

	change_speed_count++;

	strncpy(writestr, cpu->sysfs_dir, 50);
	strncat(writestr, SYSFS_SETSPEED, 20);
	
	if ((cpu->wfd = open(writestr, O_WRONLY)) < 0) {
		err = errno;
		perror("Can't open scaling_setspeed");
		return err;
	}

	lseek(cpu->wfd, 0, SEEK_CUR);
	
	sprintf(writestr, "%d\n", (cpu->in_mhz) ?
			(cpu->current_speed / 1000) : cpu->current_speed); 

	pprintf(4,"str=%s", writestr);
	
	if ((len = write(cpu->wfd, writestr, strlen(writestr))) < 0) {
		err = errno;
		perror("Couldn't write to scaling_setspeed\n");
		return err;
	}

	if (len != strlen(writestr)) {
		printf("Could not write scaling_setspeed\n");
		return EPIPE;
	}

	close(cpu->wfd);
	cpu->wfd=0;

	return 0;
}

/*
 * Reads /proc/stat into buf, and parses the output.
 *
 * Format of line:
 * ...
 * cpu<id> <user> <nice> <system> <idle> <iowait> <irq> <softirq>
 */
int get_stat(cpuinfo_t *cpu) {
	char *p1, *p2, searchfor[10];
	int err;
	
	if ((err = read_file("/proc/stat", cpu->fd, 0)) != 0) {
		return err;
	}

	sprintf(searchfor, "cpu%d ", cpu->cpuid);

	p1 = strstr(buf, searchfor);
	if (p1 == NULL) {
		perror("Error parsing /proc/stat");
		return ENOENT;
	}
	
	p2 = p1+strlen(searchfor);

	memcpy(cpu->last_reading, cpu->reading, sizeof(cpustats_t));
	
	cpu->reading->user = strtoll(p2, &p2, 10);
	cpu->reading->mynice = strtoll(p2, &p2, 10);
	cpu->reading->system = strtoll(p2, &p2, 10);
	cpu->reading->idle = strtoll(p2, &p2, 10);
	cpu->reading->iowait = strtoll(p2, &p2, 10);
	cpu->reading->irq = strtoll(p2, &p2, 10);
	cpu->reading->softirq = strtoll(p2, &p2, 10);

	return 0;
}

float calc_stat(cpuinfo_t *cpu) {
	int err;
	float pct;
	unsigned long long usage, total;

	if ((err = get_stat(cpu)) < 0) {
		perror("Can't get stats");
		return -1;
	}

	total = (cpu->reading->user - cpu->last_reading->user) +
		(cpu->reading->system - cpu->last_reading->system) +
		(cpu->reading->mynice - cpu->last_reading->mynice) +
		(cpu->reading->idle - cpu->last_reading->idle) +
		(cpu->reading->iowait - cpu->last_reading->iowait) +
		(cpu->reading->irq - cpu->last_reading->irq) +
		(cpu->reading->softirq - cpu->last_reading->softirq);

	if (ignore_nice) { 
		usage = (cpu->reading->user - cpu->last_reading->user) +
			(cpu->reading->system - cpu->last_reading->system) +
			(cpu->reading->irq - cpu->last_reading->irq) +
			(cpu->reading->softirq - cpu->last_reading->softirq);
	} else {
		usage = (cpu->reading->user - cpu->last_reading->user) +
			(cpu->reading->mynice - cpu->last_reading->mynice) +
			(cpu->reading->system - cpu->last_reading->system) +
			(cpu->reading->irq - cpu->last_reading->irq) +
			(cpu->reading->softirq - cpu->last_reading->softirq);
	}
	
	pct = ((float)usage)/((float)total);
	return pct;
}

int change_speed(cpuinfo_t *cpu, enum modes mode) {
	if (cpu->cpuid != cpu->scalable_unit) 
		return 0;
	
	if (mode == RAISE) {
		cpu->speed_index = 0;
	} else {
		if (cpu->speed_index != (cpu->table_size-1))
			cpu->speed_index++;
	}
	pprintf(4,"mode=%d", mode);
	return set_speed(cpu);
}

/* 
 * Abuse glibc's qsort.  Compare function to sort list of frequencies in 
 * ascending order.
 */
int faked_compare(const void *a, const void *b) {
	unsigned long *a1 = (unsigned long *)a;
	unsigned long *b1 = (unsigned long *)b;

	if (*a1 < *b1) return 1;
	if (*a1 > *b1) return -1;

	return 0;
}

/**
 * Allocates and initialises the per-cpu data structures.
 */
int get_per_cpu_info(cpuinfo_t *cpu, int cpuid) {
	char cpustr[100], scratch[100], tmp[11], *p1;
	int fd, err;
	unsigned long temp;
	
	cpu->cpuid = cpuid;
	cpu->sysfs_dir = (char *)malloc(50*sizeof(char));
	if (cpu->sysfs_dir == NULL) {
		perror("Couldn't allocate per-cpu sysfs_dir");
		return ENOMEM;
	}
	memset(cpu->sysfs_dir, 0, (50*sizeof(char)));

	strncpy(cpu->sysfs_dir, SYSFS_TREE, 30);
	sprintf(cpustr, "cpu%d/cpufreq/", cpuid);
	strncat(cpu->sysfs_dir, cpustr, 20);
	
	strncpy(scratch, cpu->sysfs_dir, 50);
	strncat(scratch, "cpuinfo_max_freq", 18);
	if ((err = read_file(scratch, 0, 1)) != 0) {
		return err;
	}
	
	cpu->max_speed = strtol(buf, NULL, 10);
	
	strncpy(scratch, cpu->sysfs_dir, 50);
	strncat(scratch, "cpuinfo_min_freq", 18);

	if ((err = read_file(scratch, 0, 1)) != 0) {
		return err;
	}

	cpu->min_speed = strtol(buf, NULL, 10);

	/* 
	 * More error handling, make sure step is not larger than the 
	 * difference between max and min speeds. If so, truncate it.
	 */
	if (step > (cpu->max_speed - cpu->min_speed)) {
		step = cpu->max_speed - cpu->min_speed;
	}
	
	/* XXXjc read the real current speed */
	cpu->current_speed = cpu->max_speed;
	cpu->speed_index = 0;

	strncpy(scratch, cpu->sysfs_dir, 50);
	strncat(scratch, "scaling_available_frequencies", 50);

	if (((err = read_file(scratch, 0, 1)) != 0) || (step_specified)) {
		/* 
		 * We don't have scaling_available_frequencies. build the
		 * table from the min, max, and step values.  the driver
		 * could ignore these, but we'll represent it this way since
		 * we don't have any other info.
		 */
		cpu->table_size = ((cpu->max_speed-cpu->min_speed)/step) + 1;
		cpu->table_size += ((cpu->max_speed-cpu->min_speed)%step)?1:0;
		
		cpu->freq_table = (unsigned long *)
			malloc(cpu->table_size*sizeof(unsigned long));

		if (cpu->freq_table == (unsigned long *)NULL) {
			perror("couldn't allocate cpu->freq_table");
			return ENOMEM;
		}

		/* populate the table.  Start at the top, and subtract step */
		for (temp = 0; temp < cpu->table_size; temp++) {
			cpu->freq_table[temp] = 
			((cpu->min_speed<(cpu->max_speed-(temp*step))) ? 
			 (cpu->max_speed-(temp*step)) :
			 (cpu->min_speed) );
		}	
	} else {
		/* 
		 * We do have the file, parse it and build the table from
		 * there.
		 */ 
		/* The format of scaling_available_frequencies (SAF) is:
		 * "number<space>number2<space>...numberN<space>\n", but this
		 * can change. So we're relying on the fact that strtoul will 
		 * return 0 if it can't find anything, and that 0 will never 
		 * be a real value for the available frequency. 
		 */
		p1 = buf;
		
		temp = strtoul(p1, &p1, 10);
		while((temp > 0) && (cpu->table_size < 100)) {
			cpu->table_size++;
			temp = strtoul(p1, &p1, 10);
		}
	
		cpu->freq_table = (unsigned long *)
			malloc(cpu->table_size*sizeof(unsigned long));
		if (cpu->freq_table == (unsigned long *)NULL) {
			perror("Couldn't allocate cpu->freq_table\n");
			return ENOMEM;
		}
	
		p1 = buf;
		for (temp = 0; temp < cpu->table_size; temp++) {
			cpu->freq_table[temp] = strtoul(p1, &p1, 10);
		}
	}

	/* now lets sort the table just to be sure */
	qsort(cpu->freq_table, cpu->table_size, sizeof(unsigned long), 
			&faked_compare);
	
	strncpy(scratch, cpu->sysfs_dir, 50);
	strncat(scratch, "scaling_governor", 20);

	if ((err = read_file(scratch, 0, 1)) != 0) {
		perror("couldn't open scaling_governors file");
		return err;
	}

	if (strncmp(buf, "userspace", 9) != 0) {
		if ((fd = open(scratch, O_RDWR)) < 0) {
			err = errno;
			perror("couldn't open govn's file for writing");
			return err;
		}
		strncpy(tmp, "userspace\n", 11);
		if (write(fd, tmp, 11*sizeof(char)) < 0) {
			err = errno;
			perror("Error writing file governor");
			close(fd);
			return err;
		}
		if ((err = read_file(scratch, fd, 0)) != 0) {
			perror("Error reading back governor file");
			close(fd);
			return err;
		}
		close(fd);
		if (strncmp(buf, "userspace", 9) != 0) {
			perror("Can't set to userspace governor, exiting");
			return EPIPE;
		}
	}
	
	cpu->last_reading = (cpustats_t *)malloc(sizeof(cpustats_t));
	cpu->reading = (cpustats_t *)malloc(sizeof(cpustats_t));
	memset(cpu->last_reading, 0, sizeof(cpustats_t));
	memset(cpu->reading, 0, sizeof(cpustats_t));
	
	/*
	 * Some cpufreq drivers (longhaul) report speeds in MHz instead
	 * of KHz.  Assume for now that any currently supported cpufreq 
	 * processor will a) not be faster then 10GHz, and b) not be slower
	 * then 10MHz. Therefore, is the number for max_speed is less than
	 * 10000, assume the driver is reporting speeds in MHz, not KHz,
	 * and adjust accordingly.
	 *
	 * XXXjc the longhaul driver has been fixed (2.6.5ish timeframe)
	 * so this should't be needed anymore.  Remove for 1.0?
	 */
	cpu->in_mhz = 0;
	if (cpu->max_speed <= 10000) {
		cpu->in_mhz = 1;
		cpu->max_speed *= 1000;
		cpu->min_speed *= 1000;
		cpu->current_speed *= 1000;
	}
	
	if (use_cpu_load) {
		if ((cpu->fd = open("/proc/stat", O_RDONLY)) < 0) {
			err = errno;
			perror("can't open /proc/stat");
			return err;
		}
		
		if ((err = get_stat(cpu)) < 0) {
			perror("can't read /proc/stat");
			return err;
		}
	}
	return 0;
}

/********************************************************************/

/*
 * The heart of the program... decide to raise or lower the speed.
 */
enum modes inline decide_speed(cpuinfo_t *cpu, float dspload) {
	if (use_cpu_load) {
		float pct;
		if ((pct = calc_stat(cpu)) < 0) {
			return SAME; // error
		}
		if (((dspload > highwater_dsp) || (pct >= ((float)highwater_cpu/100.0))) 
				&& (cpu->current_speed != cpu->max_speed)) {
			return RAISE;
		}
		else if (((dspload < lowwater_dsp) && (pct <= ((float)lowwater_cpu/100.0))) 
		         && (cpu->current_speed != cpu->min_speed)) {
			return LOWER;
		}
		return SAME;
	}

	if (dspload > highwater_dsp && (cpu->current_speed != cpu->max_speed)) {
		return RAISE;
	}
	else if (dspload < lowwater_dsp && (cpu->current_speed != cpu->min_speed)) {
		return LOWER;
	}
	return SAME;
}

/********************************************************************/

/*
 * Signal handler for SIGTERM/SIGINT... clean up after ourselves
 */
void terminate(int signum) {
	static int term = 0;
	if (term) {
		pprintf(1, "DEBUG: terminate while shutdown in progres.\n");
		exit(1);
	}
	term=1;
	run=0;

	int ncpus, i;
	cpuinfo_t *cpu;
	
	pprintf(4,"exiting: resetting CPU to full speed..\n");

	ncpus = sysconf(_SC_NPROCESSORS_CONF);
	if (ncpus < 1) ncpus = 1;
	
	/* 
	 * for each cpu, force it back to full speed.
	 * don't mix this with the below statement.
	 * 
	 * 5 minutes ago I convinced myself you couldn't 
	 * mix these two, now I can't remember why.  
	 */
	for(i = 0; i < ncpus; i++) {
		cpu = all_cpus[i];
		change_speed(cpu, RAISE);
	}

	pprintf(4,"exiting: cleaning up 1/2.\n");

	for(i = 0; i < ncpus; i++) {
		cpu = all_cpus[i];
		if (cpu->wfd) close(cpu->wfd);
		if (cpu->fd) close(cpu->fd);
		free(cpu->last_reading);
		free(cpu->reading);
		free(cpu->sysfs_dir);
		free(cpu->freq_table);
		free(cpu);
	}
	pprintf(4,"exiting: cleaning up 2/2.\n");
	free(all_cpus);
	if (jack_uid) free(jack_uid);
	if (jack_gid) free(jack_gid);

	pprintf(4,"exiting: closing JACK connection\n");
	jjack_close();

	time_t duration = time(NULL) - start_time;
	pprintf(1,"Statistics:\n");
	pprintf(1,"  %d speed changes in %d seconds\n",
			change_speed_count, (unsigned int) duration);
	pprintf(0,"JACKfreqd Daemon Exiting.\n");

	closelog();

	exit(0);
}

void get_jack_uid() {
	int uid,gid;
	if (get_jack_proc (&uid, &gid)) return;
	if (jack_uid) free(jack_uid);
	if (jack_gid) free(jack_gid);
	jack_uid=calloc(16,sizeof(char));
	jack_gid=calloc(16,sizeof(char));
	sprintf(jack_uid,"%i", uid);
	sprintf(jack_gid,"%i", gid);
	pprintf(1, "jackd: uid:%i gid:%i\n", uid, gid);
}

/* set process user and group(s) id */
void drop_privileges(char *setgid_group, char *setuid_user) {
	uid_t uid=0, gid=0;
	struct group *gr;
	struct passwd *pw;

	/* Get the integer values */
	if(setgid_group) {
		gr=getgrnam(setgid_group);
		if(gr) 
			gid=gr->gr_gid;
		else if(atoi(setgid_group)) /* numerical? */
			gid=atoi(setgid_group);
		else {
			pprintf(0, "Failed to get GID for group %s\n", setgid_group);
			terminate(0);
		}
	}
	if(setuid_user) {
		pw=getpwnam(setuid_user);
		if(pw)
			uid=pw->pw_uid;
		else if(atoi(setuid_user)) /* numerical? */
			uid=atoi(setuid_user);
		else {
			pprintf(0, "Failed to get UID for user %s\n", setuid_user);
			terminate(0);
		}
	}
	if (gid || uid) 
		pprintf(4, "set user: uid:%i gid:%i  -> uid:%i gid:%i\n",
				getuid(),getgid(),uid,gid);

	/* Set uid and gid */
	if(gid) {
#if 0
		if(setfsgid(gid)) {
			pprintf(0, "setfsgid failed.\n");
			terminate(0);
		}
#endif
		if(setresgid(gid, gid, (uid_t)0)) {
			pprintf(0, "setgid failed.\n");
			terminate(0);
		}
	}
	if(uid) {
#if 0
		if(setfsuid(uid)) {
			pprintf(0, "setfsuid failed.\n");
			terminate(0);
		}
#endif
		if(setresuid(uid, uid, (uid_t)0)) {
			pprintf(0, "setuid failed.\n");
			terminate(0);
		}
	}
}

void restore_privileges() {
	setresuid((uid_t)-1, (uid_t)0, (uid_t)0);
	setresgid((uid_t)-1, (uid_t)0, (uid_t)0);
#if 0
	setfsgid((uid_t)0);
	setfsuid((uid_t)0);
#endif
}


/* Generic x86 cpuid function lifted from kernel sources */
/*
 * Generic CPUID function
 */
static inline void cpuid(int op, int *eax, int *ebx, int *ecx, int *edx) {
	__asm__("cpuid"
					: "=a" (*eax),
						"=b" (*ebx),
						"=c" (*ecx),
						"=d" (*edx)
					: "0" (op));
}

/* 
 * A little bit of black magic to try and detect the number of cores per
 * processor.  This will have to be added on to for every architecture, 
 * as we learn how to detect them from userspace.  Note: This method
 * assumes uniform processor/thread ID's.  First look for affected_cpus, 
 * if not, then fall back to the cpuid check, or just default to 1. 
 *
 * By 'thread' in this case, I mean 'entity that is part of one scalable
 * instance'.  For example, a P4 with hyperthreading has 2 threads in 
 * one scalable instace.  So does an Athlon X2 dual core, because each
 * core has to have the same speed.  The new Yonah, on the other hand, may
 * have two scalable elements, as rumors say you can control both cores' 
 * speed individually.  Lets hope the speedstep driver populates affected_cpus
 * correctly...
 *
 * You can always override this by using the -c command line option to 
 * specify the number of threads per core.  If you do so, it will do a static
 * mapping, uniform for all real processors in the system.  Actually, so 
 * will this one, because there's no way for me to bind to a processor.
 * (yet. :)
 */
int determine_threads_per_core(int ncpus) {
	char filename[100], *p1;
	int err, count;

	/* if ncpus is one, we don't care */
	if (ncpus == 1) return 1;
	
	/* 
	 * First look for the affected_cpus file, and count the 
	 * number of cpus that supports.  Assume this is true for all
	 * cpus on the system.
	 */
	strncpy(filename, SYSFS_TREE, 30);
	strncat(filename, "cpu0/cpufreq/affected_cpus", 99-strlen(filename));
	
	/* 
	 * OK, the funkiest system I can think of right now is
	 * Sun's Niagara processor, which I think would have 32
	 * "cpus" in one scalable element.  So make this robust 
	 * enough to at least handle more than 32 affected cpus 
	 * at once.
	 *
	 * NOTE: I don't even know if Niagara supports scaling, 
	 * I'm dealing with hypotheticals. 
	 */
	
	count = 1;
	
	if ((err = read_file(filename, 0, 1)) == 0) {
		p1 = buf;
		err = strtoul(p1, &p1, 5);
		/* 
		 * The first cpu should always be 0, so err should be 0
		 * after the first read.  If its anything else, default to
		 * one, print a message, and move on. 
		 */
		if (err != 0) {
			pprintf(0, "WARN: cpu0 scaling doesn't affect cpu 0?"
				       " Assuming 1 thread per core.\n");
			return 1;
		}
		while ((err = strtol(p1, &p1, 5)) != 0)
			count ++;
		pprintf(4, "about to return count = %d\n", count);
		return count;	
	}
	pprintf(0,"err=%d", err);
	
#ifdef __i386__ 
	/* Only get here if there's no affected_cpus file. */
	/* 
	 * XXXjc fix eventually to run on each processor so you
	 * can support mixed multi and single-core cpus. Need to know
	 * how to force ourselves to run on one particular processor.
	 */
	int eax,ebx,ecx,edx, num=1;

	cpuid(1,&eax,&ebx,&ecx,&edx);

	/* 
	 * Do we support hyperthreading?
	 * AMD's dual-core will masquerade as HT, so this should work 
	 * for them too. (update: it does but this doesn't emulate
	 * the extra ebx parameter, appearently).
	 */
	if(edx & 0x08000000) { 
		/* 
		 * if so, is it enabled? If so, how many threads 
		 * are enabled?  Thank you, sandpile.org and the LKML. 
		 */
		num = (ebx & 0x00FF0000) >> 16;
	}
	/* 
	 * if num = 0, default to 1.  Other non-multiples of cpus will
	 * be taken care of later
	 * XXXjc, rewrite to use sysfs (affected_cpus).
	 */
	return ((num)?num:1);
#endif
	/* always default to one thread per core */
	return 1;
}

int main (int argc, char **argv) {
	cpuinfo_t *cpu;
	int ncpus, i, j, err, num_real_cpus, threads_per_core, cpubase;
	struct timespec pollts;
	enum modes change, change2;

	/* Parse command line args */
	while(1) {
		int c;

		c = getopt(argc, argv, "dnvqPwc:u:U:s:l:L:j:J:h");
		if (c == -1)
			break;

		switch(c) {
			case 'd':
				daemonize = 1;
				break;
			case 'P':
				use_cpu_load = 1;
				break;
			case 'n':
				ignore_nice = 0;
				break;
 			case 'v':
 				verbosity++;
				if (verbosity > 5) verbosity = 5;
 				break;
 			case 'q':
 				verbosity = -1;
 				break;
			case 'c':
				cores_specified = strtol(optarg, NULL, 10);
				if (cores_specified < 1) {
					printf("invalid number of cores/proc");
					help();
					exit(ENOTSUP);
				}
				break;
			case 's':
				step = strtol(optarg, NULL, 10);
				if (step < 0) {
					printf("step must be non-negative");
					help();
					exit(ENOTSUP);
				}
				step_specified = 1;
				pprintf(2,"Using %dHz step.\n", step);
				break;
			case 'p':
				poll = strtol(optarg, NULL, 10);
				if (poll < 0) {
					printf("poll must be non-negative");
					help();
					exit(ENOTSUP);
				}
				pprintf(2,"Polling every %d msecs\n", poll);
				break;
			case 'u':
				highwater_dsp = strtol(optarg, NULL, 10);
				if ((highwater_dsp < 0) || (highwater_dsp > 100)) {
					printf("upper limit must be between 0 and 100\n");
					help();
					exit(ENOTSUP);
				}
				pprintf(2,"Using upper pct of %d%%\n",highwater_dsp);
				break;
			case 'l':
				lowwater_dsp = strtol(optarg, NULL, 10);
				if ((lowwater_dsp < 0) || (lowwater_dsp > 100)) {
					printf("lower limit must be between 0 and 100\n");
					help();
					exit(ENOTSUP);
				}
				pprintf(2,"Using lower pct of %d%%\n",lowwater_dsp);
				break;
			case 'U':
				highwater_cpu = strtol(optarg, NULL, 10);
				if ((highwater_cpu < 0) || (highwater_cpu > 100)) {
					printf("upper limit must be between 0 and 100\n");
					help();
					exit(ENOTSUP);
				}
				pprintf(2,"Using upper pct of %d%%\n",highwater_cpu);
				break;
			case 'L':
				lowwater_cpu = strtol(optarg, NULL, 10);
				if ((lowwater_cpu < 0) || (lowwater_cpu > 100)) {
					printf("lower limit must be between 0 and 100\n");
					help();
					exit(ENOTSUP);
				}
				pprintf(2,"Using lower pct of %d%%\n",lowwater_cpu);
				break;
			case 'j':
				if (jack_uid) free(jack_uid);
				jack_uid = strdup(optarg);
				break;
			case 'J':
				if (jack_gid) free(jack_gid);
				jack_gid = strdup(optarg);
				break;
			case 'w':
				jack_reconnect =1;
				break;
			case 'h':
			default:
				help();
				return 0;
		}
	}

	if (jack_reconnect && !use_cpu_load) {
		printf("WARNING: jack-reconnect should be used with -P\n");
		printf("with no jackd: CPU freq scaling will never be triggered.\n");
	}


	if (lowwater_cpu > highwater_cpu) {
		printf("Invalid: lower pct higher than upper pct!\n");
		help();
		exit(ENOTSUP);
	}

	if (lowwater_dsp > highwater_dsp) {
		printf("Invalid: lower pct higher than upper pct!\n");
		help();
		exit(ENOTSUP);
	}

	/* so we don't interfere with anything, including ourself */
	nice(5);

	if (daemonize)
		openlog("jackfreqd", LOG_AUTHPRIV|LOG_PERROR, LOG_DAEMON);

	if (getuid() != 0) {
		printf("jackfreqd requires root permissions\n");
		exit(EPERM);
	}

	ncpus = sysconf(_SC_NPROCESSORS_CONF);
	if (ncpus < 0) {
		perror("sysconf could not determine number of cpus, assuming 1\n");
		ncpus = 1;
	}
	
	if (cores_specified) {
		if (ncpus < cores_specified) {
			printf("\nWARNING: bogus # of thread per core, assuming 1\n");
			threads_per_core = 1;
		} else {
			threads_per_core = cores_specified;
		}
	} else { 
		threads_per_core = determine_threads_per_core(ncpus);
		if (threads_per_core < 0) 
			threads_per_core = 1;
	}
	
	/* We don't support mixed configs yet */
	if (!ncpus || !threads_per_core || ncpus % threads_per_core) {	
		printf("WARN: ncpus(%d) is not a multiple of threads_per_core(%d)!\n",
			ncpus, threads_per_core);
		printf("WARN: Assuming 1.\n");
		threads_per_core = 1;
		/*help(); exit(ENOTSUP); */
	}
	
	num_real_cpus = ncpus/threads_per_core;

	/* Malloc, initialise data structs */
	all_cpus = (cpuinfo_t **) malloc(sizeof(cpuinfo_t *)*ncpus);
	if (all_cpus == (cpuinfo_t **)NULL) {
		perror("Couldn't malloc all_cpus");
		return ENOMEM;
	}
	
	for (i=0; i<ncpus; i++) {
		all_cpus[i] = (cpuinfo_t *)malloc(sizeof(cpuinfo_t));
		if (all_cpus[i] == (cpuinfo_t *)NULL) {
			perror("Couldn't malloc all_cpus");
			return ENOMEM;
		}
		memset(all_cpus[i],0,sizeof(cpuinfo_t));
	}
	
	for (i=0;i<ncpus;i++) {
		all_cpus[i]->threads_per_core = threads_per_core;
		all_cpus[i]->scalable_unit = (i/threads_per_core)*threads_per_core;
	}
	
	pprintf(0,"Found %d scalable unit%s:  -- %d 'CPU%s' per scalable unit\n",
			num_real_cpus,
			(num_real_cpus>1)?"s":"",
			threads_per_core,
			(threads_per_core>1)?"s":"");
	
	for (i=0;i<ncpus;i++) {
		cpu = all_cpus[i];
		if ((err = get_per_cpu_info(cpu, i)) != 0) {
			printf("\n");
			printf("JACKfreqd encountered and error and could not start.\n");
	    free(cpu);
			exit(err);
		}
		pprintf(0,"  cpu%d: %dMhz - %dMhz (%d steps)\n", 
				cpu->cpuid,
				cpu->min_speed / 1000, 
				cpu->max_speed / 1000, 
				cpu->table_size);
		for(j=0;j<cpu->table_size; j++) {
			pprintf(1, "     step%d : %ldMhz\n", j+1, 
					cpu->freq_table[j] / 1000);
		}
	}

	if (!jack_uid && !jack_gid) {
		get_jack_uid();
	}

	if (!jack_uid && !jack_reconnect) {
		pprintf(0, "No JACK-uid given or detected.\n");
		terminate(0);
	}

	/* need to deaemonize before connecting to jackd */
	if (daemonize)
		daemon(0, 0);

	if (jjack_open() && !jack_reconnect) {
		pprintf(0, "Failed to connect to jackd\n");
		terminate(0);
	}

	/* now that everything's all set up, lets set up a exit handler */
	signal(SIGTERM, terminate);
	signal(SIGINT, terminate);
	
	start_time = time(NULL);


	pthread_mutex_lock(&poll_wait_lock);

	/* Now the main program loop */
	while(run) {
		clock_gettime(CLOCK_REALTIME, &pollts);
		pollts.tv_sec += poll/1000;
		if (pollts.tv_nsec + ((poll%1000)*1000000) >= 1000000000) {
			pollts.tv_sec += 1;
		}
		pollts.tv_nsec = (pollts.tv_nsec + ((poll%1000)*1000000))%1000000000;
		pthread_cond_timedwait(&jack_trigger_cond, &poll_wait_lock, &pollts);

		float jack_load = jjack_poll();
		pprintf(4, "dsp load: %.3f\n", jack_load);

		for(i=0; i<num_real_cpus; i++) {
			change = LOWER;
			cpubase = i*threads_per_core;
			pprintf(4, "i = %d, cpubase = %d, ",i,cpubase);
			/* handle SMT/CMP here */
			for (j=0; j<all_cpus[cpubase]->threads_per_core; j++) {
				change2 = decide_speed(all_cpus[cpubase+j], jack_load);
				pprintf(4, "change = %d, change2 = %d\n",change,change2);
				if (change2 > change)
					change = change2;
			}
			if (change != SAME) {
				if ((err=change_speed(all_cpus[cpubase], change))) {
#if 0
					pprintf(0, "changing CPU speed failed.\n");
					terminate(0);
#else 
					pprintf(2, "changing CPU speed failed.\n");
#endif
				}
			}
		}
	}

	terminate(0);
	return 0;
}
