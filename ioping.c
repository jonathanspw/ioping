/*
 *  ioping  -- simple I/0 latency measuring tool
 *
 *  Copyright (C) 2011 Konstantin Khlebnikov <koct9i@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <err.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

long long now(void)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL))
		err(1, "gettimeofday failed");

	return tv.tv_sec * 1000000ll + tv.tv_usec;
}

struct suffix {
	const char	*txt;
	long long	mul;
};

long long parse_suffix(const char *str, struct suffix *sfx)
{
	char *end;
	double val;

	val = strtod(str, &end);
	for ( ; sfx->txt ; sfx++ ) {
		if (!strcasecmp(end, sfx->txt))
			return val * sfx->mul;
	}
	errx(1, "invalid suffix: \"%s\"", end);
	return 0;
}

long long parse_int(const char *str)
{
	static struct suffix sfx[] = {
		{ "",		1ll },
		{ "da",		10ll },
		{ "k",		1000ll },
		{ "M",		1000000ll },
		{ "G",		1000000000ll },
		{ "T",		1000000000000ll },
		{ "P",		1000000000000000ll },
		{ "E",		1000000000000000000ll },
		{ NULL,		0ll },
	};

	return parse_suffix(str, sfx);
}

long long parse_size(const char *str)
{
	static struct suffix sfx[] = {
		{ "",		1 },
		{ "b",		1 },
		{ "s",		1ll<<9 },
		{ "k",		1ll<<10 },
		{ "kb",		1ll<<10 },
		{ "p",		1ll<<12 },
		{ "m",		1ll<<20 },
		{ "mb",		1ll<<20 },
		{ "g",		1ll<<30 },
		{ "gb",		1ll<<30 },
		{ "t",		1ll<<40 },
		{ "tb",		1ll<<40 },
		{ "p",		1ll<<50 },
		{ "pb",		1ll<<50 },
		{ "e",		1ll<<60 },
		{ "eb",		1ll<<60 },
/*
		{ "z",		1ll<<70 },
		{ "zb",		1ll<<70 },
		{ "y",		1ll<<80 },
		{ "yb",		1ll<<80 },
*/
		{ NULL,		0ll },
	};

	return parse_suffix(str, sfx);
}

long long parse_time(const char *str)
{
	static struct suffix sfx[] = {
		{ "us",		1ll },
		{ "usec",	1ll },
		{ "ms",		1000ll },
		{ "msec",	1000ll },
		{ "",		1000000ll },
		{ "s",		1000000ll },
		{ "sec",	1000000ll },
		{ "m",		1000000ll * 60 },
		{ "min",	1000000ll * 60 },
		{ "h",		1000000ll * 60 * 60 },
		{ "hour",	1000000ll * 60 * 60 },
		{ "day",	1000000ll * 60 * 60 * 24 },
		{ "week",	1000000ll * 60 * 60 * 24 * 7 },
		{ "month",	1000000ll * 60 * 60 * 24 * 7 * 30 },
		{ "year",	1000000ll * 60 * 60 * 24 * 7 * 365 },
		{ "century",	1000000ll * 60 * 60 * 24 * 7 * 365 * 100 },
		{ "millenium",	1000000ll * 60 * 60 * 24 * 7 * 365 * 1000 },
		{ NULL,		0ll },
	};

	return parse_suffix(str, sfx);
}

void usage(void)
{
	fprintf(stderr,
			" Usage: ioping [-DCRhq] [-c count] [-w deadline] [-p pediod]\n"
			"               [-i interval] [-s size] [-o offset] device|file|directory\n"
			"\n"
			"      -c <count>      stop after <count> requests\n"
			"      -w <deadline>   stop after <deadline>\n"
			"      -p <period>     print raw statistics every <period> requests\n"
			"      -i <interval>   interval between requests\n"
			"      -s <size>       request size\n"
			"      -o <offset>     offset in file\n"
			"      -D              use direct-io\n"
			"      -C              use cached-io\n"
			"      -h              display this mesage and exit\n"
			"      -q              suppress human-readable output\n"
			"\n"
	       );
	exit(0);
}

char *path = NULL;
char *fstype = "";
char *device = "";

int fd;
char *buf;

int quiet = 0;
int period = 0;
int direct = 0;
int cached = 0;

long long interval = 1000000;
long long deadline = 0;

ssize_t size = 512;
off_t offset = 0;

int request;
int count = 0;

int exiting = 0;

void parse_options(int argc, char **argv)
{
	int opt;

	if (argc < 2)
		usage();

	while ((opt = getopt(argc, argv, "-hDCqi:w:s:c:o:p:")) != -1) {
		switch (opt) {
			case 'h':
				usage();
			case 'D':
				direct = 1;
				break;
			case 'C':
				cached = 1;
				break;
			case 'i':
				interval = parse_time(optarg);
				break;
			case 'w':
				deadline = parse_time(optarg);
				break;
			case 's':
				size = parse_size(optarg);
				break;
			case 'o':
				offset = parse_size(optarg);
				break;
			case 'p':
				period = parse_int(optarg);
				break;
			case 'q':
				quiet = 1;
				break;
			case 'c':
				count = parse_int(optarg);
				break;
			case 1:
				if (path) {
					errx(1, "more than one destination: "
							"\"%s\" and \"%s\"",
							path, optarg);
				}
				path = optarg;
				break;
			case '?':
				errx(1, "unknown option: -%c", optopt);
		}
	}

	if (!path)
		errx(1, "no destination specified");
}

