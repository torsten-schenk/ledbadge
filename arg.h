#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct arg_list arg_list_t;
typedef union arg_value arg_value_t;

/* return: 0: continue; 1: stop parsing; -1: error */
typedef int (*arg_handle_fn)(int id, arg_value_t value, void *user);

enum {
	ARG_FLAG, ARG_STRING, ARG_BOOL, ARG_INT, ARG_FLOAT
};

union arg_value {
	const char *s;
	bool b;
	long long i;
	float f;
};

struct arg_list {
	struct {
		char s;
		const char *l;
	} opt;
	int type;
	arg_value_t *value;
	const char *description;
};

/* return:
 * > 0: no error, stopped parsing at returned value
 * < 0: error (code in errno), stopped parsing at -(returned value) */
int arg_parse(
		int argi,
		int argc,
		char *argv[],
		const arg_list_t *list,
		arg_handle_fn handle,
		void *user);

void arg_dump(
		const arg_list_t *list);

void arg_fusage(
		FILE *file,
		const arg_list_t *list,
		int keyw,
		int maxw);

static inline void arg_usage(
		const arg_list_t *list,
		int keyw,
		int maxw)
{
	arg_fusage(stdout, list, keyw, maxw);
}

#ifdef __cplusplus
}
#endif


