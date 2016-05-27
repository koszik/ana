/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2003, 2006.
 *
 */
#include <stdio.h>
#define __USE_BSD
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define __READUSER_C
#include <main.h>
#include <readuser.h>
#include <conio.h>
#include <module.h>
#include <variable.h>
#include <user.h>
#include <log.h>

int quit;

static int comments;
static char cmd[1024];
static int ed_pos;
static char *PrevCmd[MAX_HIST];
static char *PS1;
static struct cmd_type firstcmd;
static int HCptr, HCcount;
static char PSbuff[1024];
static int PS1len;
static int tab_print;


void
execf(char *file)
{
	FILE *fd;
	char buff[2048];

	if((fd = fopen(file, "r")) == NULL)
	{
		uprintf( "exec: opening %s: %s\n", file, strerror( errno ) );
		return;
	}

	while( fgets( buff, sizeof( buff ), fd ) )
	{
		delrn( buff );
		getUser( buff );
	}
}

static void
Uwhoami(int argc, char *argv[])
{
	struct session_type *s;

	s = get_current_session();
	if(!s)
		uprintf("UNKNOWN (no current session!)\n");
	else if(!s->user)
		uprintf("UNKNOWN (no user associated with session!)\n");
	else
		uprintf("%s\n", s->user->name);
}

static void
Uhelp( int argc, char *argv[] )
{
	struct cmd_type *cur, *prev;
	int n;

	prev = NULL; /* shut up gcc */

	if( argc > 2 )
	{
		uprintf( "help: Too many parameters... See 'help help'\n" );
		return;
	}

	if( argc == 1 )
	{
		uprintf( "help: command list. For more info use 'help command'.\n"
			"\n" );

		n = -1;
		for( cur = firstcmd.next; cur; prev = cur, cur = cur->next )
			if( cur == firstcmd.next || strcmp( cur->name, prev->name ) )
			{
				if( n != -1 )
				{
					uprintf( ", " );
					n += 2;
				}
				else
					n = 0;

				n += strlen( cur->name );
				if( n > 75 )
				{
					n = strlen( cur->name );
					uprintf( "\n" );
				}
				uprintf( "%s", cur->name );
			}

		if( n )
			uprintf( "\n" );
		return;
	}

	for( cur = firstcmd.next; cur; cur = cur->next )
		if( !strcmp( argv[1], cur->name ) )
			break;

	if( !cur )
		uprintf( "help: command '%s' unknown\n", argv[1] );
	else
		uprintf( "%s", cur->help );
}

static void
Uset( int argc, char *argv[] )
{
	UCHECK("ana", 1);
	if( argc == 1 )
	{
		list_variables();
		return;
	}
	if( argc == 3 )
		set_var( argv[1], argv[2] );
}

static void
Uunset( int argc, char *argv[] )
{
	int i;

	UCHECK("ana", 1);
	for( i = 1; i < argc; i++ )
		del_var( argv[i] );
}

static void
Uclear( int argc, char *argv[] )
{
	uprintf( "\033[2J\033[H" );
}

static void
Uexec( int argc, char *argv[] )
{
	int s;

	UCHECK("ana", 1);
	if(argc != 2)
	{
		uprintf("usage: %s file\n", argv[0]);
		return;
	}

	s = comments;
	comments = 1;
	execf(argv[1]);
	comments = s;
}

HT(slx)
{
	FILE *fd;
	char buff[2048];

	fd = *(FILE **)arg;

	if(fgets(buff, sizeof( buff ), fd ) == NULL)
	{
		deltevent(NULL, ((int *)arg)[1], slx, arg);
		free( arg );
		fclose( fd );
		uprintf( "slexec done...\n" );
		return;
	}

	delrn( buff );
	getUser( buff );
}

static void
Uslexec( int argc, char *argv[] )
{
	int *arg;

	UCHECK("ana", 1);
	if( argc != 3 )
	{
		uprintf( "usage: %s file time\n", argv[0] );
		return;
	}

	arg = malloc( sizeof( int ) * 2 );

	arg[1] = atoi( argv[2] );

	if(( arg[0] = (int)fopen( argv[1], "r" )) == 0 )
	{
		uprintf( "%s: opening %s: %s\n", argv[0], argv[1], strerror( errno ) );
		return;
	}

	addtevent(NULL, arg[1], slx, (void *)arg );
}

static void
Ureset(int argc, char *argv[])
{
/* FIXME: ez csak a konzollal mukodik jol */
	con_reset(0);
	uprintf("\033[2J\033[H");
	fflush(stdout);
}

