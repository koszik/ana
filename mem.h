/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2004, 2006.
 *
 */
#ifndef __MEM_H
#define __MEM_H

struct macct_type
{
	void	*ptr;
	size_t	 size;
	char	*loc;
};

void *ana_malloc(const char *f, int l, TYPE m, size_t size);
void ana_free(const char *f, int l, TYPE m, void *ptr);
void *ana_realloc(const char *f, int l, TYPE m, void *ptr, size_t size);
char *ana_strdup(const char *f, int l, TYPE m, const char *str);
void *ana_calloc(const char *f, int l, TYPE m, size_t nmemb, size_t size);

#if defined(__MODULE__) && !defined(__NO_MEM_DEBUG__)
#	define malloc(x) ana_malloc(__FUNCTION__,__LINE__,module_id,x)
#	define free(x) ana_free(__FUNCTION__,__LINE__,module_id,x)
#	define realloc(x,y) ana_realloc(__FUNCTION__,__LINE__,module_id,x,y)
#	define strdup(x) ana_strdup(__FUNCTION__,__LINE__,module_id,x)
#	define calloc(x,y) ana_calloc(__FUNCTION__,__LINE__,module_id,x,y)
#endif

#endif
