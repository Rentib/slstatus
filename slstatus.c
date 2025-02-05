/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "arg.h"
#include "slstatus.h"
#include "util.h"

struct arg {
	const char *(*func)(const char *);
	const char *fmt;
	const char *args;
	unsigned interval;
	int signal;
};

char buf[1024];

#include "config.h"
#define MAXLEN (CMDLEN * LEN(args))

static char cmds[LEN(args)][CMDLEN] = {0};

static void
difftimespec(struct timespec *res, struct timespec *a, struct timespec *b)
{
	res->tv_sec = a->tv_sec - b->tv_sec - (a->tv_nsec < b->tv_nsec);
	res->tv_nsec = a->tv_nsec - b->tv_nsec +
	               (a->tv_nsec < b->tv_nsec) * 1E9;
}

static void
usage(void)
{
	die("usage: %s [-v] [-1]", argv0);
}

static void
print_status(int it, int sig)
{
	size_t i;
	int update = 0;
	static char status[MAXLEN];
	const char *res;

	for (i = 0; i < LEN(args); i++) {
		if (!(it == 0
		||   (args[i].interval != 0 && it % args[i].interval == 0)
		||   (sig != -1 && args[i].signal == sig)))
			continue;

		if (!(res = args[i].func(args[i].args)))
			res = unknown_str;
		update = 1;
		if (esnprintf(cmds[i], sizeof(cmds[i]), args[i].fmt, res) < 0)
			break;
	}

	if (!update)
		return;

	status[0] = '\0';
	for (i = 0; i < LEN(args); i++) {
		if (strncmp(cmds[i], unknown_str, strlen(unknown_str)))
			strcat(status, cmds[i]);
	}

	puts(status);
	fflush(stdout);
	if (ferror(stdout))
		die("puts:");
}

static int
gcd(int a, int b)
{
	return b ? gcd(b, a % b) : a;
}

int
main(int argc, char *argv[])
{
	struct timespec start, current, diff, intspec, wait;
	size_t i, time = 0, interval = 60 * 1000, lcm = interval;
	int done = 0, signo;
	sigset_t mask;

	ARGBEGIN {
	case 'v':
		die("slstatus-"VERSION);
		return 0; /* no need for return, but it silences a warning */
	case '1':
		done = 1;
		/* FALLTHROUGH */
	default:
		usage();
	} ARGEND

	if (argc)
		usage();

	if (sigemptyset(&mask) < 0)
		die("sigemptyset:");
	if (sigaddset(&mask, SIGINT) < 0 || sigaddset(&mask, SIGTERM) < 0)
		die("sigaddset:");
	for (i = 0; i < LEN(args); i++) {
		interval = gcd(interval, args[i].interval);
		lcm = lcm / gcd(lcm, interval) * interval;
		if (args[i].signal != -1 &&
		    sigaddset(&mask, args[i].signal + SIGRTMIN) < 0)
			die("sigaddset:");
	}

	if (sigprocmask(0, &mask, NULL) < 0)
		die("sigprocmask:");

	do {
		if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
			die("clock_gettime:");

		print_status(time, -1);
		time = (time + interval) % lcm;
		if (time == 0)
			time = lcm;

		if (done)
			continue;

	sleep:
		if (clock_gettime(CLOCK_MONOTONIC, &current) < 0)
			die("clock_gettime:");

		difftimespec(&diff, &current, &start);
		intspec.tv_sec = interval / 1000;
		intspec.tv_nsec = (interval % 1000) * 1E6;
		difftimespec(&wait, &intspec, &diff);

		if ((signo = sigtimedwait(&mask, NULL, &wait)) < 0) {
			if (errno == EAGAIN)
				continue;
			else if (errno == EINTR)
				goto sleep;
			die("sigtimedwait:");
		} else if (signo == SIGINT || signo == SIGTERM) {
			done = 1;
		} else {
			print_status(-1, signo - SIGRTMIN);
			goto sleep;
		}
	} while (!done);

	return 0;
}
