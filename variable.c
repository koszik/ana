/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2005.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#define __USE_BSD
#include <string.h>

#include <main.h>
#include <variable.h>
#include <log.h>
#include <user.h>

static struct variable_type *var;

static char *
GET(struct variable_type *x)
{
	switch(x->type)
	{
		case VT_NORM: return x->value;
		case VT_STR: return *x->values;
		case VT_INT: sprintf(x->val_str, "%i", *x->valuei);
			return x->val_str;
		case VT_PR: return x->get(x); break;
		default: exc_raise(E_P, "get: type=%i", x->type);
			return "";
	}
}

char *
get_var_str(char *varname)
{
	int x;
	struct variable_type *cur;

	for(cur = var; cur; cur = cur->next)
	{
		if((x = strcmp(cur->name, varname)) == 0)
			return GET(cur);
		if(x > 0)
			break;
	}

	return "";
}

void
set_var_if_empty(char *varname, char* value)
{
	int x;
	struct variable_type *cur;

	for(cur = var; cur; cur = cur->next)
	{
		if((x = strcmp(cur->name, varname)) == 0)
			return;
		if(x > 0)
			break;
	}

	set_var(varname, value);
}

void
set_var(char *varname, char *value)
{
	int r;
	struct variable_type *nvar, *cur, *prev;

	r = 1;
	for(cur = var; cur; cur = cur->next)
		if((r = strcmp(cur->name, varname)) >= 0)
			break;

	if(r == 0)
	{
		switch(cur->type)
		{
			case VT_NORM:
				free(cur->value);
				cur->value = strdup(value);
				return;
			case VT_INT:
				*cur->valuei = atoi(value);
				return;
			case VT_STR:
				free(*cur->values);
				*cur->values = strdup(value);
				return;
			case VT_PR:
				cur->set(cur, value);
				return;
			default:
				exc_raise(E_P, "set: type=%i", cur->type);
				return;
		}
	}


	prev = NULL;
	for(cur = var; cur; prev = cur, cur = cur->next)
		if((r = strcmp(cur->name, varname)) > 0)
			break;

	nvar = malloc(sizeof(struct variable_type));
	memset(nvar, 0, sizeof(struct variable_type));

	nvar->name = strdup(varname);
	nvar->value = strdup(value);
	nvar->type = VT_NORM;

	if(prev)
		prev->next = nvar;
	else
		var = nvar;
	nvar->next = cur;
}

void
del_var(char *varname)
{
	struct variable_type *cur, *prev;

	prev = NULL;
	for(cur = var; cur; prev = cur, cur = cur->next)
		if(!strcmp(cur->name, varname))
		{
			if(prev)
				prev->next = cur->next;
			else
				var = cur->next;
			free(cur->name);
			free(cur->value);
			free(cur);
			return;
		}
}

void
list_variables(void)
{
	struct variable_type *cur;

	for(cur = var; cur; cur = cur->next)
		uprintf("%s='%s'\n", cur->name, GET(cur));
}

int
add_bi_str(struct module_type *m, char *name, char **ptr)
{
	int r;
	struct variable_type *nvar, *cur, *prev;

	r = 1;
	prev = NULL;
	for(cur = var; cur; prev = cur, cur = cur->next)
		if((r = strcmp(cur->name, name)) >= 0)
			break;

	if(r == 0 && cur->type != VT_NORM)
	{
		exc_raise(E_P, "builtin variable %s already exists! (t=%i)\n", name, cur->type);
		return ANA_ERR;
	}

	if(r == 0)
	{
		free(*ptr);
		*ptr = cur->value;
		cur->values = ptr;
		cur->value = NULL;
		cur->type = VT_STR;
		cur->mid = m;
		return ANA_OK;
	}

	nvar = malloc(sizeof(struct variable_type));
	memset(nvar, 0, sizeof(struct variable_type));

	nvar->name = strdup(name);
	nvar->values = ptr;
	nvar->type = VT_STR;
	nvar->mid = m;

	if(prev)
		prev->next = nvar;
	else
		var = nvar;
	nvar->next = cur;

	if(*ptr == NULL)
		*ptr = strdup("");
	return ANA_OK;
}