void parse_device(dev_t dev)
{
	unsigned major, minor;
	char *buf = NULL, *ptr, *sep;
	size_t len;
	FILE *mountinfo;

	/* since v2.6.26 */
	mountinfo = fopen("/proc/self/mountinfo", "r");
	if (!mountinfo)
		return;

	while (getline(&buf, &len, mountinfo) > 0) {
		sscanf(buf, "%*d %*d %u:%u", &major, &minor);
		if (makedev(major, minor) != dev)
			continue;
		ptr = strstr(buf, " - ");
		if (!ptr)
			break;
		ptr += 3;
		sep = strchr(ptr, ' ');
		if (!sep)
			break;
		fstype = strndup(ptr, sep - ptr);
		ptr = sep + 1;
		sep = strchr(ptr, ' ');
		if (!sep)
			break;
		device = strndup(ptr, sep - ptr);
		break;
	}
	free(buf);
	fclose(mountinfo);
}

void sig_exit(int signo)
{
	(void)signo;
	exiting = 1;
}

void set_signal(int signo, void (*handler)(int))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	sigaction(signo, &sa, NULL);
}

int main (int argc, char **argv)
{
	ssize_t ret_size;
	struct stat stat;
	int ret, flags;

	long long this_time, time_total;
	long long part_min, part_max, time_min, time_max;
	double time_sum, time_sum2, time_mdev, time_avg;
	double part_sum, part_sum2, part_mdev, part_avg;

	parse_options(argc, argv);

	if (lstat(path, &stat))
		err(1, "stat \"%s\" failed", path);

	if (S_ISREG(stat.st_mode) || S_ISDIR(stat.st_mode)) {
		if (!quiet)
			parse_device(stat.st_dev);
	} else if (S_ISBLK(stat.st_mode)) {
		fstype = "block";
		device = "device";
	} else {
		errx(1, "unsupported destination: \"%s\"", path);
	}

	buf = memalign(sysconf(_SC_PAGE_SIZE), size);
	if (!buf)
		errx(1, "buffer allocation failed");
	memset(buf, '*', size);

	flags = O_RDONLY;
	if (direct)
		flags |= O_DIRECT;

	if (S_ISDIR(stat.st_mode)) {
		char *tmpl = "/ioping.XXXXXX";
		char *temp = malloc(strlen(path) + strlen(tmpl) + 1);

		if (!temp)
			errx(1, "no memory");
		sprintf(temp, "%s%s", path, tmpl);
		fd = mkostemp(temp, flags);
		if (fd < 0)
			err(1, "failed to create temporary file at \"%s\"", path);
		if (unlink(temp))
			err(1, "unlink \"%s\" failed", temp);
		if (pwrite(fd, buf, size, offset) != size)
			err(1, "write failed");
		if (fsync(fd))
			err(1, "fsync failed");
	} else {
		fd = open(path, flags);
		if (fd < 0)
			err(1, "failed to open \"%s\"", path);
	}

	if (deadline)
		deadline += now();

	set_signal(SIGINT, sig_exit);

	request = 0;
	part_min = time_min = LLONG_MAX;
	part_max = time_max = LLONG_MIN;
	part_sum = time_sum = 0;
	part_sum2 = time_sum2 = 0;
	time_total = now();

	while (!exiting) {
		if (count && request >= count)
			break;

		if (deadline && now() >= deadline)
			break;

		request++;

		if (!cached) {
			ret = posix_fadvise(fd, offset, size,
					POSIX_FADV_DONTNEED);
			if (ret)
				err(1, "fadvise failed");
		}

		this_time = now();
		ret_size = pread(fd, buf, size, offset);
		if (ret_size < 0 && errno != EINTR)
			err(1, "read failed");
		this_time = now() - this_time;

		part_sum += this_time;
		part_sum2 += this_time * this_time;
		if (this_time < part_min)
			part_min = this_time;
		if (this_time > part_max)
			part_max = this_time;

		if (!quiet)
			printf("%lld bytes from %s (%s %s): request=%d time=%.1f ms\n",
					(long long)ret_size, path, fstype, device,
					request, this_time / 1000.);

		if (period && request % period == 0) {
			part_avg = part_sum / period;
			part_mdev = sqrt(part_sum2 / period - part_avg * part_avg);

			printf("%lld %.0f %lld %.0f\n",
					part_min, part_avg,
					part_max, part_mdev);

			time_sum += part_sum;
			time_sum2 += part_sum2;
			if (part_min < time_min)
				time_min = part_min;
			if (part_max > time_max)
				time_max = part_max;
			part_min = LLONG_MAX;
			part_max = LLONG_MIN;
			part_sum = part_sum2 = 0;
		}

		if (!exiting)
			usleep(interval);
	}

	time_total = now() - time_total;

	time_sum += part_sum;
	time_sum2 += part_sum2;
	if (part_min < time_min)
		time_min = part_min;
	if (part_max > time_max)
		time_max = part_max;

	time_avg = time_sum / request;
	time_mdev = sqrt(time_sum2 / request - time_avg * time_avg);

	if (!quiet) {
		printf("\n--- %s ioping statistics ---\n", path);
		printf("%d requests completed in %.1f ms\n",
				request, time_total/1000.);
		printf(" min/avg/max/mdev = %.1f/%.1f/%.1f/%.1f ms\n",
				time_min/1000., time_avg/1000.,
				time_max/1000., time_mdev/1000.);
	}

	return 0;
}