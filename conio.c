/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2004.
 *
 */
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/types.h>   
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#define __CONIO_C
#include <conio.h>

static struct termios conio_old_tio;

void
con_reset(int x)
{
	struct termios tio;

	if(tcgetattr(0, &tio) < 0)
	{
		printf("%scan't get terminal attributes\n", x? "": "reset: ");
		return;
	}

	tio.c_lflag &= ~(ICANON | ECHO /*| ISIG*/);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	if(tcsetattr(0, TCSANOW, &tio) < 0)
	{
		printf("%scan't set terminal attributes\n", x? "": "reset: ");
		return;
	}
}

void
console_init(void)
{
	struct termios tio;

	if(tcgetattr(0, &tio) < 0)
	{
		fprintf(stderr, "Can't get terminal attributes\n");
		exit(1);
	}

	conio_old_tio = tio;
	tio.c_lflag &= ~(ICANON | ECHO /*| ISIG*/);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	if(tcsetattr(0, TCSANOW, &tio) < 0)
	{
		fprintf(stderr, "Can't set terminal attributes\n");
		exit(1);
	}
	atexit(console_deinit);
}

void
console_deinit(void)
{
	tcsetattr(0, TCSAFLUSH, &conio_old_tio);
}

int
kbhit(void)
{
	fd_set fds;
	struct timeval tmv;

	memset(&fds, 0, sizeof(fds)); /* not really necessary since we're only checking the first fd anyway */
	FD_SET(0, &fds);
	memset(&tmv, 0, sizeof(tmv));
	if(select(1, &fds, NULL, NULL, &tmv) != 0)
		return 1;
	return 0;
}

static int
conio_gchar(void)
{
	unsigned char z;

	read(0, &z, 1);
	return z;
}
		

int
getch(void)
{
	int a, a1, a2;

	a2 = conio_gchar();

	if(a2 == 27)
	{
		a1 = conio_gchar();
		if( a1 == '[' )
		{
			a = conio_gchar();
			if(a == 'A')
				a2 = KEY_UP;
			else if(a == 'B')
				a2 = KEY_DOWN;
			else if(a == 'C')
				a2 = KEY_RIGHT;
			else if(a == 'D')
				a2 = KEY_LEFT;
			else if(a == '[')
			{
				a = conio_gchar();
				if(a >= 'A' && a <= 'Z')
					a2 = KEY_F0 + a - 'A' + 1;
			}
			else if(a == '3')
			{
				if(conio_gchar() == '~')
					a2 = KEY_DEL;
			}
			/* openbsd = 7/8, tobbi = 1/4 */
			else if(a == '7' || a == '1')
			{
				if(conio_gchar() == '~')
					a2 = KEY_HOME;
			}
			else if(a == '8' || a == '4')
			{
				if(conio_gchar() == '~')
					a2 = KEY_END;
			}
		}
		else if(a1 == 27)
			a2 = KEY_ESC;
		else
		{
			a2 = a1;
		}
	}
	return a2;
}

