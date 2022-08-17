/*
 * Copyright (C) 2010 Robin Gareus <robin@gareus.org>
 * 
 * this file contains code from the sysvinit suite
 * Copyright (C) 1991-2004 Miquel van Smoorenburg.
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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "globals.h"

#define LOG_ERR_FILE stderr

/* Info about a process. */
typedef struct proc {
	char *argv0;      /* Name as found out from argv[0] */
	char *argv0base;  /* `basename argv[1]` */
	char *argv1;      /* Name as found out from argv[1] */
	char *argv1base;  /* `basename argv[1]` */
	char *statname;	  /* the statname without braces */
	ino_t ino;        /* Inode number */
	dev_t dev;        /* Device it is on */
	pid_t pid;        /* Process ID. */
	pid_t sid;        /* Session ID. */
	uid_t uid;        /* User ID */
	uid_t gid;        /* Group ID */
	char kernel;      /* Kernel thread or zombie. */
	struct proc *next;/* Pointer to next struct. */
} PROC;

PROC *plist;

int readarg(FILE *fp, char *buf, int sz) {
	int		c = 0, f = 0;
	while (f < (sz-1) && (c = fgetc(fp)) != EOF && c)
		buf[f++] = c;
	buf[f] = 0;
	return (c == EOF && f == 0) ? c : f;
}

/*
 *	Read the proc filesystem.
 *	CWD must be /proc to avoid problems if / is affected by the killing (ie depend on fuse).
 */
int readproc() {
	DIR		*dir;
	FILE		*fp;
	PROC		*p, *n;
	struct dirent	*d;
	struct stat	st;
	char		path[PATH_MAX+1];
	char		buf[PATH_MAX+1];
	char		*s, *q;
	unsigned long	startcode, endcode;
	int		pid, f;

	/* Open the /proc directory. */
	if (chdir("/proc") == -1) {
		fprintf(LOG_ERR_FILE, "chdir /proc failed");
		return -1;
	}
	if ((dir = opendir(".")) == NULL) {
		fprintf(LOG_ERR_FILE, "cannot opendir(/proc)");
		return -1;
	}

	/* Free the already existing process list. */
	n = plist;
	for (p = plist; n; p = n) {
		n = p->next;
		if (p->argv0) free(p->argv0);
		if (p->argv1) free(p->argv1);
		if (p->statname) free(p->statname);
		free(p);
	}
	plist = NULL;

	/* Walk through the directory. */
	while ((d = readdir(dir)) != NULL) {

		/* See if this is a process */
		if ((pid = atoi(d->d_name)) == 0) continue;

		/* Get a PROC struct . */
		p = (PROC *)malloc(sizeof(PROC));
		memset(p, 0, sizeof(PROC));

		/* Open the status file. */
		snprintf(path, sizeof(path), "%s/stat", d->d_name);

		/* Read SID & statname from it. */
		if ((fp = fopen(path, "r")) != NULL) {
			buf[0] = 0;
			fgets(buf, sizeof(buf), fp);

			/* See if name starts with '(' */
			s = buf;
			while (*s != ' ') s++;
			s++;
			if (*s == '(') {
				/* Read program name. */
				q = strrchr(buf, ')');
				if (q == NULL) {
					p->sid = 0;
					fprintf(LOG_ERR_FILE,
					"can't get program name from /proc/%s\n",
						path);
					if (p->argv0) free(p->argv0);
					if (p->argv1) free(p->argv1);
					if (p->statname) free(p->statname);
					free(p);
					continue;
				}
				s++;
			} else {
				q = s;
				while (*q != ' ') q++;
			}
			*q++ = 0;
			while (*q == ' ') q++;
			p->statname = (char *)malloc(strlen(s)+1);
			strcpy(p->statname, s);

			/* Get session, startcode, endcode. */
			startcode = endcode = 0;
			if (sscanf(q, 	"%*c %*d %*d %d %*d %*d %*u %*u "
					"%*u %*u %*u %*u %*u %*d %*d "
					"%*d %*d %*d %*d %*u %*u %*d "
					"%*u %lu %lu",
					&p->sid, &startcode, &endcode) != 3) {
				p->sid = 0;
				fprintf(LOG_ERR_FILE, "can't read sid from %s\n", path);
				if (p->argv0) free(p->argv0);
				if (p->argv1) free(p->argv1);
				if (p->statname) free(p->statname);
				free(p);
				continue;
			}
			if (startcode == 0 && endcode == 0)
				p->kernel = 1;
			fclose(fp);
		} else {
			/* Process disappeared.. */
			if (p->argv0) free(p->argv0);
			if (p->argv1) free(p->argv1);
			if (p->statname) free(p->statname);
			free(p);
			continue;
		}

		snprintf(path, sizeof(path), "%s/cmdline", d->d_name);
		if ((fp = fopen(path, "r")) != NULL) {

			/* Now read argv[0] */
			f = readarg(fp, buf, sizeof(buf));

			if (buf[0]) {
				/* Store the name into malloced memory. */
				p->argv0 = (char *)malloc(f + 1);
				strcpy(p->argv0, buf);

				/* Get a pointer to the basename. */
				p->argv0base = strrchr(p->argv0, '/');
				if (p->argv0base != NULL)
					p->argv0base++;
				else
					p->argv0base = p->argv0;
			}

			/* And read argv[1] */
			while ((f = readarg(fp, buf, sizeof(buf))) != EOF)
				if (buf[0] != '-') break;

			if (buf[0]) {
				/* Store the name into malloced memory. */
				p->argv1 = (char *)malloc(f + 1);
				strcpy(p->argv1, buf);

				/* Get a pointer to the basename. */
				p->argv1base = strrchr(p->argv1, '/');
				if (p->argv1base != NULL)
					p->argv1base++;
				else
					p->argv1base = p->argv1;
			}

			fclose(fp);

		} else {
			/* Process disappeared.. */
			if (p->argv0) free(p->argv0);
			if (p->argv1) free(p->argv1);
			if (p->statname) free(p->statname);
			free(p);
			continue;
		}

		/* Try to stat the executable. */
		snprintf(path, sizeof(path), "/proc/%s/exe", d->d_name);

		if (lstat(path, &st) == 0) {
			p->dev = st.st_dev;
			p->ino = st.st_ino;
			p->uid = st.st_uid;
			p->gid = st.st_gid;
		}

		/* Link it into the list. */
		p->next = plist;
		plist = p;
		p->pid = pid;
	}
	closedir(dir);

	/* Done. */
	return 0;
}

