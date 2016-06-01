/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2006, 2016.
 *
 */
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
/*#define __USE_BSD*/
#include <unistd.h>
#include <errno.h>
#define __USE_POSIX
#define __USE_POSIX199309
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>  
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define __ANA_C
#include <main.h>
#include <module.h>
#include <readuser.h>
#include <conio.h>
#include <log.h>
#include <variable.h>
#include <mem.h>
#include <user.h>

/* #define logprintf printf */

char *strdup(const char *s);

#define CONFIG_FILE	"ana.conf"

static fd_set grset, gwset;
static sockh_t socks[S_MAX][1024];
static void *sock_arg[S_MAX][1024];
static int wrs, rrs;
static int brsd = -1, bwsd = -1;
static struct aevent_type *falw;
static struct tevent_type *tevents;
static struct module_type mods[MAX_MODULES];

static void deleventmod(struct module_type *m);
static void delaevent_mod(struct module_type *m);
static void deltevent_mod(struct module_type *m);

time_t nowtime;	/* cached time() */
time_t uptime;

#ifdef __STATIC__
struct statmod_type
{
	char		 *name;
	int		(*init)(int argc2, char *argv2[]);
	void		(*cleanup)(void);
};

int cloner_init();
void cloner_cleanup();
int handle_init();
void handle_cleanup();
int irc_mod_init();
void irc_mod_cleanup();
int crypt_init();
void crypt_cleanup();


struct statmod_type statmods[] =
{
	{ "cloner",	cloner_init,	cloner_cleanup	},
	{ "handle",	handle_init,	handle_cleanup	},
	{ "irc_mod",	irc_mod_init,	irc_mod_cleanup	},
	{ "crypt",	crypt_init,	crypt_cleanup	},
	{ NULL }
};

#endif

/* compat */
#ifndef HAVE_GETIPNODEBYNAME
struct hostent *gethostbyname2(const char *name, int af);

struct hostent *
getipnodebyname(const char *name, int af, int flags, int *error_num)
{
	struct hostent *he;

	he = gethostbyname2(name, af);
	*error_num = h_errno;
	return he;
}

void
freehostent(struct hostent *ip)
{
}
#endif

static int
sym_exists(char *n)
{
	int i, j;

	for(i = 0; i < MAX_MODULES; i++)
		if(mods[i].in_use && mods[i].exports)
			for(j = 0; mods[i].exports[j].name; j++)
				if(!strcmp(n, mods[i].exports[j].name))
					return 1;
	return 0;
}

static struct sym_exp_tbl_type *
get_sym(char *n, int *mod)
{
	int j;

	for(*mod = 0; *mod < MAX_MODULES; (*mod)++)
		if(mods[*mod].in_use && mods[*mod].exports)
			for(j = 0; mods[*mod].exports[j].name; j++)
				if(!strcmp(n, mods[*mod].exports[j].name))
					return &mods[*mod].exports[j];
	return NULL;
}

static void
addref(int m, int mod)
{
	int i;

	for(i = 0; i < mods[m].usecount; i++)
		if(mods[m].usedby[i] == mod)
			return;

	mods[m].usedby = realloc(mods[m].usedby, sizeof(*mods[m].usedby) * (mods[m].usecount + 1));
	mods[m].usedby[mods[m].usecount++] = mod;
}

static void
delref(int m, int mod)
{
	int i;

	for(i = 0; i < mods[m].usecount; i++)
		if(mods[m].usedby[i] == mod)
		{
			memmove(&mods[m].usedby[i], &mods[m].usedby[--mods[m].usecount], sizeof(*mods[m].usedby));
			return;
		}
}

static void
resolve_syms(int mod, struct sym_tbl_type *t)
{
	int i, m;
	struct sym_exp_tbl_type *s;

	if(!t)
		return;
	for(i = 0; t[i].name; i++)
	{
		s = get_sym(t[i].name, &m);
		addref(m, mod);
		s->used++;
		*(void **)t[i].addr = s->addr;
	}
}

