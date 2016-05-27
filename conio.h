/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2002.
 *
 */
#ifndef __CONIO_H
#define __CONIO_H

#define KEY_ENTER	10
#define KEY_CTRL_L	12
#define KEY_BACKSPACE	127

#define KEY_UP		1024
#define KEY_DOWN	1025
#define KEY_LEFT	1026
#define KEY_RIGHT	1027
#define KEY_ESC		1028
#define KEY_DEL		1029
#define KEY_HOME	1030
#define KEY_END		1031

#define KEY_F0		1032
#define KEY_F(n)	(KEY_F0 + (n))

#define BLACK		"\033[0;30m"
#define BLUE		"\033[0;34m"
#define GREEN		"\033[0;32m"
#define CYAN		"\033[0;36m"
#define RED		"\033[0;31m"
#define MAGENTA		"\033[0;35m"
#define BROWN		"\033[0;33m"
#define LG		"\033[0;37m"

#define DG		"\033[1;30m"
#define LBLUE		"\033[1;34m"
#define LGREEN		"\033[1;32m"
#define LCYAN		"\033[1;36m"
#define LRED		"\033[1;31m"
#define LMAGENTA	"\033[1;35m"
#define YELLOW		"\033[1;33m"
#define WHITE		"\033[1;37m"

#define BOLD		"\033[1m"
#define UNBOLD		"\033[22m"
#define REVERSE		"\033[7m"
#define UNREVERSE	"\033[27m"
#define UNDERLINE	"\033[4m"
#define UNUNDERLINE	"\033[24m"

#define NORMAL		"\033[m"

#ifdef __CONIO_C
static int conio_gchar( void );
#endif

void con_reset( int x );
void console_init( void );
void console_deinit( void );
int kbhit( void );
int getch( void );


#endif
