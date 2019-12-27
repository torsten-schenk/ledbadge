#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <hidapi/hidapi.h>

#include "lib.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

/* see also: https://github.com/HappyCodingRobot/USB_LED_Badge/blob/master/badge.c */

enum {
	VENDOR_ID = 0x0416, PRODUCT_ID = 0x5020
};

enum {
	MAX_CHARS = 0x200, CHAR_WIDTH = 8, DISPLAY_CHARS = 6, MAX_WIDTH = MAX_CHARS * CHAR_WIDTH, HEIGHT = 11, MAX_MESSAGES = 8, MAX_BRIGHTNESS = 3, MAX_SPEED = 7
};

enum {
	OPTION_FLASH = 0x01, OPTION_BORDER = 0x02, N_OPTION = 2
};

enum {
	MODE_LEFT, MODE_RIGHT, MODE_UP, MODE_DOWN, MODE_FREEZE, MODE_ANIM, MODE_PILING, MODE_SPLITE, MODE_LASER, MODE_SMOOTH, MODE_ROTATE, N_MODE
};

enum {
	PACKET_SIZE = 0x40
};

static const char *mode_name[N_MODE] = {
	"left", "right", "up", "down", "freeze", "animation", "piling", "splite", "laser", "smooth", "rotate"
};

static const char *option_name[N_OPTION] = {
	"flash", "border"
};

static const ledtag_info_t info = {
	.name = "M1-8",
	.height = HEIGHT,
	.char_width = CHAR_WIDTH,
	.display_chars = DISPLAY_CHARS, 
	.max_width = MAX_WIDTH,
	.max_chars = MAX_CHARS,
	.max_messages = MAX_MESSAGES,
	.max_brightness = MAX_BRIGHTNESS,
	.max_speed = MAX_SPEED,
	.n_mode = N_MODE,
	.mode_name = mode_name,
	.n_option = N_OPTION,
	.option_name = option_name
};

typedef struct message message_t;

struct message {
	uint8_t options;
	uint8_t mode;
	uint8_t speed;
	uint16_t bchar; /* first char in bitmap */
	uint16_t nchar;
};

struct ledtag {
	hid_device *handle;

	uint8_t brightness;
	message_t message[MAX_MESSAGES];
	uint32_t time; /* unknown value, which is changing by time */
	uint8_t data[MAX_CHARS * HEIGHT];
};

static int error = LEDTAG_ERR_NOINIT;

int ledtag_init()
{
	if(!error)
		return 0;
	else if(hid_init() < 0) {
		errno = LEDTAG_ERR_NOINIT;
		return -1;
	}
	else {
		error = 0;
		return 0;
	}
}

void ledtag_exit()
{
	hid_exit();
}

ledtag_t *ledtag_open(
		const wchar_t *serial)
{
	ledtag_t *self;

	if(error) {
		errno = error;
		return NULL;
	}
	self = calloc(1, sizeof(ledtag_t));
	if(!self)
		goto error_0;
	self->handle = hid_open(VENDOR_ID, PRODUCT_ID, serial);
	if(!self->handle) {
		errno = ENODEV;
		goto error_1;
	}

	return self;

error_1:
	free(self);
error_0:
	return NULL;
}

void ledtag_close(
		ledtag_t *self)
{
	hid_close(self->handle);
	free(self);
}

const ledtag_info_t *ledtag_info(
		ledtag_t *self)
{
	return &info;
}

int ledtag_setup(
		ledtag_t *self,
		unsigned brightness)
{
	if(brightness > MAX_BRIGHTNESS) {
		errno = LEDTAG_ERR_INVALID;
		return -1;
	}
	self->brightness = brightness;
	return 0;
}

int ledtag_setup_message(
		ledtag_t *self,
		unsigned msg,
		unsigned bchar,
		unsigned nchar,
		unsigned mode,
		unsigned speed,
		unsigned options)
{
	if(msg >= MAX_MESSAGES) {
		errno = LEDTAG_ERR_INVALID;
		return -1;
	}
	else if(bchar >= MAX_CHARS || bchar + nchar > MAX_CHARS) {
		errno = LEDTAG_ERR_INVALID;
		return -1;
	}
	else if(mode >= N_MODE) {
		errno = LEDTAG_ERR_INVALID;
		return -1;
	}
	self->message[msg].bchar = bchar;
	self->message[msg].nchar = nchar;
	self->message[msg].mode = mode;
	self->message[msg].options = options;
	self->message[msg].speed = speed;
	return 0;
}