static void
release_syms(int mod, struct sym_tbl_type *t)
{
	int i, m;
	struct sym_exp_tbl_type *s;

	if(!t)
		return;
	for(i = 0; t[i].name; i++)
	{
		s = get_sym(t[i].name, &m);
		delref(m, mod);
		s->used--;
		*(void **)t[i].addr = NULL;
	}
}

int
loadmodule(const char *modpath, int argc, char *argv[])
{
	int j, cur;
#ifndef __STATIC__
	typedef int (*init_t)(int argc2, char *argv2[]);
	typedef void (*cleanup_t)(void);

	void *handle, **id;
	char *error;
	init_t init;
	struct sym_exp_tbl_type *exports;
	struct sym_tbl_type *imports;
#else
	int i;
#endif


	for(cur = 0; cur < MAX_MODULES; cur++)
		if(!mods[cur].in_use)
			break;

	if(cur == MAX_MODULES)
	{
		exc_raise(E_P, "there're already %i modules loaded, can't load more", MAX_MODULES);
		return ANA_ERR;
	}

	memset(&mods[cur], 0, sizeof(mods[cur]));

#ifdef __STATIC__
	for(i = 0; statmods[i].name; i++)
		if(!strcmp(statmods[i].name, modpath)) {
			mods[cur].name = statmods[i].name;
			mods[cur].cleanup = statmods[i].cleanup;
			break;
		}
#else
	if(*modpath == '/')
		handle = dlopen(modpath, DL_FLAG);
	else
	{
		char *fullfn;

		fullfn = malloc(strlen(modpath) + 3);
		sprintf(fullfn, "./%s", modpath);
		handle = dlopen(fullfn, DL_FLAG);
		free(fullfn);
	}

#define DLERR(x) \
	if((error = dlerror()) != NULL) \
	{ \
		exc_raise(E_P, "dlsym("x"): %s", error); \
		dlclose(handle); \
		return ANA_ERR; \
	} \


	if((error = dlerror()) != NULL)
	{
		exc_raise(E_P, "dlopen: %s", error);
		return ANA_ERR;
	}

	exports = dlsym(handle, "export_table");
	imports = dlsym(handle, "import_table");
	if(imports)
	for(j = 0; imports[j].name; j++)
	{
		if(!sym_exists(imports[j].name))
		{
			exc_raise(E_P, "%s: unresolved symbol: %s", modpath, imports[j].name);
			dlclose(handle);
			return ANA_ERR;
		}
	}

	while(dlerror());

	init = (init_t)dlsym(handle, "init");
	DLERR("init");
	mods[cur].name = dlsym(handle, "module_name");
	DLERR("module_name");
	mods[cur].cleanup = (cleanup_t)dlsym(handle, "cleanup");
	DLERR("cleanup");
	id = dlsym(handle, "module_id");
	DLERR("module_id");
#endif

	for(j = 0; j < MAX_MODULES; j++)
		/* mods[cur].in_use is 0 */
		if(mods[j].in_use && !strcmp(mods[j].name, mods[cur].name))
		{
			exc_raise(E_P, "module %s: module with this name (%s) already exists.", modpath, mods[j].name);
			return ANA_ERR;
		}

	mods[cur].used = 0;
	mods[cur].in_use = 1;


#ifndef __STATIC__
	mods[cur].handle = handle;
	mods[cur].exports = exports;
	mods[cur].imports = imports;
	*id = &mods[cur];
	resolve_syms(cur, imports);
	if(init(argc, argv) != 0)
#else
	if(statmods[i].init(argc, argv) != 0)
#endif
	{
		exc_raise(E_P, "module %s: init returned non-zero", modpath);
		delmod(mods[cur].name);
#ifndef __STATIC__
		release_syms(cur, imports);
#endif
		return ANA_ERR;
	}

	return ANA_OK;
}