int
add_bi_num(struct module_type *m, char *name, int *ptr)
{
	int r;
	struct variable_type *nvar, *cur, *prev;

	r = 1;
	prev = NULL;
	for(cur = var; cur; prev = cur, cur = cur->next)
		if((r = strcmp(cur->name, name)) >= 0)
			break;

	if(r == 0 && cur->type != VT_NORM)
	{
		exc_raise(E_P, "builtin variable %s already exists! (t=%i)\n", name, cur->type);
		return ANA_ERR;
	}

	if(r == 0)
	{
		*ptr = atoi(cur->value);
		free(cur->value);
		cur->value = NULL;
		cur->valuei = ptr;
		cur->type = VT_INT;
		cur->mid = m;
		return ANA_OK;
	}

	nvar = malloc(sizeof(struct variable_type));
	memset(nvar, 0, sizeof(struct variable_type));

	nvar->name = strdup(name);
	nvar->valuei = ptr;
	nvar->type = VT_INT;
	nvar->mid = m;

	if(prev)
		prev->next = nvar;
	else
		var = nvar;
	nvar->next = cur;

	return ANA_OK;
}

int
add_bi_pr(struct module_type *m, char *name,
	  void (*set)(struct variable_type *c, char *val),
	  char *(*get)(struct variable_type *c))
{
	int r;
	struct variable_type *nvar, *cur, *prev;

	r = 1;
	prev = NULL;
	for(cur = var; cur; prev = cur, cur = cur->next)
		if((r = strcmp(cur->name, name)) >= 0)
			break;

	if(r == 0 && cur->type != VT_NORM)
	{
		exc_raise(E_P, "builtin variable %s already exists! (t=%i)\n", name, cur->type);
		return ANA_ERR;
	}

	if(r == 0)
	{
		set(cur, cur->value);
		free(cur->value);
		cur->value = NULL;
		cur->get = get;
		cur->set = set;
		cur->type = VT_PR;
		cur->mid = m;
		return ANA_OK;
	}

	nvar = malloc(sizeof(struct variable_type));
	memset(nvar, 0, sizeof(struct variable_type));

	nvar->name = strdup(name);
	nvar->set = set;
	nvar->get = get;
	nvar->type = VT_PR;
	nvar->mid = m;

	if(prev)
		prev->next = nvar;
	else
		var = nvar;
	nvar->next = cur;

	return ANA_OK;
}

void
del_var_mod(struct module_type *m)
{
	struct variable_type *cur;

	for(cur = var; cur; cur = cur->next)
		if(cur->mid == m)
		{
			if(VN("log-autodel"))
				logprintf(LOG_NOTICE, "del_var_mod: %s\n", cur->name);
			cur->value = GET(cur);
			if(cur->type == VT_PR)
				cur->value = strdup(cur->value);
			cur->type = VT_NORM;
			cur->mid = NULL;
		}
}

void
del_var_int(struct module_type *m, char *varname)
{
	struct variable_type *cur;

	for(cur = var; cur; cur = cur->next)
		if(!strcmp(cur->name, varname))
		{
			if(cur->type != VT_INT)
				return;
			sprintf(cur->val_str, "%i", *cur->valuei);
			cur->value = strdup(cur->val_str);
			cur->type = VT_NORM;
			cur->mid = NULL;
			return;
		}
}

void
del_var_str(struct module_type *m, char *varname)
{
	struct variable_type *cur;

	for(cur = var; cur; cur = cur->next)
		if(!strcmp(cur->name, varname))
		{
			if(cur->type != VT_STR)
				return;
			cur->value = *cur->values;
			cur->type = VT_NORM;
			cur->mid = NULL;
			return;
		}
}

void
del_var_pr(struct module_type *m, char *varname)
{
	struct variable_type *cur;

	for(cur = var; cur; cur = cur->next)
		if(!strcmp(cur->name, varname))
		{
			if(cur->type != VT_PR)
				return;
			cur->value = strdup(cur->get(cur));
			cur->type = VT_NORM;
			cur->mid = NULL;
			return;
		}
}

void
v_null_set(struct variable_type *c, char *v)
{
}
