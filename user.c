/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2004.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#define __USE_BSD
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include <main.h>
#include <user.h>
#include <readuser.h>
#include <log.h>
#include <variable.h>

static struct user_type *users;
int usern;

static struct ctype_type *cts;
int ctn;

static struct session_type *currentsess;

struct user_type *
finduser(char *user)
{
	int i;

	for(i = 0; i < usern; i++)
		if(!strcmp(users[i].name, user))
			return &users[i];
	return NULL;
}

int
adduser(char *user)
{
	int i;

	if(finduser(user))
	{
		exc_raise(E_P, "user %s already exists", user);
		return ANA_ERR;
	}
	i = usern++;
	users = realloc(users, sizeof(*users) * usern);
	users[i].name = strdup(user);
	users[i].creds = NULL;
	users[i].credn = 0;
	return ANA_OK;
}

struct ctype_type *
findctype(char *cred)
{
	int i;

	for(i = 0; i < ctn; i++)
		if(!strcmp(cts[i].name, cred))
			return &cts[i];
	return NULL;
}

struct ctype_type *
addctype(char *cred)
{
	int i;

	if(findctype(cred))
	{
		exc_raise(E_P, "ctype %s already exists", cred);
		return NULL;
	}
	i = ctn++;
	cts = realloc(cts, sizeof(*cts) * ctn);
	memset(&cts[i], 0, sizeof(cts[i]));
	cts[i].name = strdup(cred);
	return &cts[i];
}

struct cred_type *
findcred(struct user_type *user, struct ctype_type *c)
{
	int i;

	for(i = 0; i < user->credn; i++)
		if(user->creds[i].type == c)
			return &user->creds[i];
	return NULL;
}

int
setcreds(char *user, char *cred, void *data)
{
	struct user_type *u;

	if((u = finduser(user)) == NULL)
	{
		exc_raise(E_P, "user %s does not exist", user);
		return ANA_ERR;
	}
	return setcredsN(u, cred, data);
}

int
setcredsN(struct user_type *u, char *cred, void *data)
{
	int i;
	struct cred_type *c;
	struct ctype_type *ct;

	if((ct = findctype(cred)) == NULL)
	{
		exc_raise(E_P, "ctype %s does not exist", cred);
		return ANA_ERR;
	}

	if((c = findcred(u, ct)) == NULL)
	{
		i = u->credn++;
		u->creds = realloc(u->creds, sizeof(*u->creds) * u->credn);
		c = &u->creds[i];
		c->type = ct;
	}
	c->data = data;
	return ANA_OK;
}

static char *
ana_display(void *data)
{
	char tmp[50];

	snprintf(tmp, sizeof(tmp), "%i", data);
	return strdup(tmp);
}

static int
ana_check(void *data, void *cred)
{
	return data == cred;
}

static int
passwd_check(void *data, void *cred)
{
	return !strcmp(data, cred);
}

char *
cred_display(void *data)
{
	return strdup(data);
}

int
user_check(struct user_type *u, char *cred, void *tst)
{
	struct ctype_type *ct;
	struct cred_type *c;

	if((ct = findctype(cred)) == NULL)
	{
		exc_raise(E_P, "ctype %s does not exist", cred);
		return -2;
	}
	if((c = findcred(u, ct)) == NULL)
		return -1;

	return ct->check(c->data, tst);
}

int
check_current_user(char *cred, void *tst)
{
	struct ctype_type *ct;
	struct cred_type *c;

	if(!currentsess)
	{
		exc_raise(E_P, "there's no current session");
		return -3;
	}
	if(!currentsess->user)
	{
		exc_raise(E_P, "the current session has no user associated with it");
		return -4;
	}
	if((ct = findctype(cred)) == NULL)
	{
		exc_raise(E_P, "ctype %s does not exist", cred);
		return -2;
	}
	if((c = findcred(currentsess->user, ct)) == NULL)
		return -1;

	return ct->check(c->data, tst);
}

void
set_current_session(struct session_type *s)
{
	currentsess = s;
}

struct session_type *
get_current_session(void)
{
	return currentsess;
}

struct user_type *
get_current_user(void)
{
	if(!currentsess)
	{
		exc_raise(E_P, "no current session");
		return NULL;
	}
	return currentsess->user;
}

char *
get_cusername(void)
{
	if(!currentsess)
	{
		exc_raise(E_P, "no current session");
		return "*UNKOWN*";
	}
	return currentsess->user->name;
}