int
delmod(const char *name)
{
	int i, j, rv;
#ifndef __STATIC__
	char *error;
#endif

	for(i = 0; i < MAX_MODULES; i++)
		if(mods[i].in_use && !strcmp(mods[i].name, name))
			break;

	if(i == MAX_MODULES)
	{
		exc_raise(E_P, "module `%s' not found", name);
		return ANA_ERR;
	}

#ifndef __STATIC__
	if(mods[i].exports)
	for(j = 0; mods[i].exports[j].name; j++)
		if(mods[i].exports[j].used)
		{
			exc_raise(E_P, "can't unload %s, symbol %s is still in use", name, mods[i].exports[j].name);
			return ANA_ERR;
		}
#endif

	mods[i].cleanup();
#ifndef __STATIC__
	release_syms(i, mods[i].imports);
	dlclose(mods[i].handle);
	if((error = dlerror()) != NULL)
	{
		rv = ANA_ERR;
		exc_raise(E_P, "module %s: dlclose: %s", name, error);
	}
	else
		rv = ANA_OK;
#else
	rv = ANA_OK;
#endif

	del_var_mod(&mods[i]);
	deleventmod(&mods[i]);
	deltevent_mod(&mods[i]);
	delaevent_mod(&mods[i]);
	delcmd_mod(&mods[i]);
	mods[i].in_use = 0;
	if(mods[i].malloc_bytes)
	{
		logprintf(LOG_ERR, "delmod: WARNING: unfreed bytes:\n");
		for(j = 0; j < mods[i].malloc_chunks; j++)
		{
			logprintf(LOG_ERR, "delmod:  %p: %i bytes, alloced @ %s\n", mods[i].macct[j].ptr, mods[i].macct[j].size, mods[i].macct[j].loc);
			free(mods[i].macct[j].ptr);
			free(mods[i].macct[j].loc);
		}
	}
	return rv;
}

static void
Ulsmem(int argc, char *argv[])
{
	int i, j;

	UCHECK("ana", 1);

	if(argc < 2)
	{
		uprintf("Usage: %s <module_name>\n", argv[0]);
		return;
	}

	for(i = 0; i < MAX_MODULES; i++)
		if(mods[i].in_use && !strcmp(mods[i].name, argv[1]))
			break;

	if(i == MAX_MODULES)
	{
		uprintf("%s: module `%s' not found\n", argv[0], argv[1]);
		return;
	}

	if(mods[i].malloc_bytes)
	{
		uprintf("Currently malloc()ed by module %s:\n", argv[1]);
		for(j = 0; j < mods[i].malloc_chunks; j++)
			uprintf("  %p:%4i bytes, @ %s\n", mods[i].macct[j].ptr, mods[i].macct[j].size, mods[i].macct[j].loc);
	}
}

int
addtevent(struct module_type *m, int interval, teventh_t handler_, void *arg)
{
	struct tevent_type *New;

	if(interval <= 0)
	{
		exc_raise(E_P, "invalid interval: %i", interval);
		return ANA_ERR;
	}

	New = malloc(sizeof(*New));
	memset(New, 0, sizeof(*New));
	New->interval = interval;
	New->handler = handler_;
	New->arg = arg;
	New->mid = m;

	New->next = tevents;
	tevents = New;
	return ANA_OK;
}

int
deltevent(struct module_type *m, int interval, teventh_t handler_, void *arg)
{
	struct tevent_type *cur, *prev;

	if(interval < 1)
	{
		exc_raise(E_P, "invalid interval: %i", interval);
		return ANA_ERR;
	}

	for(cur = tevents, prev = NULL; cur; prev = cur, cur = cur->next)
	{
		if(cur->handler == handler_ && cur->interval == interval
		   &&  cur->arg == arg)
		{
			if(prev)
				prev->next = cur->next;
			else
				tevents = cur->next;
			free(cur);
			return ANA_OK;
		}
	}
	exc_raise(E_P, "attempt to remove nonexistent handler!");
	return ANA_ERR;
}

