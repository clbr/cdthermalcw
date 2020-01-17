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

#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include "lrtypes.h"

#define ROWS 128
#define COLS 592

struct bitmap_t {
	u32 w, h;
	u8 data[ROWS * COLS];
};

// cmd.c
void device_info();
u8 print(const struct bitmap_t *in);

// png.c
u8 readpng(const char name[], struct bitmap_t *out);

#endif
