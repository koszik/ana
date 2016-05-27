/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2004, 2006.
 *
 */
#include <stdio.h>
#define __USE_POSIX
#define __USE_BSD
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define __MEM_C
#include <main.h>
#include <module.h>
#include <log.h>
#include <mem.h>

static void
mem_add(const char *f, int l, struct module_type *m, size_t size, void *ptr)
{
	int i;

	if(!size || !ptr)
		return;
	i = m->malloc_chunks++;
	m->macct = realloc(m->macct, sizeof(*m->macct) * m->malloc_chunks);
	m->macct[i].ptr = ptr;
	m->macct[i].size = size;
	m->malloc_bytes += size;
	m->macct[i].loc = malloc(strlen(f) + 20);
	sprintf(m->macct[i].loc, "%s():%i", f, l);
}

static int
mem_del(const char *f, int l, struct module_type *m, void *ptr)
{
	int i;

	if(!ptr)
		return 1;
	for(i = 0; i < m->malloc_chunks; i++)
		if(m->macct[i].ptr == ptr)
		{
			m->malloc_bytes -= m->macct[i].size;
			free(m->macct[i].loc);
			if(i != --m->malloc_chunks)
				memcpy(&m->macct[i], &m->macct[m->malloc_chunks], sizeof(m->macct[i]));
			m->macct = realloc(m->macct, sizeof(*m->macct) * m->malloc_chunks);
			return 1;
		}
	logprintf(LOG_CRIT, "mem_del: %s():%i: chunk %p not found!\n", f, l, ptr);
	kill(getpid(), SIGURG);
	return 1; /* hooray for malloc_check */
}

void *
ana_malloc(const char *f, int l, struct module_type *m, size_t size)
{
	void *ret;

	ret = malloc(size);
	mem_add(f, l, m, size, ret);
	return ret;
}

void
ana_free(const char *f, int l, struct module_type *m, void *ptr)
{
	if(!ptr)
		return;
	if(mem_del(f, l, m, ptr))
		free(ptr);
}

void *
ana_realloc(const char *f, int l, struct module_type *m, void *ptr, size_t size)
{
	void *ret;

	mem_del(f, l, m, ptr);
	ret = realloc(ptr, size);
	mem_add(f, l, m, size, ret);
	return ret;
}

char *
ana_strdup(const char *f, int l, struct module_type *m, const char *str)
{
	char *ret;

	ret = strdup(str);
	mem_add(f, l, m, strlen(ret) + 1, ret);
	return ret;
}

void *
ana_calloc(const char *f, int l, struct module_type *m, size_t nmemb, size_t size)
{
	char *ret;

	ret = calloc(nmemb, size);
	mem_add(f, l, m, nmemb * size, ret);
	return ret;
}
