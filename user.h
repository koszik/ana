/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2004.
 *
 */
#ifndef __USER_H
#define __USER_H

struct user_type *finduser(char *user);
int adduser(char *user);
struct ctype_type *findctype(char *cred);
struct ctype_type *addctype(char *cred);
struct cred_type *findcred(struct user_type *user, struct ctype_type *c);
int setcreds(char *user, char *cred, void *data);
int setcredsN(struct user_type *u, char *cred, void *data);
char *cred_display(void *data);
int user_check(struct user_type *u, char *cred, void *tst);
void user_init(void);
int check_current_user(char *cred, void *tst);
void set_current_session(struct session_type *session);
struct session_type *get_current_session(void);
int uprintf(char *fmt, ...);
void addsession(struct session_type *s);
int delsession(struct session_type *s);
struct user_type *get_current_user(void);
void uprintall(const char *fmt, va_list list);
void uprintfall(const char *fmt, ...);
char *get_cusername(void);
struct session_type *get_session_by_line(char *line);

#define UCHECK(type,val) \
	if(check_current_user((type), (void*)(val)) != 1) \
	{ uprintf("%s: You don't have the permission to use this command\n", argv[0]); \
	return; }

#endif
