/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2006.
 *
 */
#ifndef __MODULE_H
#define __MODULE_H

#define HT(x) \
static void \
x(void *arg)

#ifdef __MODULE__
#define N_S_S \
struct sym_tbl_type import_table[] = \
{
#define N_S(x) \
{ #x , &x }

#define N_S_E \
{ NULL, NULL } \
}

#ifdef __STATIC__
#	define MOD_INIT(x)	x##_init (int argc, char *argv[])
#	define MOD_CLEANUP(x)	x##_cleanup (void)
#	define MOD_NAME(x)	/* TODO: module_id */
#else
#	define MOD_INIT(x)	init (int argc, char *argv[])
#	define MOD_CLEANUP(x)	cleanup (void)
#	define MOD_NAME(x)	char module_name[] = #x; void *module_id;
/*char ana_version[] = VER;*/
#endif

int MOD_INIT(MODULE_NAME);
void MOD_CLEANUP(MODULE_NAME);
#endif

struct sym_tbl_type
{
	char	*name;
	void	*addr;
};

struct sym_exp_tbl_type
{
	char	*name;
	void	*addr;
	int	 used;
};

int loadmodule(const char *modpath, int argc, char *argv[]);
int delmod(const char *name);
int addtevent(TYPE m, int interval, teventh_t handler_, void *arg);
int deltevent(TYPE m, int interval, teventh_t handler_, void *arg);
void addsock(TYPE m, int type, int sd, sockh_t hl, void *sa);
void delsock(TYPE m, int type, int sd);
void addaevent(TYPE m, aeventh_t handler_);
void delaevent(TYPE m, aeventh_t handler_);
int emit(char *event, void *data);

typedef int (*callback_t)(char *event, void *evdata, void *cbdata);
int addeventd(TYPE m, char *event, callback_t callback, void *data, int prio);
int deleventd(TYPE m, char *event, callback_t callback, void *data, int flags);
#if defined(__MODULE__)
#	define addevent(e, cb, p) addeventd(module_id, e, (callback_t)cb, NULL, p)
#	define delevent(e, cb) deleventd(module_id, e, (callback_t)cb, NULL, 0)
#	define addtevent(i, h, a) addtevent(module_id, i, h, a)
#	define deltevent(i, h, a) deltevent(module_id, i, h, a)
#	define addsock(t, s, h, sa) addsock(module_id, t, s, h, sa)
#	define delsock(t, s) delsock(module_id, t, s)
#	define addaevent(h) addaevent(module_id, h)
#	define delaevent(h) delaevent(module_id, h)
#endif

int split(char *s, char ***argv);
int split2(char *s, char ***argv, char d);
void splitfree(int argc, char **argv);
char *ana_xstrdup(const char *s, int size);
void restart(void);

void ana_real_sleep(jmp_buf env, unsigned int seconds);

#define ana_sleep(sec) {jmp_buf env; if(!setjmp(env)) ana_real_sleep(env, sec);}

#endif