static void
deltevent_mod(struct module_type *m)
{
	struct tevent_type *cur, *prev;

	for(cur = tevents, prev = NULL; cur; prev = cur, cur = cur->next)
	{
		if(cur->mid == m)
		{
			if(VN("log-autodel"))
				logprintf(LOG_NOTICE, "deltevent_mod: %i/%p\n",
						cur->interval, cur->handler);
			if(prev)
				prev->next = cur->next;
			else
				tevents = cur->next;
			free(cur);
			if(prev)
				cur = prev;
			else
				cur = tevents;
		}
	}
}

void
addsock(struct module_type *m, int type, int sd, sockh_t hl, void *sa)
{
	if(sd < 0 || sd > 1023)
	{
		exc_raise(E_P, "addsock: sd = %i", sd);
		return;
	}

	if(VN("ANA_DEBUG_SOCKET") == 1)
		logprintf(LOG_DEBUG, "addsock: %i, %i, %lx, %lx\n", type, sd, hl, sa);

	if(socks[type][sd])
	{
		exc_raise(E_P, "attempt to re-add socket! (sd=%i, type=%i)", sd, type);
		if(VN("ANA_DEBUG_SOCKET") == 1)
			abort();
		return;
	}

/*	logprintf(LOG_DEBUG, "addsock: %i, %i, %p, %p\n", type, sd, (void*)hl, sa);*/
	socks[type][sd] = hl;
	sock_arg[type][sd] = sa;

	if(type == S_READ)
	{
		if(brsd < sd)
			brsd = sd;
		rrs++;
		FD_SET(sd, &grset);
	}
	else
	{
		if(bwsd < sd)
			bwsd = sd;
		wrs++;
		FD_SET(sd, &gwset);
	}
}

void
delsock(struct module_type *m, int type, int sd)
{
	int i;

	if(sd < 0 || sd > 1023)
	{
		exc_raise(E_P, "delsock: sd = %i", sd);
		return;
	}

	if(VN("ANA_DEBUG_SOCKET") == 1)
		logprintf(LOG_DEBUG, "delsock: %i, %i\n", type, sd);

	if(!socks[type][sd])
	{
		exc_raise(E_P, "attempt to delete nonexistent socket!");
		if(VN("ANA_DEBUG_SOCKET") == 1)
			abort();
		return;
	}

/*	logprintf(LOG_DEBUG, "delsock: %i, %i\n", type, sd);*/

	socks[type][sd] = NULL;
	if(type == S_READ)
	{
		if(sd == brsd)
		{
			for(i = brsd - 1; i > -1; i--)
				if(socks[S_READ][i])
					break;
			brsd = i;
		}
		rrs--;
		FD_CLR(sd, &grset);
	}
	else
	{
		if(sd == bwsd)
		{
			for(i = bwsd - 1; i > -1; i--)
				if(socks[S_WRITE][i])
					break;
			bwsd = i;
		}
		wrs--;
		FD_CLR(sd, &gwset);
	}
}

/*static void
list_sym( void )
{
	struct sym_type *cur;

	for( cur = fsym.next; cur; cur = cur->next )
		printf( "ls: %s %p\n", cur->name, cur->addr );
}*/


void
addaevent(struct module_type *m, aeventh_t handler_)
{
	struct aevent_type *New;

	New = malloc(sizeof(*New));
	New->handler = handler_;
	New->mid = m;
	New->next = falw;
	falw = New;
}

void
delaevent(struct module_type *m, aeventh_t handler_)
{
	struct aevent_type *cur, *prev;

	for(cur = falw, prev = NULL; cur; prev = cur, cur = cur->next)
	{
		if(cur->handler == handler_)
		{
			if(prev)
				prev->next = cur->next;
			else
				falw = cur->next;
			free(cur);
			return;
		}
	}
	exc_raise(E_P, "attempt to remove nonexistent handler!");
}

