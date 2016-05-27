/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2006.
 *
 */
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#define __LOG_C
#include <main.h>
#include <log.h>
#include <variable.h>
#include <readuser.h>
#include <user.h>

static FILE *logfd;

static int loglevel_file;
static int loglevel_console;

static char mon[12][4] =
{
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *
ana_getdate(void)
{
	static char ret[50];
	struct tm *s_tm;
	time_t secs;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	secs = tv.tv_sec;
	s_tm = localtime(&secs);

	snprintf(ret, sizeof(ret), "%s %2i %02i:%02i:%02i.%06lu",
				mon[s_tm->tm_mon], s_tm->tm_mday,
				s_tm->tm_hour, s_tm->tm_min,
				s_tm->tm_sec, tv.tv_usec);
	return ret;
}

static void
Ulogopen(int argc, char *argv[])
{
	UCHECK("ana", 1);

	if(argc != 2)
	{
		printf("usage: %s logfile\n", argv[0]);
		return;
	}

	if((logfd = fopen(argv[1], "a")) == NULL)
	{
		printf(
	"%s: Can't open logfile `%s'! Logging messages will go to stderr.\n",
		argv[0], argv[1]);
	}
}

void
log_init(void)
{
	add_bi_num(NULL, "loglevel_file", &loglevel_file);
	add_bi_num(NULL, "loglevel_console", &loglevel_console);
	addcmd(NULL, "logopen", Ulogopen, "Usage: logopen logfile\n  Use logfile for logging.\n");
	loglevel_file = 2;
	loglevel_console = 3;
}

void
logprintf(int level, const char *fmt, ...)
{
	va_list list;
	int pr;

	pr = 0;

	if(level >= loglevel_file)
	{
		if(logfd)
		fprintf(logfd, "%s ", ana_getdate());
		va_start(list, fmt);
		vfprintf(logfd?logfd:stderr, fmt, list);
		va_end(list);
		fflush(logfd?logfd:stderr);
		pr = 1;
	}

	if(level >= loglevel_console && (pr == 0 || logfd != NULL))
	{
		va_start(list, fmt);
		uprintall(fmt, list);
		va_end(list);
		fflush(stderr);
	}
}

static struct exc_type exc;

void
exc_raise(struct module_type *m, char *file, const char *func, int line, char *fmt, ...)
{
	va_list list;

	exc_print();
	va_start(list, fmt);
	vsnprintf(exc.msg, sizeof(exc.msg), fmt, list);
	va_end(list);
	exc.file = file;
	exc.func = func;
	exc.line = line;
	exc.mod = m;
}

void
exc_clear(void)
{
	if(!exc.file)
		return;

	exc.file = NULL;
	exc.func = NULL;
	exc.mod = NULL;
	exc.line = 0;
	exc.msg[0] = 0;
}

void
exc_print(void)
{
	if(!exc.file)
		return;

	logprintf(LOG_ERR, "unhandled exception: %s:%s:%s():%i: %s\n",
			exc.mod? exc.mod->name: "*main*", exc.file, exc.func,
			exc.line, exc.msg);
	exc_clear();
}

void
exc_printf(void)
{
	if(exc.file)
	uprintf("%s:%s:%s():%i: %s\n",
			exc.mod? exc.mod->name: "*main*", exc.file, exc.func,
			exc.line, exc.msg);
}

struct exc_type *
exc_get(void)
{
	if(exc.file)
		return &exc;
	return NULL;
}
