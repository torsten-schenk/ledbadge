#include <png.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>

#include "arg.h"
#include "lib.h"

static arg_value_t arg_list;
static arg_value_t arg_file;
static arg_value_t arg_bright;

enum {
	ARG_LIST, ARG_BRIGHT, ARG_ENABLE, ARG_MODE, ARG_OPTION, ARG_SPEED, ARG_FILE
};

static arg_list_t argdef[] = {
	{ { 'l', "list"   }, ARG_FLAG  , &arg_list  , "find device and list device options; ignore all other options and don't flash" },
	{ { 'b', "bright" }, ARG_INT   , &arg_bright, "brightness" },
	{ { 'e', "enable" }, ARG_INT   , NULL       , "enable current message with given width and start a new message" },
	{ { 'm', "mode"   }, ARG_STRING, NULL       , "set mode of current message" },
	{ { 'o', "option" }, ARG_STRING, NULL       , "enable given option" },
	{ { 's', "speed"  }, ARG_INT   , NULL       , "set speed" },
	{ {               }, ARG_STRING, &arg_file  , ".png image file (1-bit depth)" },
};

static int width;
static int height;
static png_byte color_type;
static png_byte bit_depth;
static png_bytep *rows;

static int read_png(
		const char *filename)
{
	int ret = -1;
	unsigned char header[8];
	int y;
	png_structp png_ptr;
	png_infop info_ptr;

	FILE *fp = fopen(filename, "rb");
	if(!fp) {
		printf("error opening file\n");
		goto error_0;
	}
	if(fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
		printf("file is not a valid .png\n");
		goto error_1;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr) {
		printf("error creating .png read structure\n");
		goto error_1;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr) {
		printf("error creating .png info structure\n");
		goto error_1;
	}

	if(setjmp(png_jmpbuf(png_ptr))) {
		printf("error initializing .png io\n");
		goto error_1;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	png_read_update_info(png_ptr, info_ptr);

	if(bit_depth != 1) {
		printf(".png file must have bit depth of 1, but found %d\n", bit_depth);
		goto error_1;
	}
	else if(png_get_rowbytes(png_ptr, info_ptr) < (width + 7) / 8) {
		printf("invalid row size for .png\n");
		goto error_1;
	}

	if(setjmp(png_jmpbuf(png_ptr))) {
		printf("error initializing .png io\n");
		goto error_1;
	}

	rows = (png_bytep*) malloc(sizeof(png_bytep) * height);
	for(y = 0; y < height; y++)
		rows[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr, info_ptr));

	png_read_image(png_ptr, rows);
	ret = 0;

error_1:
	fclose(fp);
error_0:
	return ret;

}

static void free_png()
{
	int y;
  for(y = 0; y < height; y++)
		free(rows[y]);
	free(rows);
}

static ledtag_t *ledtag;
static const ledtag_info_t *info;

static int action_list()
{
	unsigned i;
	printf("device name     : %s\n", info->name);
	printf("image properties: height=%d max_width=%d char_width=%d display_chars=%d max_chars=%d\n", info->height, info->max_width, info->char_width, info->display_chars, info->max_chars);
	printf("setup           : max_brightness=%d max_messages=%d max_speed=%d\n", info->max_brightness, info->max_messages, info->max_speed);
	printf("possible modes  :\n");
	for(i = 0; i < info->n_mode; i++)
		printf("  %s\n", info->mode_name[i]);
	printf("possible options:\n");
	for(i = 0; i < info->n_option; i++)
		printf("  %s\n", info->option_name[i]);
	return 0;
}

static unsigned cur_msg = 0;
static unsigned cur_bchar = 0;
static unsigned cur_mode = 0;
static unsigned cur_speed = 3;
static unsigned cur_options = 0;

static int handle_args(
		int id,
		arg_value_t value,
		void *user)
{
	if(cur_msg == info->max_messages) {
		printf("too many messages specified\n");
		return -1;
	}
	if(id == ARG_ENABLE) {
		if(ledtag_setup_message(ledtag, cur_msg, cur_bchar, value.i, cur_mode, cur_speed, cur_options) < 0) {
			printf("invalid message specification\n");
			return -1;
		}
		cur_msg++;
		cur_bchar += value.i;
	}
	else if(id == ARG_MODE) {
		unsigned i;
		for(i = 0; i < info->n_mode; i++)
			if(!strcasecmp(value.s, info->mode_name[i])) {
				cur_mode = i;
				break;
			}
		if(i == info->n_mode) {
			printf("no such mode: %s\n", value.s);
			return -1;
		}
	}
	else if(id == ARG_OPTION) {
		unsigned i;
		for(i = 0; i < info->n_option; i++)
			if(!strcasecmp(value.s, info->option_name[i])) {
				cur_options ^= 1 << i;
				break;
			}
		if(i == info->n_option) {
			printf("no such option: %s\n", value.s);
			return -1;
		}
	}
	else if(id == ARG_SPEED) {
		if(value.i > info->max_speed) {
			printf("invalid speed: %lld\n", value.i);
			return -1;
		}
		cur_speed = value.i;
	}
	return 0;
}

int main(
		int argn,
		char **argv)
{
	int ret = 1;

	if(ledtag_init() < 0) {
		printf("error initializing ledtag library\n");
		goto error_0;
	}
	ledtag = ledtag_open(NULL);
	if(!ledtag) {
		printf("error opening ledtag device\n");
		goto error_1;
	}
	info = ledtag_info(ledtag);

	if(arg_parse(1, argn, argv, argdef, handle_args, NULL) < 0) {
		printf("invalid argument\n");
		goto error_2;
	}

	if(arg_list.b) {
		if(action_list() < 0)
			goto error_2;
	}
	else {
		int x;
		int y;
		if(!arg_file.s) {
			printf("missing filename\n");
			goto error_2;
		}
		if(read_png(arg_file.s) < 0)
			goto error_2;
		if(arg_bright.i > info->max_brightness) {
			printf("invalid brightness: %lld\n", arg_bright.i);
			goto error_2;
		}
		ledtag_setup(ledtag, arg_bright.i);
		if(width % info->char_width) {
			printf("invalid width of .png file: must be a multiple of %d for ledtag\n", info->char_width);
			free_png();
			goto error_2;
		}
		else if(height != info->height) {
			printf("invalid height of .png file: must be %d for ledtag\n", info->height);
			free_png();
			goto error_2;
		}

		for(y = 0; y < height; y++)
			for(x = 0; x < width; x++)
				if(rows[y][x / 8] & (0x80 >> (x % 8)))
					ledtag_set(ledtag, x, y);
		if(!cur_msg && ledtag_setup_message(ledtag, 0, 0, width / info->char_width, cur_mode, cur_speed, cur_options) < 0) {
			printf("invalid default message specification\n");
			goto error_2;
		}


		free_png();

		if(ledtag_flash(ledtag) < 0) {
			printf("error flashing ledtag\n");
			goto error_2;
		}
	}

	ret = 0;

error_2:
	ledtag_close(ledtag);
error_1:
	ledtag_exit();
error_0:
	return ret;
}

