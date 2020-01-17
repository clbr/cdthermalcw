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

int main(int argc, char **argv) {

	printf("CDThermalCW: a Linux driver for the Casio CW-75 thermal CD printer\n\n");

	// check device status
	device_info();

	if (argc != 2) {
		printf("Usage: %s file.png\n"
			"The PNG file should be 592x128 or narrower, grayscale or monochrome.\n",
			argv[0]);
	} else {
		struct bitmap_t bitmap;
		if (readpng(argv[1], &bitmap))
			return 1;

		return print(&bitmap);
	}

	return 0;
}