static void
Uecho( int argc, char *argv[] )
{
	int na, nonl, pr;

	na = 1;
	pr = nonl = 0;

	if( argc > 1 && !strcmp( argv[1], "-n" ) )
	{
		nonl = 1;
		na++;
	}
	while( na < argc )
	{
		uprintf( "%s%s", pr? " ": "", argv[na++] );
		pr = 1;
	}
	if(!nonl)
		uprintf("\n");
}

static void
Uver( int argc, char *argv[] )
{
	uprintf("ana v"VER", (c) Matyas Koszik, 2001-2006.\n"
		"Compiled on: " __DATE__ " " __TIME__ "\n");
}


#define free_args( argc, argv )						\
{									\
	int i;								\
	for(i = 0; i < argc; i++)					\
		free(argv[i]);						\
}

extern struct session_type ana_sess;

static void
getUser_wrapper(int dummy, void *dummy2)
{
	char *realcmd;

	(void)dummy;
	(void)dummy2;

	set_current_session(&ana_sess);
	ana_sess.idle = nowtime;

	if((realcmd = getuser()) != NULL)
	{
		getUser(realcmd);
		if(!quit)
			pr_pr();
	}

	set_current_session(NULL);
}



void
getUser( const char *cmd )
{
	char *argv[MAX_ARGS], *ptr2, buff[1024], varname[1024], *vp;
	const char *ptr;
	char cc;
	int argc, isstr, prevspc, esc, nchar, dollar;
	int unum;
	int unumbase;
	struct cmd_type *cur;

	vp = NULL;

	while( *cmd )
	{
		dollar = unumbase = isstr = prevspc = argc = esc = 0;
		ptr = cmd;
		ptr2 = buff;
		unum = -1;

		memset( buff, 0, sizeof( buff ) );
		memset( argv, 0, sizeof( argv ) );


		while( (*ptr || dollar) &&
		       ( esc || isstr || (*ptr != ';' && *ptr != '#') ) )
		{
			if( prevspc )
				prevspc--;

			nchar = -1;

			if( dollar == 2 )
			{
				if( !*vp )
				{
					dollar = 0;
					cc = *ptr;
				}
				else
					cc = *vp++;
			}
			else
				cc = *ptr;

			if( dollar == 1 )
			{
				const char *nptr;
				char *v;

				nptr = ptr;
				v = varname;
				if( *ptr != '{' )
				{
					while( (*ptr >= '0' && *ptr <= '9') ||
					       (*ptr >= 'a' && *ptr <= 'z') ||
					       (*ptr >= 'A' && *ptr <= 'Z') ||
					        *ptr == '_' )
						*v++ = *ptr++;
				}
				else
				{
					ptr++;
					while( *ptr && *ptr != '}' )
						*v++ = *ptr++;
					if( !*ptr )
					{
						uprintf( "alsh: please use {}-s in pairs...\n" );
						free_args( argc, argv );
						return;
					}
					ptr++;
				}
				*v = '\0';

				if( nptr == ptr )
				{
					dollar = 0;
					nchar = *ptr;
				}
				else
				{
					*--ptr2 = '\0';
					vp = get_var_str( varname );
					dollar = 2;
				}
			}
			else if( !esc && unum == -1 )
			{
				if( cc == '"' && !dollar )
				{
					if( isstr == 2 )
						isstr = 0;
					else if( isstr == 0 )
						isstr = 2;
					else
						nchar = '"';
				}
				else if( cc == '\'' && !dollar )
				{
					if( isstr == 1 )
						isstr = 0;
					else if( isstr == 0 )
						isstr = 1;
					else
						nchar = '\'';
				}
				else if( cc == ' ' || cc == '\t' )
				{
					if( !isstr )
						prevspc = 2;
					else
						nchar = ' ';
				}
				else if( cc == '\\' && isstr != 1 )
					esc = 1;
				else if( cc == '$' && isstr != 1 &&
					 dollar == 0 )
				{
					nchar = cc;
					dollar = 1;
				}
				else
					nchar = cc;
			}
			else
			{
				esc = 0;

				if( unum != -1 )
				{
					int unumtmp;
					char mch;
					int nn;

					unumtmp = unum;
					mch = (cc >= 'a' && cc <= 'z'? cc - 32: cc);
					for( nn = 0; nn < 16; nn++ )
						if( "0123456789ABCDEF"[nn] == mch )
							break;

					if( nn >= unumbase )
					{
						*ptr2++ = unum;
						unum = -1;
						continue;
					}

					unum *= unumbase;
					unum += nn;

					if( unum > 255 )
					{
						*ptr2++ = unumtmp;
						unum = -1;
						continue;
					}
				}
				else
					switch( cc )
					{
						case '\\':
							nchar = '\\';
							break;
						case 'r':
							nchar = '\r';
							break;
						case 'n':
							nchar = '\n';
							break;
						case 'a':
							nchar = '\a';
							break;
						case 'b':
							nchar = '\b';
							break;
						case 'e':
							nchar = '\033';
							break;
						case 't':
							nchar = '\t';
							break;
						case 'x':
							unum = 0;
							unumbase = 16;
							break;
						case '0':
							unum = 0;
							unumbase = 8;
							break;
						default:
							if( cc >= '1' && cc <= '9' )
							{
								unum = cc - '0';
								unumbase = 10;
							}
							else
								nchar = cc;
					}
			}

			if( prevspc == 1 && ( argc || ptr2 != buff ))
			{
				argv[argc++] = strdup(buff);
				memset( buff, 0, sizeof( buff ) );
				ptr2 = buff;
				if( argc == MAX_ARGS - 1 )
				{
					uprintf( "error: MAX_ARGS = %i\n", MAX_ARGS );
					free_args( argc, argv );
					return;
				}
			}

			if( nchar != -1 )
				*ptr2++ = nchar;
			if( dollar != 2 )
				ptr++;
		}

		if( *ptr == ';' )
			cmd = ptr + 1;
		else
			cmd = ptr;

		if( unum != -1 )
			*ptr2++ = unum;

		if( isstr )
		{
			uprintf( "alsh: please use `%c'-s in pairs...\n",
				isstr == 1? '\'': '"' );
			free_args( argc, argv );
			return;
		}

		if( esc )
		{
			uprintf( "alsh: \\\\\\n is not a valid escape sequent...\n" );
			free_args( argc, argv );
			return;
		}

		argv[argc++] = strdup(buff);

		if( !argv[0][0] )
			return;

		for( cur = firstcmd.next; cur; cur = cur->next )
			if( !strcmp( argv[0], cur->name ) )
				break;

		if( !cur )
			uprintf( "alsh: %s: command not found\n", argv[0] );
		else
			cur->handler( argc, argv );

		free_args( argc, argv );
	}
}

