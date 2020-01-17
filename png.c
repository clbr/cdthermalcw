/*
    CDThermalCW: a Linux driver for the Casio CW-75 thermal CD printer
    Copyright (C) 2020 Lauri Kasanen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "main.h"

#include <png.h>

#define err(...) { fprintf(stderr, __VA_ARGS__); return 1; }

u8 readpng(const char name[], struct bitmap_t *out) {

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if (!png_ptr) err("PNG error\n");
	png_infop info = png_create_info_struct(png_ptr);
	if (!info) err("PNG error\n");
	if (setjmp(png_jmpbuf(png_ptr))) err("PNG error\n");

	FILE *f = fopen(name, "r");
	if (!f) err("Can't open %s\n", name);

	png_init_io(png_ptr, f);
	png_read_png(png_ptr, info,
		PNG_TRANSFORM_PACKING|PNG_TRANSFORM_STRIP_16|PNG_TRANSFORM_EXPAND|
		PNG_TRANSFORM_STRIP_ALPHA|PNG_TRANSFORM_GRAY_TO_RGB,
		NULL);
	u8 **rows = png_get_rows(png_ptr, info);
	const u32 imgw = png_get_image_width(png_ptr, info);
	const u32 imgh = png_get_image_height(png_ptr, info);
	const u8 type = png_get_color_type(png_ptr, info);
	const u8 depth = png_get_bit_depth(png_ptr, info);

	if (imgh != ROWS)
		err("Image height not 128\n");
	if (imgw > COLS)
		err("Image too wide, max %u\n", COLS);
	if (imgw < COLS / 2)
		err("Image too narrow, width must be at least %u\n", COLS / 2);
	out->w = imgw;
	out->h = imgh;

	if (type != PNG_COLOR_TYPE_RGB)
		err("Something went wrong, PNG not in color after transformations\n");
	if (depth != 8)
		err("Something went wrong, PNG not depth 8 after transformations\n");

	const u32 rowbytes = png_get_rowbytes(png_ptr, info);
	if (rowbytes != imgw * 3)
		err("Unexpected row size\n");

	u32 y;
	for (y = 0; y < imgh; y++) {
		u32 x;
		for (x = 0; x < imgw; x++) {
			out->data[y * imgw + x] = rows[y][x * 3];
		}
	}

	fclose(f);
	png_destroy_info_struct(png_ptr, &info);
	png_destroy_read_struct(&png_ptr, NULL, NULL);

	return 0;
}
