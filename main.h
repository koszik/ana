/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2006.
 *
 */
#ifndef __MAIN_H
#define __MAIN_H

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <setjmp.h>

#ifdef __MODULE2__
#	define __MODULE__
extern void *module_id;
#endif


#ifdef __MODULE__
#	define TYPE void *
#else
#	define TYPE struct module_type *
#endif


#if 0
#ifndef __FUNCTION__
#	define __FUNCTION__ "???"
#	define __inline__ inline
#endif
#endif

#define NUL(x) ((x)?(x):"(null)")

#define VER "0.6.0"

#ifdef __BSD__
#	define	DL_FLAG	DL_LAZY
#else
#	define	DL_FLAG	RTLD_NOW /* vagy LAZY */
#endif

#define	ANA_OK		1
#define ANA_ERR		0

#define MAX_HIST	128	/* lines to remember, readuser 		*/
#define MAX_ARGS	128	/* max number of arguments, readuser	*/

#define MAX_MODULES	64

#define S_READ		0
#define S_WRITE		1
#define S_MAX		2

#define MAXX(a,b)	((a)>(b)?(a):(b))
#define MINN(a,b)	((a)<(b)?(a):(b))

#define delrn(buf)							\
{									\
	register int x;							\
									\
	x = strlen(buf) - 1;						\
	if(x > -1 && buf[x] == '\n')					\
		buf[x--] = '\0';					\
	if(x > -1 && buf[x] == '\r')					\
		buf[x] = '\0';						\
}

#define DIFFT(a,b)							\
	((a.tv_sec - b.tv_sec) * 1000000 + (a.tv_usec - b.tv_usec))

#define DIFFTm(a,b)							\
	((a.tv_sec - b.tv_sec) * 1000 + (a.tv_usec - b.tv_usec) / 1000)

/*
#define ADDT(a,b)							\
	a.tv_sec += b / 1000000;					\
	if( a.tv_usec + (b % 1000000) > 999999 )			\
	{								\
		a.tv_sec++;						\
		a.tv_usec = a.tv_usec + (b % 1000000) - 1000000;	\
	}								\
	else								\
		a.tv_usec += (b % 1000000);

#define ONE_MP(a,b)							\
	(a.tv_sec < b.tv_sec)*/

#define BIGGER(a,b)							\
	(a.tv_sec > b.tv_sec || (a.tv_sec == b.tv_sec &&		\
				 a.tv_usec > b.tv_usec))


extern int quit;
extern time_t nowtime, uptime;

struct module_type
{
	int		 in_use;
	char		*name;
	void		*handle;		/* ifndef __STATIC__ ? */
	int		 used;
	void	       (*cleanup)(void);
	struct sym_exp_tbl_type *exports;
	struct sym_tbl_type *imports;
	size_t		 malloc_bytes;
	int		 malloc_chunks;
	struct macct_type *macct;
	int		 usecount;
	int		*usedby;
};

struct cmd_type
{
	char			*name;
	void		       (*handler)(int argc, char *argv[]);
	char			*help;
	struct cmd_type		*next;
	struct module_type	*mid;
};

typedef void (*teventh_t)(void *arg);
typedef void (*aeventh_t)(void);
typedef void (*sockh_t)(int sd, void *arg);

struct tevent_type
{
	struct tevent_type	*next;
	void			*arg;
	time_t			 nextwhen;
	time_t			 interval;
	teventh_t		 handler;
	struct module_type	*mid;
};

struct aevent_type
{
	aeventh_t	    handler;
	struct module_type *mid;
	struct aevent_type *next;
};

#define CRED_PUBLIC		1

struct ctype_type
{
	char			 *name;
	/* data-t nezi, hogy benne van-e cred */
	int			(*check)(void *data, void *cred);
	void			(*free)(void *data);
	char			*(*display)(void *data);
	int			(*set)(char *cred);
	/* public, stb */
	int			  flags;
};

struct cred_type
{
	void			 *data;
	struct ctype_type	 *type;
};

struct user_type
{
	char			*name;
	struct cred_type	*creds;
	int			 credn;
};

struct session_type;

#define UC_LINEBUF		1

struct userconn_type
{
	int			 (*printf)(struct session_type *s, char *str);
	void			 (*logout)(struct session_type *s);
	int			 flags;
};

struct session_type
{
	struct user_type	 *user;
	struct userconn_type	 *conn;
	void			 *data;
	int		 	  sd;
	char			 *from;
	char			 *line;
	time_t			  login;
	time_t			  idle;
	char			 *outbuf;
};

#if defined(__MODULE__)
#include "module.h"
#endif

#ifdef __MODULE__
#include "user.h"
#include "variable.h"
#include "log.h"
#include "mem.h"
#include "readuser.h"
#include "socket.h"
#endif

#ifndef HAVE_GETIPNODEBYNAME
struct hostent *getipnodebyname(const char *name, int af, int flags, int *error_num);
void freehostent(struct hostent *ip);
#endif

#endif