static void
delaevent_mod(struct module_type *m)
{
	struct aevent_type *cur, *prev;

	for(cur = falw, prev = NULL; cur; prev = cur, cur = cur->next)
	{
		if(cur->mid == m)
		{
			if(VN("log-autodel"))
				logprintf(LOG_NOTICE, "delaevent_mod: %p\n", cur->handler);
			if(prev)
				prev->next = cur->next;
			else
				falw = cur->next;
			free(cur);
			if(prev)
				cur = prev;
			else
				cur = falw;
		}
	}
}

char *
ana_xstrdup(const char *s, int size)
{
	char *ret;

	ret = malloc(size + 1);
	memcpy(ret, s, size);
	ret[size] = 0;
	return ret;
}

static void
Ulsmod(int argc, char *argv[])
{
	int i;

	uprintf("Module        Sym    Memory    Used by\n");

	for(i = 0; i < MAX_MODULES; i++)
		if(mods[i].in_use)
		{
			int sym, j;

			sym = 0;
			if(mods[i].exports)
				for(j = 0; mods[i].exports[j].name; j++)
					sym += mods[i].exports[j].used;
			uprintf("%-15s%2i %6i/%-5i%s", mods[i].name, sym,
					mods[i].malloc_bytes, mods[i].malloc_chunks,
					mods[i].usecount? " [":"");
			for(j = 0; j < mods[i].usecount; j++)
				uprintf("%s%s", j?" ":"", mods[mods[i].usedby[j]].name);
			uprintf("%s\n", j?"]":"");
		}
}

static void
Urmmod(int argc, char *argv[])
{
	UCHECK("ana", 1);

	if(argc != 2)
	{
		uprintf("Usage: %s <module_to_unload>\n", argv[0]);
		return;
	}

	if(delmod(argv[1]) != ANA_OK)
	{
		exc_printf();
		exc_clear();
	}
}

static void
Uinsmod(int argc, char *argv[])
{
	UCHECK("ana", 1);

	if(argc < 2)
	{
		uprintf("Usage: insmod <module_to_load>\n");
		return;
	}

	if(loadmodule(argv[1], argc - 1, argv + 1) != ANA_OK)
	{
		exc_printf();
		exc_clear();
	}
}

struct event_type
{
	struct event_type *next;
	char		  *event;
	int		 (*callback)(char *event, void *evdata, void *cbdata);
	void		  *data;
	int		   prio;
	struct module_type*mid;
};

static struct event_type *evhead;

int
emit(char *event, void *data)
{
	struct event_type *cur;
	int i;

	for(i = 0, cur = evhead; cur; cur = cur->next)
		if(!strcasecmp(cur->event, event))
			if(++i && cur->callback(event, data, cur->data))
				break;

	logprintf(LOG_INFO, "emit: signal `%s', %i callbacks\n", event, i);
	return i;
}

int
addeventd(struct module_type *m, char *event,
	 int (*callback)(char *event, void *evdata, void *cbdata),
	 void *data, int prio)
{
	struct event_type *New, *cur, *prev;

	New = malloc(sizeof(*New));
	New->event = strdup(event);
	New->callback = callback;
	New->data = data;
	New->prio = prio;
	New->mid = m;

	for(prev = NULL, cur = evhead; cur; prev = cur, cur = cur->next)
		if(cur->prio > New->prio)
			break;

	New->next = cur;
	if(!prev)
		evhead = New;
	else
		prev->next = New;

	return ANA_OK;
}

int
deleventd(struct module_type *m, char *event,
	 int (*callback)(char *event, void *evdata, void *cbdata),
	 void *data, int flags)
{
	struct event_type *cur, *prev;

	for(prev = NULL, cur = evhead; cur; prev = cur, cur = cur->next)
	{
		if((!flags || cur->data == data) &&
		  !strcasecmp(cur->event, event) && cur->callback == callback)
		{
			free(cur->event);
			if(prev)
				prev->next = cur->next;
			else
				evhead = cur->next;
			free(cur);
			return ANA_OK;
		}
	}
	exc_raise(E_P, "attempt to remove nonexistent handler!");
	return ANA_ERR;
}

