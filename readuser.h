/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2002, 2006.
 *
 */
#ifndef __READUSER_H
#define __READUSER_H

#ifdef __READUSER_C
static void Uhelp   ( int argc, char *argv[] );
static void Uset    ( int argc, char *argv[] );
static void Uunset  ( int argc, char *argv[] );


static void getUser_wrapper( int dummy, void *dummy2 );
static void HistCmd( int dir );
static void posstr( int *cur, int newp );
static int prprompt( const char *cmd );
static char *getuser( void );
static void sigcont( int s );
#endif 

void cmd_init(void);
void addcmd(TYPE m, char *cmd, void (*handler)(int argc, char *argv[]), char *help);
void delcmd(TYPE m, char *cmd, void (*handler)(int argc, char *argv[]));
#if defined(__MODULE__)
#	define addcmd(c, h, e) addcmd(module_id, c, h, e)
#	define delcmd(c, h) delcmd(module_id, c, h)
#endif
void execf(char *file);
void delcmd_mod(TYPE m);

void getUser(const char *cmd);
void pr_pr(void);


#endif
