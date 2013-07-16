/*
 * Copyright 2008-2013 Various Authors
 * Copyright 2005 Timo Hirvonen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "fetch.h"
#include "file.h"
#include "xmalloc.h"
#include "debug.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/signalfd.h>

/* On a non zero return we return the hopefully meaningful error messages from 
 * stderr, in the other case we ignore it. this is on purpose because some tools
 * spam stderr with interactive crap but we want to save us a shell invocation 
 * for 2>/dev/null */
char * fetch(char *argv[])
{
	int err_pipe[2];
	int out_pipe[2];
	pid_t pid;

	if (pipe(err_pipe) == -1)
		return NULL;
	if (pipe(out_pipe) == -1)
		return NULL;

	pid = fork();
	if (pid == -1) {
		/******** error *******/
		return NULL;
	} else if (pid == 0) {
		/******** child *******/
		int err, i;
		sigset_t mask;
		int sfd;
		struct signalfd_siginfo fdsi;
		ssize_t s;

		/* create grandchild and exit child to avoid zombie processes */
		switch (pid = fork()) {
			case 0:
				/* grandchild */
				break;
			case -1:
				/* error */
				exit(EXIT_FAILURE);
			default:
				/* parent of grandchild */
				sigemptyset(&mask);
				sigaddset(&mask, SIGINT);
				sigaddset(&mask, SIGQUIT);
				sigaddset(&mask, SIGUSR1);
				sigaddset(&mask, SIGCHLD);

				/* Block signals so that they aren't handled according to the default */
				if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1){
					d_print("error setting sigprocmask");
					exit(EXIT_FAILURE);
				}

				sfd = signalfd(-1, &mask, 0);
				if (sfd == -1){
					d_print("error setting up signalfd");
					exit(EXIT_FAILURE);
				}

				for (;;) {
					s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
					if (s != sizeof(struct signalfd_siginfo)){
						d_print("error reading signalfd");
						exit(EXIT_FAILURE);
					}
					if (fdsi.ssi_signo == SIGINT || fdsi.ssi_signo == SIGQUIT ||
						fdsi.ssi_signo == SIGUSR1) {
						d_print("we are asked to kill our poor child\n");
						kill(pid, SIGTERM);
						usleep(200000);
						kill(pid, SIGKILL);
						exit(EXIT_SUCCESS);
					} else if (fdsi.ssi_signo == SIGCHLD) {
						exit(EXIT_SUCCESS);
					} else {
						d_print("Read unexpected signal%d\n", fdsi.ssi_signo);
					}
				}
		}

		/* we are now in the grandchild, communicate our pid to the grandparent */
		pid = getpid();
		write(out_pipe[1], &pid, sizeof pid);

		/* reading end of our pipes */
		close(err_pipe[0]);
		close(out_pipe[0]);
		/* close the original fd on exec ... */
		fcntl(out_pipe[1], F_SETFD, FD_CLOEXEC);
		fcntl(err_pipe[1], F_SETFD, FD_CLOEXEC);
		/* because we also attached them to stdout and stderr ... */
		dup2(out_pipe[1], 1);
		dup2(err_pipe[1], 2);		/* not interactive, close stdin */

		close(0);

		/* close unused fds */
		for (i = 3; i < 30; i++)
			close(i);

		/* for the new process */
		execvp(argv[0], argv);

		/* error exec'ing - we shouldn't be here */
		err = errno;
		write_all(err_pipe[1], &err, sizeof(int));
		exit(1);

	} else {

		/******** (grand)parent ********/
		int rc, errno_save, status = 0;
		char *err_buf=NULL, *out_buf=NULL;

		close(err_pipe[1]);
		close(out_pipe[1]);

		/* grab the grandchild's pid */
		while ((rc = read(out_pipe[0], &pid, sizeof pid)) == -1 &&
			(errno == EAGAIN || errno == EINTR))
				;

		if (rc < (sizeof pid)){
			close(out_pipe[0]);
			close(err_pipe[0]);
			d_print("Couldn't read from pipe, something's amiss.\n");
			return NULL;
		}
		/* wait for it to exit */
		while (waitpid(pid, &status, 0) == -1 && errno == EINTR);

		if (status != 0){
			/* grandkid had an error */
			close(out_pipe[0]);
			rc = really_read_all(err_pipe[0], &err_buf, 64);
			errno_save = errno;
			close(err_pipe[0]);
			if (rc == -1) {
				free(err_buf);
				errno = errno_save;
				d_print("Reading the error output for a fetch cmd gave an error, %s",
					       "shouldn't happen\n");
				return NULL;
			}
			d_print("Fetch command produced the following error %d %s\n", 
					status,err_buf);
			return err_buf;
		} else {
			/* execution went dandy */
			close(err_pipe[0]);
			rc = really_read_all(out_pipe[0], &out_buf, 1024);
			errno_save = errno;
			close(out_pipe[0]);
			if (rc == -1) {
				free(out_buf);
				errno = errno_save;
				return xstrdup("Command produced no output\n");
			}
			return out_buf;
		}
	}
}