static void
deleventmod(struct module_type *m)
{
	struct event_type *cur, *prev;

	for(prev = NULL, cur = evhead; cur; prev = cur, cur = cur->next)
	{
		if(cur->mid == m)
		{
			if(VN("log-autodel"))
				logprintf(LOG_NOTICE, "deleventmod: %s\n", cur->event);
			free(cur->event);
			if(prev)
				prev->next = cur->next;
			else
				evhead = cur->next;
			free(cur);
			if(prev)
				cur = prev;
			else
				cur = evhead;
		}
	}
}

int
split2(char *s, char ***argv, char d)
{
	int argc;
	char *p;

	argc = 0;
	*argv = NULL;

	while(*s)
	{
		while(*s == d)
			s++;
		if(!*s)
			break;
		if((p = strchr(s, d)))
			*p = 0;
		*argv = realloc(*argv, sizeof(char *) * ++argc);
		(*argv)[argc - 1] = strdup(s);
	
		if(p)
			*p++ = d;
		else
			break;
		s = p;
	}

	return argc;
}

int
split(char *s, char ***argv)
{
	return split2(s, argv, ' ');
}

void
splitfree(int argc, char **argv)
{
	if(argc)
	{
		while(--argc)
			free(argv[argc]);
		free(argv);
	}
}

#ifdef LINUX
static void
sig11(int s, siginfo_t *sinfo, void *x)
{
	switch(sinfo->si_code)
	{
		case SI_USER:
			logprintf(LOG_ERR, "SIGSEGV received: killed by uid %i, pid %i\n", sinfo->si_uid, sinfo->si_pid);
			break;
		case SEGV_MAPERR:
			logprintf(LOG_CRIT, "SIGSEGV received: address not mapped to object at %p\n", sinfo->si_addr);
			exit(128 + SIGSEGV);
		case SEGV_ACCERR:
			logprintf(LOG_CRIT, "SIGSEGV received: invalid permissions for object at %p\n", sinfo->si_addr);
			exit(128 + SIGSEGV);
	}
}
#endif

static int
ana_printf(struct session_type *s, char *str)
{
	return printf("%s", str);
}

struct session_type ana_sess;

static void
ana_logout(struct session_type *s)
{
	int f;

	if((f = fork()) != 0)
	{
		if(f != -1)
			exit(0);
		else
			exc_raise(E_P, "fork failed (%s)", strerror(errno));
	}
	else
	{
		close(0);
		close(1);
		close(2);
		delsession(&ana_sess);
	}
}

static int s_argc;
static char **s_argv;

void
restart(void)
{
	int i;

	console_deinit();
	i = 1024; // TODO: actual max of fds
	while(--i > 2) close(i); // TODO: what if no console session
	execv(s_argv[0], s_argv);
}

static void
init(int argc, char *argv[])
{
	static struct userconn_type uc = {ana_printf, ana_logout, 0};
#if defined(LINUX) && !defined(DEBUG)
	struct sigaction s11h;

	memset(&s11h, 0, sizeof(s11h));
	s11h.sa_sigaction = sig11;
	s11h.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &s11h, NULL);
