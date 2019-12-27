#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "arg.h"

static int find_long(
		const arg_list_t *list,
		const char *key,
		size_t keylen)
{
	int i;
	for(i = 0; list[i].opt.s || list[i].opt.l; i++)
		if(list[i].opt.l && strlen(list[i].opt.l) == keylen && !strncmp(list[i].opt.l, key, keylen))
			return i;
	errno = ENOENT;
	return -1;
}

static int find_short(
		const arg_list_t *list,
		char key)
{
	int i;
	for(i = 0; list[i].opt.s || list[i].opt.l; i++)
		if(list[i].opt.s == key)
			return i;
	errno = ENOENT;
	return -1;
}

static int find_end(
		const arg_list_t *list)
{
	int i;
	for(i = 0; list[i].opt.s || list[i].opt.l; i++);
	return i;
}

static int parse_value(
		int type,
		const char *value,
		arg_value_t *v)
{
	switch(type) {
		case ARG_STRING:
			v->s = value;
			return 0;
		case ARG_BOOL:
			if(!value)
				v->b = true;
			else if((value[0] == 'y' || value[0] == 'Y') && !value[1])
				v->b = true;
			else if((value[0] == 'n' || value[0] == 'N') && !value[1])
				v->b = false;
			else {
				errno = EINVAL;
				return -1;
			}
			return 0;
		case ARG_INT:
		{
			char *end;
			errno = 0;
			v->i = strtoll(value, &end, 0);
			if(end == value) {
				errno = EINVAL;
				return -1;
			}
			else if(errno == ERANGE) {
				errno = EOVERFLOW;
				return -1;
			}
			return 0;
		}
		case ARG_FLOAT:
		{
			v->f = atof(value);
			return 0;
		}
		default:
			errno = EINVAL;
			return -1;
	}
}

static int handle_arg(
		const arg_list_t *list,
		int idx,
		const char *value,
		arg_handle_fn handle,
		void *user)
{
	arg_value_t v;
	memset(&v, 0, sizeof(v));
	if(list[idx].type == ARG_FLAG)
		v.b = true;
	else if(parse_value(list[idx].type, value, &v) < 0)
		return -1;
	if(list[idx].value)
		*list[idx].value = v;
	if(handle)
		return handle(idx, v, user);
	else
		return 0;
}

int arg_parse(
		int argi,
		int argc,
		char *argv[],
		const arg_list_t *list,
		arg_handle_fn handle,
		void *user)
{
	int sep;
	int idx;
	int i;
	for(; argi < argc; argi++) {
		const char *arg = argv[argi];
		arg_value_t v;
		int ret = 0;
		memset(&v, 0, sizeof(v));

		if(arg[0] == '-') {
			if(arg[1] == '-') {
				for(sep = 2; arg[sep]; sep++)
					if(arg[sep] == '=')
						break;
				idx = find_long(list, arg + 2, sep - 2);
				if(idx < 0)
					ret = -1;
				else if(arg[sep]) {
					if(list[idx].type == ARG_FLAG) {
						errno = EINVAL;
						ret = -1;
					}
					else
						ret = handle_arg(list, idx, arg + sep + 1, handle, user);
				}
				else {
					if(list[idx].type == ARG_FLAG)
						ret = handle_arg(list, idx, NULL, handle, user);
					else {
						errno = EINVAL;
						ret = -1;
					}
				}
			}
			else {
				for(i = 1; arg[i] && !ret; i++) {
					idx = find_short(list, arg[i]);
					if(idx < 0)
						ret = -1;
					else if(list[idx].type == ARG_FLAG)
						ret = handle_arg(list, idx, NULL, handle, user);
					else if(arg[i + 1] || argi + 1 == argc || argv[argi + 1][0] == '-') {
						errno = EINVAL;
						ret = -1;
					}
					else
						ret = handle_arg(list, idx, argv[++argi], handle, user);
				}
			}
		}
		else
			ret = handle_arg(list, find_end(list), argv[argi], handle, user);
		if(ret < 0)
			return -argi;
		else if(ret > 0)
			return argi;
	}
	return argi;
}

void arg_fusage(
		FILE *file,
		const arg_list_t *list,
		int keyw,
		int maxw)
{
	fprintf(file, "Usage:\n");
}

void arg_dump(
		const arg_list_t *list)
{
	for(;;) {
		if(list->opt.l)
			printf("%s = ", list->opt.l);
		else if(list->opt.s)
			printf("%c = ", list->opt.s);
		else
			printf("<free> = ");
		if(list->value) {
			switch(list->type) {
				case ARG_FLAG:
				case ARG_BOOL:
					printf("%c", list->value->b ? 'y' : 'n');
					break;
				case ARG_STRING:
					printf("%s", list->value->s);
					break;
				case ARG_INT:
					printf("%lld", list->value->i);
					break;
				case ARG_FLOAT:
					printf("%f", list->value->f);
					break;
				default:
					printf("<invalid type>");
					break;
			}
		}
		else
			printf("<unknown>");
		printf("\n");
		if(!list->opt.s && !list->opt.l)
			break;
		else
			list++;
	}
}

