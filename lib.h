#pragma once

#include <stdio.h>

enum {
	LEDTAG_DEV_NONE, LEDTAG_DEV_M1_8, LEDTAG_NDEV
};

enum {
	LEDTAG_ERR_NOINIT = 16700, /* initialization failed/missing initialization  */
	LEDTAG_ERR_INVALID, /* invalid argument */
	LEDTAG_ERR_OPEN, /* device could not be opened */
	LEDTAG_ERR_IO, /* io error */
	LEDTAG_ERR_MSGSPAN, /* invalid message span */
};

typedef struct ledtag_info ledtag_info_t;
typedef struct ledtag ledtag_t;

struct ledtag_info {
	const char *name;
	unsigned height;
	unsigned max_width;
	unsigned char_width; /* width of each message must be a multiple of char_width */
	unsigned display_chars; /* width of physical display in characters */
	unsigned max_chars; /* equals max_width / char_width */
	unsigned max_messages;
	unsigned max_speed;
	unsigned max_brightness;
	unsigned n_mode;
	const char **mode_name;
	unsigned n_option;
	const char **option_name;
};

int ledtag_init();
void ledtag_exit();

/* opens first device which has been found. */
ledtag_t *ledtag_open(
		const wchar_t *serial);

void ledtag_close(
		ledtag_t *self);

const ledtag_info_t *ledtag_info(
		ledtag_t *self);

int ledtag_setup(
		ledtag_t *self,
		unsigned brightness);

int ledtag_setup_message(
		ledtag_t *self,
		unsigned msg,
		unsigned bchar, /* begin character */
		unsigned nchar, /* end character */
		unsigned mode,
		unsigned speed,
		unsigned options); /* for each option to enable: 1 << option_id */

void ledtag_invert(
		ledtag_t *self);

void ledtag_clear(
		ledtag_t *self);

void ledtag_fill(
		ledtag_t *self);

int ledtag_set(
		ledtag_t *self,
		unsigned x, /* coords in pixels */
		unsigned y);

int ledtag_unset(
		ledtag_t *self,
		unsigned x,
		unsigned y);

/* check, whether messages are setup correctly */
int ledtag_validate(
		const ledtag_t *self);

int ledtag_flash(
		ledtag_t *self);