const char *basename(const char *path) {
  const char *lastSlash = strrchr(path, '/');
  
  return lastSlash != NULL ? lastSlash + 1 : path;
}

int get_jack_proc (int *uid, int *gid, int *pid)
{
  PROC		*p;
  
  readproc();
  for (p = plist; p; p = p->next)
    if (p->argv0)
    {
      const char *exeName = basename(p->argv0);

      if (strcmp(exeName, "jackd") == 0)
      {
	pprintf(1, "Found jackd running; pid:%i '%s' u:%i g:%i\n",p->pid, p->argv0, p->uid, p->gid);
	if (uid) *uid=p->uid;
	if (gid) *gid=p->gid;
	if (pid) *pid=p->pid;
	return (0);
      }
      if (strcmp(exeName, "pipewire") == 0)
      {
	pprintf(1, "Found pipewire running; pid:%i '%s' u:%i g:%i\n",p->pid, p->argv0, p->uid, p->gid);
	if (uid) *uid=p->uid;
	if (gid) *gid=p->gid;
	if (pid) *pid=p->pid;
	return (0);
      }
    }
  return(-1);
}

int get_xdg_runtime_dir(char *pid, char *runtime_dir) {
	char path[32];
	char *buf = NULL;
	FILE *fp;
	int  offset = 0;
	int  size;
	
	sprintf(path, "/proc/%s/environ", pid);
	if ((fp = fopen(path, "r")) != NULL) {
		for (size=0; fgetc(fp) != EOF; size++) {}
		rewind(fp);
		buf = malloc(size + 1);
		fread(buf, size, 1, fp);
		fclose(fp);

		while(offset < size) {
			if ( strncmp(buf+offset, "XDG_RUNTIME_DIR=", 16) == 0 ) {
				sprintf(runtime_dir, "%s", buf+offset+16);
				free(buf);
				return 0;
			}
			offset += strlen(buf+offset) + 1;
		}
	}
	if (buf) free(buf);
	return 1;
}


#ifdef MAIN
int main (int argc, char **argv) {
	char *prog = "jackd";
	PROC		*p;
	readproc();

	for (p = plist; p; p = p->next) {
		if (p->argv0 && strstr(p->argv0, prog) != 0) {
			printf("pid:%i '%s' u:%i g:%i\n",p->pid, p->argv0, p->uid, p->gid);
		}
	}

	return(0);
}
#endif
