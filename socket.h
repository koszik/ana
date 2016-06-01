/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2002, 2005, 2016.
 *
 */
#ifndef __SOCKET_H
#define __SOCKET_H

int sockprintf(int sd, char *fmt, ...);
int listen_on_af_type(int port, int af, int type);
int host_connect(int type, char *host, int port);
#define listen_on(port) listen_on_af_type(port, AF_INET, SOCK_STREAM)
#define udp_listen_on(port) listen_on_af_type(port, AF_INET, SOCK_DGRAM)
#define tcp6_listen_on(port) listen_on_af_type(port, AF_INET6, SOCK_STREAM)
#define udp6_listen_on(port) listen_on_af_type(port, AF_INET6, SOCK_DGRAM)

#endif