int
uprintf(char *fmt, ...)
{
	va_list list;
	char buf[32768];
	int rv;

	if(!currentsess)
	{
		exc_raise(E_P, "there's no current session");
		return -1;
	}

	va_start(list, fmt);
	vsnprintf(buf, sizeof(buf), fmt, list);
	va_end(list);

	if(currentsess->conn->flags & UC_LINEBUF)
	{
		int s, l;
		char *b2, *ptr;

		s = strlen(buf);
		l = currentsess->outbuf?strlen(currentsess->outbuf):0;
		currentsess->outbuf = realloc(currentsess->outbuf, l + s + 1);
		currentsess->outbuf[l] = 0;
		strcat(currentsess->outbuf, buf);
		rv = 0;
		while((ptr = strchr(currentsess->outbuf, '\n')) != NULL)
		{
			char save;

			save = *++ptr;
			*ptr = 0;
			rv += currentsess->conn->printf(currentsess, currentsess->outbuf);
			*ptr = save;
			b2 = strdup(ptr);
			free(currentsess->outbuf);
			currentsess->outbuf = b2;
		}
		return rv;
	}
	else
		return currentsess->conn->printf(currentsess, buf);
}

static struct session_type **sessions;
static int sessionn;

void
uprintall(const char *fmt, va_list list)
{
	char buf[32768];
	int i;


	vsnprintf(buf, sizeof(buf), fmt, list);

	for(i = 0; i < sessionn; i++)
		sessions[i]->conn->printf(sessions[i], buf);
}

void
uprintfall(const char *fmt, ...)
{
	va_list lst;

	va_start(lst, fmt);
	uprintall(fmt, lst);
	va_end(lst);
}

struct session_type *
get_session_by_line(char *line)
{
	int i;

	for(i = 0; i < sessionn; i++)
		if(sessions[i]->line && !strcmp(sessions[i]->line, line))
			return sessions[i];
	return NULL;
}

void
addsession(struct session_type *s)
{
	int i;

	i = sessionn++;
	sessions = realloc(sessions, sizeof(*sessions) * sessionn);
	sessions[i] = s;
}

int
delsession(struct session_type *s)
{
	int i;

	for(i = 0; i < sessionn; i++)
		if(sessions[i] == s)
		{
			if(i != --sessionn)
				memcpy(&sessions[i], &sessions[sessionn], sizeof(*sessions));
			sessions = realloc(sessions, sizeof(*sessions) * sessionn);
			return ANA_OK;
		}
	return ANA_ERR;
}

static void
Ulogout(int argc, char *argv[])
{
	if(!currentsess)
	{
		exc_raise(E_P, "there's no current session");
		return;
	}
	if(currentsess->conn->logout)
		currentsess->conn->logout(currentsess);
}

static void print_time_ival7(time_t t) {
    if((long)t < (long)0){  /* system clock changed? */
      printf("   ?   ");
      return;
    }
    if (t >= 48*60*60)                          /* > 2 days */
        uprintf(" %2ludays", t/(24*60*60));
    else if (t >= 60*60)                        /* > 1 hour */
        uprintf(" %2lu:%02um", t/(60*60), (unsigned) ((t/60)%60));
    else if (t > 60)                            /* > 1 minute */
        uprintf(" %2lu:%02u ", t/60, (unsigned) t%60);
    else
        uprintf(" %2lu.%02us", t, 0);
}

