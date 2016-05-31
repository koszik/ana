/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2002, 2005, 2016.
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include <main.h>
#include <socket.h>
#include <log.h>

int
sockprintf(int sd, char *fmt, ...)
{
	char *buf;
	va_list lst;
	int len, ret;

	va_start(lst, fmt);
	if(!(buf = malloc(2048)))
	{
		exc_raise(E_P, "can't malloc 2048 bytes");
		return -1;
	}
	len = vsnprintf(buf, 2048, fmt, lst);
	if(len > 2048)
	{
		if(!(buf = realloc(buf, len + 1)))
		{
		    exc_raise(E_P, "can't realloc %i bytes", len + 1);
		    return -1;
		}
		vsnprintf(buf, len, fmt, lst);
	}
	else if(len == -1)
		exc_raise(E_P, "len = -1!\n");
	ret = write(sd, buf, len);
	free(buf);
	va_end(lst);
	return ret;
}

int
listen_on_af(int port, int af)
{
	int sd;
	int one;
	struct sockaddr_in host_addr;


	sd = socket(af, SOCK_STREAM, IPPROTO_IP);
	one = 1;
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	host_addr.sin_family = af;
	host_addr.sin_addr.s_addr = 0;
	host_addr.sin_port = htons(port); 

	if((bind(sd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr_in))) == -1)
	{
		exc_raise(E_P, "bind(%i, %s:%i) = -1 (%s)\n",
		    sd, inet_ntoa(*(struct in_addr*)&host_addr.sin_addr.s_addr),
		    port, strerror(errno));
		return -1;
	}

	listen(sd, 128);
	return sd;
}