static int
prprompt(const char *cmd)
{
	char buff[1024];

	snprintf(buff, sizeof(buff), "xecho \"%s\"", PS1);
	getUser(buff);
	PS1len = uprintf("%s", PSbuff);
	return PS1len + uprintf("%s", cmd);
}

static void
Uxecho(int argc, char *argv[])
{
	if(argc == 2)
		snprintf(PSbuff, sizeof(PSbuff), "%s", argv[1]);
}


static void
Uquit(int argc, char *argv[])
{
	UCHECK("ana", 1);
	exit(0);
}

static void
Urestart(int argc, char *argv[])
{
	UCHECK("ana", 1);
	restart();
}

static void
HistCmd(int dir)
{
	HCptr += dir;
	if(HCptr < 0)
		HCptr = 0;
	memset(cmd, 0, sizeof(cmd));
	if(HCptr >= HCcount)
		HCptr = HCcount;
	else
		strcpy(cmd, PrevCmd[HCptr]);

	ed_pos = strlen(cmd);
	prprompt(cmd);
}

#define LINE_LEN	80

static void
posstr(int *cur, int newp)
{
	int ccur;

	ccur = *cur;
	*cur = newp;

	if( ccur / LINE_LEN < newp / LINE_LEN )
		uprintf( "\033[%iB", newp / LINE_LEN - ccur / LINE_LEN );
	if( ccur / LINE_LEN > newp / LINE_LEN )
		uprintf( "\033[%iA", ccur / LINE_LEN - newp / LINE_LEN );
	ccur = ccur % LINE_LEN;
	newp = newp % LINE_LEN;
	if(ccur < newp)
		uprintf( "\033[%iC", newp - ccur );
	if(ccur > newp)
		uprintf( "\033[%iD", ccur - newp );
}

void
pr_pr( void )
{
	memset(cmd, 0, sizeof(cmd));
	prprompt("");
	fflush( stdout ); /* TODO: fflush_session(); */
}