#endif

	s_argc = argc; s_argv = argv;

	signal(SIGPIPE, SIG_IGN);
	time(&nowtime);
	srand(nowtime);
	uptime = nowtime;

	addcmd(NULL, "insmod",	Uinsmod,	"Usage: insmod file [module params]\n  Loads a module.\n");
	addcmd(NULL, "lsmod",	Ulsmod,		"Usage: lsmod\n  Lists all loaded modules.\n");
	addcmd(NULL, "rmmod",	Urmmod,		"Usage: rmmod modulename\n  Removes the named module.\n");
	addcmd(NULL, "lsmem",	Ulsmem,		"Usage: lsmem modulename\n  Show malloc information about module.\n");

	log_init();
	cmd_init();
	user_init();

	ana_sess.conn = &uc;
	ana_sess.user = finduser("root");
	ana_sess.from = "-";
	ana_sess.line = "tty1";
	ana_sess.login = ana_sess.idle = nowtime;
	set_current_session(&ana_sess);
	addsession(&ana_sess);

	if(argc == 1)
	{
		if(access(CONFIG_FILE, R_OK) == 0)
			execf(CONFIG_FILE);
	}
	else
		execf(argv[1]);
	exc_print();

	console_init();
	pr_pr();
	set_current_session(NULL);
}

static void
main_loop(void)
{
	int i, ret;
	fd_set rset, wset;
	struct timeval tv;
	struct aevent_type *cur;
	time_t lastttime;

	lastttime = nowtime - 1;

	while(1)
	{
		memcpy(&rset, &grset, sizeof(fd_set));
		memcpy(&wset, &gwset, sizeof(fd_set));
		tv.tv_usec = 10000;
		tv.tv_sec = 0;

		ret = select(1024, &rset, &wset, NULL, &tv);
		if(ret == -1)
		{
			if(errno != EINTR)
			{
				logprintf(LOG_ERR, "main_loop: select: %s\n", strerror(errno));
				for(i = 0; i < 1024; i++)
				{
					if(socks[S_READ][i] && fcntl(i, F_GETFD, 0) == -1)
					{
						logprintf(LOG_ERR, "main_loop: select: removing fd r/%i\n", i);
						socks[S_READ][i](i, sock_arg[S_READ][i]);
						exc_print();
						if(socks[S_READ][i])
							delsock(NULL, S_READ, i);
					}
					else if(socks[S_WRITE][i] && fcntl(i, F_GETFD, 0) == -1)
					{
						logprintf(LOG_ERR, "main_loop: select: removing fd w/%i\n", i);
						socks[S_WRITE][i](i, sock_arg[S_WRITE][i]);
						exc_print();
						if(socks[S_WRITE][i])
							delsock(NULL, S_WRITE, i);
					}
				}
			}
			continue;
		}

		time(&nowtime);
/*		gettimeofday( &tv, NULL );
		nowtime = tv.tv_sec;*/

		if(rrs)
		for(i = 0; i <= brsd && ret; i++)
			if(socks[S_READ][i] && FD_ISSET(i, &rset))
			{
				socks[S_READ][i](i, sock_arg[S_READ][i]), ret--;
				exc_print();
			}

		if(wrs)
		for(i = 0; i <= bwsd && ret; i++)
			if(socks[S_WRITE][i] && FD_ISSET(i, &wset))
			{
				socks[S_WRITE][i](i, sock_arg[S_WRITE][i]), ret--;
				exc_print();
			}

		for(cur = falw; cur; cur = cur->next)
		{
			cur->handler();
			exc_print();
		}

		if(nowtime != lastttime)
		{
			struct tevent_type *cur_t, *next/*, *cur2, *prev*/;

			if(nowtime != lastttime + 1)
			{
				logprintf(LOG_ERR, "main_loop: time is expected to be %i, not %i\n",
						lastttime + 1, nowtime);
			}
			lastttime = nowtime;

			for(cur_t = tevents; cur_t; cur_t = next)
			{
				next = cur_t->next;
				if(cur_t->nextwhen <= nowtime)
				{
					cur_t->nextwhen = nowtime + cur_t->interval;
					cur_t->handler(cur_t->arg);
					exc_print();
/*					for( cur2 = cur_t->next;*//*illene*/
				}
/*				else
					break;*/
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	printf("ana v"VER", (c) Matyas Koszik, 2001-2006, 2016.\n");

	if(argc > 2)
	{
		printf("usage: %s [config]\n", argv[0]);
		exit(1);
	}

	init(argc, argv);
	main_loop();
	return 0;
}

