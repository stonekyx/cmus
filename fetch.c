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

/* On a non zero return we return the hopefully meaningful error messages from 
 * stderr, in the other case we ignore it. this is on purpose because some tools
 * spam stderr with interactive crap but we want to save us a shell invocation 
 * for 2>/dev/null */
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

		/* error execing */
		err = errno;
		write_all(err_pipe[1], &err, sizeof(int));
		exit(1);
	} else {
		/* parent */
		int rc, errno_save, status;
		char *err_buf=NULL, *out_buf=NULL;

		close(err_pipe[1]);
		close(out_pipe[1]);

		if (waitpid(pid, &status, 0) == -1){
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
			d_print("Fetch command produced the following error %s\n", err_buf);
			return err_buf;
		} else {
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