static void print_logintime(time_t logt) {
    char weekday[][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
         month  [][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                        "Aug", "Sep", "Oct", "Nov", "Dec" };
    struct tm *logtm, *curtm;
    int today;

    curtm = localtime(&nowtime);
    /* localtime returns a pointer to static memory */
    today = curtm->tm_yday;
    logtm = localtime(&logt);
    if (nowtime - logt > 12*60*60 && logtm->tm_yday != today) {
        if (nowtime - logt > 6*24*60*60)
            uprintf(" %02d%3s%02d", logtm->tm_mday, month[logtm->tm_mon],
                    logtm->tm_year % 100);
        else
            uprintf(" %3s%02d  ", weekday[logtm->tm_wday], logtm->tm_hour);
    } else {
        uprintf(" %02d:%02d  ", logtm->tm_hour, logtm->tm_min);
    }
}


static void
Uw(int argc, char *argv[])
{
	int i;
	struct tm *tm;
	int ud, uh, us, um;

	tm = localtime(&nowtime);
	uprintf(" %02d:%02d:%02d up ", tm->tm_hour, tm->tm_min, tm->tm_sec);
	us = nowtime - uptime;
	if((ud = us / 86400))
		uprintf("%i day%s, ", ud, ud > 1? "s": "");
	um = (us / 60) % 60;
	if((uh = (us / 3600) % 24))
		uprintf("%2d:%02d, ", uh, um);
	else
		uprintf("%d min, ", um);
	uprintf("%2d user%s\n", sessionn, sessionn != 1? "s" :"");


	uprintf("USER     TTY      FROM              LOGIN@   IDLE\n");


	for(i = 0; i < sessionn; i++)
	{
		if(!sessions[i]->login)
			continue;
		uprintf("%-9.8s%-9.8s", sessions[i]->user?
			sessions[i]->user->name:"NOUSER", sessions[i]->line);
		uprintf("%-16s", sessions[i]->from);
		print_logintime(sessions[i]->login);
		print_time_ival7(nowtime - sessions[i]->idle);
		uprintf("\n");
	}
}


static void
Upasswd(int argc, char *argv[])
{
	struct user_type *user;

	if(argc < 2 || argc > 3)
	{
		uprintf("Usage: passwd [user] <new_password>\n");
		return;
	}

	user = get_current_user();
	if(!user)
	{
		uprintf("there's no current user\n");
		return;
	}
	if(argc == 3)
	{
		if(user_check(user, "ana", (void *)1) != 1)
		{
			uprintf("you can't change other users' password\n");
			return;
		}
		user = finduser(argv[1]);
		if(!user)
		{
			uprintf("%s: no such user\n", argv[1]);
			return;
		}
	}

	setcredsN(user, "passwd", strdup(argv[argc - 1]));
}

static void
Usu(int argc, char *argv[])
{
	struct user_type *user;

	if(argc < 2 || argc > 3)
	{
		uprintf("Usage: su <user> [<password>]\n");
		return;
	}

	if(!currentsess)
	{
		uprintf("there's no current session!\n");
		return;
	}
	if(!currentsess->user)
	{
		uprintf("there's no current user!\n");
		return;
	}

	user = finduser(argv[1]);
	if(!user)
	{
		uprintf("%s: no such user\n", argv[1]);
		return;
	}
	if(user_check(currentsess->user, "ana", (void *)1) == 1 ||
	   (argc == 3 && user_check(user, "passwd", argv[2]) == 1))
	{
		currentsess->user = user;
	}
	else
		uprintf("Authentication failure\n");
}

static char *
v_user_get(struct variable_type *c)
{
	if(!currentsess || !currentsess->user)
		return "";
	return currentsess->user->name;
}

static void
Uid(int argc, char *argv[])
{
	struct user_type *user;
	int i;

	if(!currentsess)
	{
		uprintf("there's no current session!\n");
		return;
	}

	if(argc > 1)
		user = finduser(argv[1]);
	else
		user = currentsess->user;

	if(!user)
	{
		uprintf("user not found\n");
		return;
	}

	uprintf("user: %s\n", user->name);
	for(i = 0; i < user->credn; i++)
		if((user->creds[i].type->flags & CRED_PUBLIC) &&
		   user->creds[i].type->display)
		{
		   	char *t;

			t = user->creds[i].type->display(user->creds[i].data);
			uprintf("-- %s: [%s]\n", user->creds[i].type->name, t);
			free(t);
		}
}

void
user_init(void)
{
	struct ctype_type *c;


	c = addctype("ana");
	c->check = ana_check;
	c->display = ana_display;
	c->flags = CRED_PUBLIC;

	c = addctype("passwd");
	c->check = passwd_check;
	c->free = free;
	c->display = cred_display;

	adduser("root");
	setcreds("root", "ana", (void *)1);

	adduser("guest");
	setcreds("guest", "passwd", strdup("guest"));

	addcmd(NULL, "logout",	Ulogout,	"Usage: logout\n  Logs out\n");
	addcmd(NULL, "w",	Uw,		"Usage: w\n  Show who is logged on.\n");
	addcmd(NULL, "passwd",	Upasswd,	"Usage: passwd [user] newpassword\n  Change password.\n");
	addcmd(NULL, "su",	Usu,		"Usage: su user [password]\n  Change user.\n");
	addcmd(NULL, "id",	Uid,		"Usage: id [user]\n  Show info about an user.\n");

	add_bi_pr(NULL, "USER", v_null_set, v_user_get);
}
