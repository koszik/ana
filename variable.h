/*
 * ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2006.
 *
 */
#ifndef __VARIABLE_H
#define __VARIABLE_H

#define V(x)	get_var_str(x)
#define VN(x)	atoi(get_var_str(x))

#define VT_NORM		0
#define VT_STR		1
#define	VT_INT		2
#define VT_PR		3

struct variable_type
{
	struct variable_type    *next;
	int			 type;
	char			*name;
	char			*value;
	char		       **values;
	int			*valuei;
	char			 val_str[20];
	char		      *(*get)(struct variable_type *c);
	void		       (*set)(struct variable_type *c, char *val);
	struct module_type	*mid;
};

char *get_var_str(char *varname);
void set_var_if_empty(char *varname, char* value);
void set_var(char *varname, char *value);
void del_var(char *varname);
void list_variables(void);
int add_bi_str(TYPE m, char *name, char **ptr);
int add_bi_num(TYPE m, char *name, int *ptr);
int add_bi_pr(TYPE m, char *name, void (*set)(struct variable_type *c, char *val), char *(*get)(struct variable_type *c));
void del_var_int(TYPE m, char *varname);
void del_var_str(TYPE m, char *varname);
void del_var_pr(TYPE m, char *varname);
void v_null_set(struct variable_type *c, char *v);

#ifdef __MODULE__
#	define	add_bi_str(x, y) add_bi_str(module_id, x, y)
#	define	add_bi_num(x, y) add_bi_num(module_id, x, y)
#	define	add_bi_pr(x, y) add_bi_pr(module_id, x, y)
#	define	del_var_int(v) del_var_int(module_id, v)
#	define	del_var_str(v) del_var_str(module_id, v)
#	define	del_var_pr(v) del_var_pr(module_id, v)
#else
void del_var_mod(struct module_type *m);
#endif

#endif
