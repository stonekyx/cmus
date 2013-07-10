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

/* Quick and dirty implementation, the error output should be separate from the status 
 * Plus more error output to differentiate the null cases, yada, yada*/
char * fetch(char *argv[])
{
	pid_t pid;
	int err_pipe[2];
	int out_pipe[2];

	if (pipe(err_pipe) == -1)
		return NULL;
	if (pipe(out_pipe) == -1)
		return NULL;


	pid = fork();
	if (pid == -1) {
		/* error */
		return NULL;
	} else if (pid == 0) {
		/* child */
		int err, i;

		/* why is this necessary, let's just copy and forget about it */
		/* create grandchild and exit child to avoid zombie processes */
		switch (fork()) {
		case 0:
			/* grandchild */
			break;
		case -1:
			/* error */
			_exit(127);
		default:
			/* parent of grandchild */
			_exit(0);
		}

		close(err_pipe[0]);
		close(out_pipe[0]);
		fcntl(out_pipe[1], F_SETFD, FD_CLOEXEC);
		fcntl(err_pipe[1], F_SETFD, FD_CLOEXEC);
		dup2(out_pipe[1], 1);
		dup2(err_pipe[1], 2);

		/* not interactive, close stdin */
		close(0);

		/* close unused fds */
		for (i = 3; i < 30; i++)
			close(i);

		execvp(argv[0], argv);

		/* error */
		err = errno;
		write_all(err_pipe[1], &err, sizeof(int));
		exit(1);
	} else {
		/* parent */
		int rc, errno_save;
		char *err_buf=NULL, *out_buf=NULL;

		close(err_pipe[1]);
		close(out_pipe[1]);

		rc = really_read_all(err_pipe[0], &err_buf, 64);
		errno_save = errno;
		close(err_pipe[0]);
		if (rc == -1) {
			close(out_pipe[0]);
			free(err_buf);
			errno = errno_save;
			d_print("reading the error output gave an error, shouldn't happen\n");
			return NULL;
		}
		if (strlen(err_buf)>0){
			close(out_pipe[0]);
			d_print("Command produced the following error %s\n", err_buf);
			return err_buf;
		} else 
			free(err_buf);

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
