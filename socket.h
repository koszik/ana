/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2002, 2005.
 *
 */
#ifndef __SOCKET_H
#define __SOCKET_H

int sockprintf(int sd, char *fmt, ...);
int listen_on(int port);

#endif
