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
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

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
listen_on_af_type(int port, int af, int type)
{
	int sd;
	int one;
	struct sockaddr_in host_addr;


	if((sd = socket(af, type, IPPROTO_IP)) < 0)
	{
		exc_raise(E_P, "socket(%i, %i, IPPROTO_IP) = %i (%s)", af, type, sd, strerror(errno));
		return -1;
	}
	one = 1;
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	host_addr.sin_family = af;
	host_addr.sin_addr.s_addr = 0;
	host_addr.sin_port = htons(port); 

	if((bind(sd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr_in))) == -1)
	{
		exc_raise(E_P, "bind(%i, %s:%i) = -1 (%s)",
		    sd, inet_ntoa(*(struct in_addr*)&host_addr.sin_addr.s_addr),
		    port, strerror(errno));
		close(sd);
		return -1;
	}

	if(type == SOCK_STREAM)
	    listen(sd, 128);
	return sd;
}

int
host_connect(int type, char *host, int port)
{
	struct hostent *he;
	struct sockaddr_in host_addr;
	int sd;

	memset(&host_addr, 0, sizeof(host_addr));
	if((he = gethostbyname(host)) == NULL) {
		exc_raise(E_P, "gethostbyname(%s): %s", host, hstrerror(h_errno));
		return -1;
	}
	host_addr.sin_family = AF_INET;
	memcpy(&host_addr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
	host_addr.sin_port = htons(port);
	sd = socket(AF_INET, type, IPPROTO_IP);
	fcntl(sd, F_SETFL, O_NONBLOCK);
	if((connect(sd, (struct sockaddr *)&host_addr,
	      sizeof(struct sockaddr_in))) == -1 && errno != EINPROGRESS) {
		close(sd);
		exc_raise(E_P, "connect(%i, %s:%i, %i): %s",
			sd, host, port, sizeof(struct sockaddr_in), strerror(errno));
		return -1;
	}
	return sd;
}
