/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2002, 2005, 2016.
 *
 */
#ifndef __SOCKET_H
#define __SOCKET_H

int sockprintf(int sd, char *fmt, ...);
int listen_on_af(int port, int af);
#define listen_on(port) listen_on_af(port, AF_INET)

#endif