static char *
getuser( void )
{
	int be, i, o_pos;

	o_pos = ed_pos + PS1len;

	if( tab_print )
		tab_print--;

	be = getch();
	if(be == KEY_UP || be == KEY_DOWN)
	{
		memset( cmd, ' ', strlen( cmd ) );
		posstr( &o_pos, PS1len );
		o_pos += uprintf( cmd );
		posstr( &o_pos, 0 );
		HistCmd( be == KEY_UP? -1: +1 );
		o_pos = strlen( cmd ) + PS1len;
	}
	else if( be == KEY_CTRL_L )
	{
		uprintf( "\033[2J\033[H" );
		prprompt( cmd );
		o_pos = PS1len + strlen( cmd );
	}
	else if( be == KEY_LEFT )
	{
		if( ed_pos )
			ed_pos--;
	}
	else if( be == KEY_RIGHT )
	{
		if( ed_pos < strlen( cmd ) )
			ed_pos++;
	}
	else if( be == KEY_HOME )
		ed_pos = 0;
	else if( be == KEY_END )
		ed_pos = strlen( cmd );
	else if( be == KEY_ENTER )
	{
		uprintf( "\n" );
		ed_pos = 0;

		if( cmd[0] != '\0' && (HCcount == 0 || strcmp( PrevCmd[HCcount - 1], cmd ) != 0 ))
		{
			if( HCcount == MAX_HIST )
			{
				free( PrevCmd[0] );
				for( i = 1; i < MAX_HIST; i++ )
					PrevCmd[i - 1] = PrevCmd[i];
				PrevCmd[MAX_HIST - 1] = NULL;
				HCcount--;
			}
			free(PrevCmd[HCcount]);
			PrevCmd[HCcount] = strdup(cmd);
			HCcount++;
		}
		HCptr = HCcount;
		return( cmd );
	}
	else if( be == KEY_BACKSPACE )
	{
		if( ed_pos )
		{
			int op;

			posstr( &o_pos, strlen( cmd ) - 1 + PS1len );
			uprintf( " " );
			o_pos++;
			if( !(o_pos % LINE_LEN) )
				uprintf( "\n" );
			ed_pos--;
			memmove( cmd + ed_pos, cmd + ed_pos + 1, sizeof( cmd ) - ed_pos - 1 );
			posstr( &o_pos, ed_pos + PS1len );
			op = uprintf( "%s", cmd + ed_pos );
			o_pos += op;
			if( op && !(o_pos % LINE_LEN) )
				uprintf( "\n" );
		}
	}
	else if( be == KEY_DEL )
	{
		if( ed_pos < strlen( cmd ) )
		{
			int op;

			posstr( &o_pos, strlen( cmd ) - 1 + PS1len );
			uprintf( " " );
			o_pos++;
			if( !(o_pos % LINE_LEN) )
				uprintf( "\n" );
			memmove( cmd + ed_pos, cmd + ed_pos + 1, sizeof( cmd ) - ed_pos - 1 );
			posstr( &o_pos, ed_pos + PS1len );
			op = uprintf( "%s", cmd + ed_pos );
			o_pos += op;
			if( op && !(o_pos % LINE_LEN) )
				uprintf( "\n" );
		}
	}
	else if( be == 9 ) /* TODO : tobbsor, sta<TAB>dfsdf -> stats dfsdf */
	{
		int z, nn, tnum;
		char *ptr;
		struct cmd_type *cur, *nnum;

		nnum = NULL;
		nn = 0;
		tnum = 0;

		ptr = cmd;
		while( *ptr == ' ' )
			ptr++;

		z = strlen( ptr );

		for( cur = firstcmd.next; cur; cur = cur->next )
			if( !(nnum && !strcmp( nnum->name, cur->name )) &&
			!strncmp( ptr, cur->name, z ) )
			{
				nnum = cur;
				nn++;
				if( tab_print )
				{
					if( !tnum )
						uprintf( "\n" );
					tnum++;
					tnum &= 3;
					uprintf( "%-17s", cur->name );
					/* leghosszabb + 1 */
				}
			}

		if( nn == 1 )
		{
			strcpy( ptr, nnum->name );
			strcat( cmd, " " );
			ed_pos = strlen( cmd );
			uprintf( "\r" );
			prprompt( cmd );
		}
		else if( nn > 1 )
		{
			if( !tab_print )
			{
				uprintf( "\a" );
				tab_print = 2;
			}
			else
			{
				uprintf( "\n" );
				prprompt( cmd );
			}
		}
		else
			uprintf( "\a" );
		o_pos = ed_pos + PS1len;
	}
	else if( be == 4 )
	{
		if( !ed_pos )
		{
			uprintf( "logout\n" );
			return( "logout" );
		}
		else
			uprintf( "\a" );
	}
	else if( be >= 32 && be < 256 )
	{
		int op;

		if( ed_pos == sizeof( cmd ) )
			return( NULL );

		memmove( cmd + ed_pos + 1, cmd + ed_pos, sizeof( cmd ) - ed_pos - 1 );
		posstr( &o_pos, ed_pos + PS1len );
		cmd[ed_pos] = be;
		op = uprintf( "%s", cmd + ed_pos++ );
		o_pos += op;
		if( op && !(o_pos % LINE_LEN) )
			uprintf( "\n" );
	}

	posstr( &o_pos, ed_pos + PS1len );
	fflush( stdout );
	return( NULL );
}