void ledtag_invert(
		ledtag_t *self)
{
	unsigned i;
	for(i = 0; i < MAX_WIDTH * HEIGHT; i++)
		self->data[i] ^= 0xff;
}

void ledtag_clear(
		ledtag_t *self)
{
	memset(self->data, 0x00, MAX_WIDTH * HEIGHT);
}

void ledtag_fill(
		ledtag_t *self)
{
	memset(self->data, 0xff, MAX_WIDTH * HEIGHT);
}

int ledtag_set(
		ledtag_t *self,
		unsigned x,
		unsigned y)
{
	unsigned index;
	if(x >= MAX_WIDTH || y >= HEIGHT) {
		errno = LEDTAG_ERR_INVALID;
		return -1;
	}
	index = x / CHAR_WIDTH * HEIGHT + y;
	self->data[index] |= 0x80 >> (x % CHAR_WIDTH);
	return 0;
}

int ledtag_unset(
		ledtag_t *self,
		unsigned x,
		unsigned y)
{
	unsigned index;
	if(x >= MAX_WIDTH || y >= HEIGHT) {
		errno = LEDTAG_ERR_INVALID;
		return -1;
	}
	index = x / CHAR_WIDTH * HEIGHT + y;
	self->data[index] &= 0xff ^ (0x80 >> (x % CHAR_WIDTH));
	return 0;
}

int ledtag_validate(
		const ledtag_t *self)
{
	unsigned i;
	unsigned cur = 0;
	for(i = 0; i < MAX_MESSAGES; i++) {
		const message_t *msg = self->message + i;
		if(msg->nchar && msg->bchar != cur) {
			errno = LEDTAG_ERR_MSGSPAN;
			return -1;
		}
		cur += msg->nchar;
	}
	return 0;
}

int ledtag_flash(
		ledtag_t *self)
{
	uint8_t report[PACKET_SIZE + 1];
	uint8_t *packet = report + 1;
	uint16_t u16;
	unsigned i;
	unsigned k;
	unsigned nchar = 0; /* total number of chars to send to device */
	unsigned nbytes; /* total number of bitmap bytes to send */
	uint8_t *si; /* current source data */

	memset(report, 0, sizeof(report));

	packet[0x00] = 'w';
	packet[0x01] = 'a';
	packet[0x02] = 'n';
	packet[0x03] = 'g';
	packet[0x05] = MAX_BRIGHTNESS - self->brightness; /* invert brightness, since maximum value means darkest */
	for(i = 0; i < MAX_MESSAGES; i++) {
		const message_t *msg = self->message + i;
		/* options start at offset 0x06. each option has bit 'message index' set, if option is activated for given message. */
		for(k = 0; k < N_OPTION; k++)
			if(msg->options & (1 << k))
				packet[0x06 + k] |= 1 << i;
		
		/* speed and mode start at offset 0x08 */
		packet[0x08 + i] = (msg->speed << 4) | msg->mode;

		/* number of characters start at offset 0x10 */
		u16 = htobe16(msg->nchar);
		memcpy(packet + 0x10 + i * sizeof(u16), &u16, sizeof(u16));

		if(msg->nchar && msg->bchar != nchar) {
			errno = LEDTAG_ERR_MSGSPAN;
			return -1;
		}
		nchar += msg->nchar;
	}
	if(hid_write(self->handle, report, sizeof(report)) < 0) {
		errno = LEDTAG_ERR_IO;
		return -1;
	}

	nbytes = nchar * HEIGHT;
	si = self->data;
	while(nbytes) {
		unsigned cur = MIN(nbytes, PACKET_SIZE);
		memset(report, 0, sizeof(report));
		memcpy(packet, si, cur);
		if(hid_write(self->handle, report, sizeof(report)) < 0) {
			errno = LEDTAG_ERR_IO;
			return -1;
		}
		si += cur;
		nbytes -= cur;
	}
	return 0;
}

