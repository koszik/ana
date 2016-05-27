/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2006.
 *
 */
#ifndef __LOG_H
#define __LOG_H

#define LOG_DEBUG		0
#define LOG_INFO		1
#define LOG_NOTICE		2
#define LOG_WARN		3
#define LOG_ERR			4
#define LOG_CRIT		5
#define LOG_ALERT		6
#define LOG_EMERG		7

char *ana_getdate(void);
void log_init(void);
void logprintf(int level, const char *fmt, ...);
void exc_raise(struct module_type *m, char *file, const char *func, int line, char *fmt, ...);
void exc_clear(void);
void exc_print(void);
void exc_printf(void);
struct exc_type *exc_get(void);

struct exc_type
{
	struct	module_type	*mod;
	char			*file;
	const char		*func;
	int			 line;
	char			 msg[1024];
};

#ifdef __MODULE__
#define	E_P	module_id, __FILE__, __FUNCTION__, __LINE__
#else
#define	E_P	NULL, __FILE__, __FUNCTION__, __LINE__
#endif

#endif