static void
sigcont(int s)
{
/*	int o_pos;*/

	con_reset(s);

/*	o_pos = prprompt(cmd);
	posstr(&o_pos, ed_pos + PS1len);
	fflush(stdout);*/
	signal(SIGCONT, sigcont);
}

void
addcmd(struct module_type *m, char *cmd,
       void (*handler)(int argc, char *argv[]), char *help)
{
	struct cmd_type *ncmd, *cur, *prev;

	ncmd = malloc( sizeof( struct cmd_type ) );
	memset( ncmd, 0, sizeof( struct cmd_type ) );
	ncmd->name = strdup(cmd);
	ncmd->handler = handler;
	ncmd->help = strdup(help);
	ncmd->mid = m;

	prev = &firstcmd;
	for(cur = firstcmd.next; cur; prev = cur, cur = cur->next)
		if(strcmp(cur->name, cmd) >= 0)
			break;

	ncmd->next = cur;
	prev->next = ncmd;
}

void
delcmd(struct module_type *m, char *cmd,
       void (*handler)(int argc, char *argv[]))
{
	struct cmd_type *cur, *prev;

	prev = &firstcmd;
	for( cur = firstcmd.next; cur; prev = cur, cur = cur->next )
	{
		if( cur->handler == handler && !strcmp( cmd, cur->name ) )
		{
			prev->next = cur->next;
			free(cur->name);
			free(cur->help);
			free(cur);
			return;
		}
	}
}

void
delcmd_mod(struct module_type *m)
{
	struct cmd_type *cur, *prev;

	prev = &firstcmd;
	for(cur = firstcmd.next; cur; prev = cur, cur = cur->next)
	{
		if(cur->mid == m)
		{
			if(VN("log-autodel"))
				logprintf(LOG_NOTICE, "delcmd: %s\n", cur->name);
			prev->next = cur->next;
			free(cur->name);
			free(cur->help);
			free(cur);
		}
	}
}

static char *
v_rand_get(struct variable_type *c)
{
	sprintf(c->val_str, "%i", (int)(65536.0 * rand() / (RAND_MAX + 1.0)));
	return c->val_str;
}

void
cmd_init(void)
{
	memset(PrevCmd, 0, sizeof(PrevCmd));
	signal(SIGCONT, sigcont);

	addsock(NULL, S_READ, 0, getUser_wrapper, NULL);
	add_bi_str(NULL, "PS1", &PS1);
	add_bi_num(NULL, "comments", &comments);
	add_bi_pr(NULL, "RANDOM", v_null_set, v_rand_get);
	set_var("HOSTNAME", "ana"VER);

	PS1 = strdup("$USER@$HOSTNAME$ ");
	comments = 1;

	addcmd(NULL, "clear",		Uclear,		"Usage: clear\n  Clear screen.\n");
	addcmd(NULL, "echo",		Uecho,		"Usage: echo [-n] [string]\n  Display string.\n");
	addcmd(NULL, "exec",		Uexec,		"Usage: exec file\n  Execute file.\n");
	addcmd(NULL, "help",		Uhelp,		"Usage: help [command]\n  Show info about command.\n");
	addcmd(NULL, "reset",		Ureset,		"Usage: reset\n  Reset terminal.\n");
	addcmd(NULL, "set",		Uset,		"Usage: set [variable value]\n  Set/show variables.\n");
	addcmd(NULL, "slexec",		Uslexec,	"Usage: slexec file time\n  Slowly execute file, one line in every 'time' secs.\n");
	addcmd(NULL, "unset",		Uunset,		"Usage: unset [variable1 [variable2 ...]]\n  Delete variables.\n");
	addcmd(NULL, "ver",		Uver,		"Usage: ver\n  Show version.\n");
	addcmd(NULL, "xecho",		Uxecho,		"Internal command\n");
	addcmd(NULL, "quit",		Uquit,		"Usage: quit\n  Quit.\n");
	addcmd(NULL, "whoami",		Uwhoami,	"Usage: whoami\n  Tells who you are.\n");
	addcmd(NULL, "restart",		Urestart,	"Usage: restart\n  Restarts program.\n");
}
