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

#include <libusb.h>
#include <string.h>

struct prod_t {
	u16 id;
	char name[16];
};

static const struct prod_t products[] = {
	{0x4007, "CW-50"},
	{0x4103, "CW-70"},
	{0x4104, "CW-75"},
	{0x4009, "CW-100"},
	{0x4105, "KLD-700"},
	{0x4106, "KLD-300"},
	{0x410d, "KLD-350"},
	{0x4108, "KLD-200"},
	{0x4019, "CW-E60"},
	{0x410a, "CW-K80"},
	{0x410b, "CW-K85"},
	{0x0, "Unknown"},
};

static struct {
	libusb_device_handle *usb;
	libusb_device *usbdev;

	union {
		u32 cmdint;
		u16 cmdshort[4];
		u8 cmdbytes[64];
	};
	union {
		u64 statuslong;
		u32 statusints[2];
		u8 statusbytes[8];
	};

	u16 id;

	u8 data[ROWS * COLS / 8];
} dev;

static u8 cmd_prep() {

	libusb_device **devs;

	if (libusb_init(NULL) < 0)
		return 1;

	int num = libusb_get_device_list(NULL, &devs);
	if (num < 0)
		return 1;

	u32 i;
	for (i = 0; i < (u32) num; i++) {
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(devs[i], &desc) < 0)
			return 1;

		if (desc.idVendor != 1999) // Casio
			continue;
		dev.id = desc.idProduct;

		int ret = libusb_open(devs[i], &dev.usb);
		if (ret == LIBUSB_ERROR_ACCESS)
			printf("Insufficient permissions (try as root?)\n");

		if (ret < 0) {
			dev.id = 0;
			return 1;
		}

		dev.usbdev = libusb_get_device(dev.usb);

		break;
	}

	libusb_free_device_list(devs, 1);

	return 0;
}

static u8 cmd_send(const u32 len) {

	int written;
	int ret = libusb_bulk_transfer(dev.usb, 1, dev.cmdbytes, len, &written, 0);
	if (ret < 0)
		libusb_clear_halt(dev.usb, 1);

	//printf("sent %u %d\n", written, ret);
	return ret != 0;
}

static u8 cmd_recv() {

	int moved;
	dev.statuslong = 0xffffffffffffffff;
	int ret = libusb_bulk_transfer(dev.usb, 0x82, dev.statusbytes, 8, &moved, 0);
	if (ret < 0)
		libusb_clear_halt(dev.usb, 0x82);

	//printf("got %u %d\n", moved, ret);
	return ret != 0;
}

static u8 cmd_device() {

	u8 status;
	dev.cmdshort[0] = 2;
	status = cmd_send(2);
	if (status == 0) {
		status = cmd_recv();
		if (status == 0 && dev.statusbytes[0] == 2) {
			//printf("Got device bytes 0x%x\n", dev.statusints[1]);
			return 0;
		}
	}
	return 1;
}

static u8 cmd_reset() {

	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 1;
	status = cmd_send(2);
	if (status == 0) {
		cmd_recv();
		return dev.statusbytes[0] != 6;
	}
	return 1;
}

static u8 cmd_battery() {
	u8 status;

	dev.cmdshort[0] = 0x1d02;
	status = cmd_send(2);
	if (status != 0)
		return 1;
	status = cmd_recv();
	if (status != 0)
		return 1;
	if (dev.statusbytes[0] != 2)
		return 1;
	if (dev.statusbytes[4] > 1) {
		printf("Battery low\n");
		return 1;
	}
	//printf("battery low byte %u\n", dev.statusbytes[4]);
	return 0;
}

#define fiver(inner) status = cmd_send(2); \
	if (status == 0) { \
	status = cmd_recv(); \
		if (status == 0 && dev.statusbytes[0] == 2 && \
			dev.statusbytes[1] == 0x80 && \
			dev.statusbytes[2] == 1) { \
			inner \
			return 0; \
		} \
	}

static u8 cmd_eject();

static u8 cmd_media() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0x21;
	fiver(
		if (dev.statusbytes[4]) {
			printf("There is no CD in the printer\n");
			cmd_eject();
			return 1;
		}
	)
	return 1;
}

static u8 cmd_toner() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0x1a;
	fiver(
		if (dev.statusbytes[4] > 1) {
			printf("The ink ribbon is empty\n");
			return 1;
		}
	)
	return 1;
}

static u8 cmd_temps() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0x84;
	fiver(
		if (dev.statusbytes[4] < 0x39) {
			printf("The printer is overheated\n");
			return 1;
		}
	)
	return 1;
}

// no idea what HRK is
static u8 cmd_hrkstat() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0x80;
	fiver(
		//printf("hrk status %u\n", dev.statusbytes[4]);
	)
	return 1;
}

// something to do with horizontal position of the ribbon
static u8 cmd_hpstat() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0x24;
	fiver (
		//printf("hp status %u\n", dev.statusbytes[4]);
	)
	return 1;
}

static u8 cmd_errstat() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0x22;
	fiver(
		if (dev.statusbytes[4])
			printf("err stat %u\n", dev.statusbytes[4]);
	)
	return 1;
}

static u8 cmd_enoughribbon() {
	u8 status;

	dev.cmdshort[0] = 0x2002;
	status = cmd_send(2);
	if (status == 0) {
		status = cmd_recv();;
		if (status == 0 && dev.statusbytes[0] == 2) {
			if (dev.statusbytes[4]) {
				printf("There is not enough ink ribbon\n");
				return 1;
			}
			return 0;
		}
	}
	return 1;
}

static u8 cmd_slack() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0x25;
	status = cmd_send(2);
	if (status == 0) {
		cmd_recv();
		return dev.statusbytes[0] != 6;
	}
	return 1;
}

static u8 cmd_eject() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0x23;
	status = cmd_send(2);
	if (status == 0) {
		cmd_recv();
		return dev.statusbytes[0] != 6;
	}
	return 1;
}

static u8 check_status() {
	if (cmd_device() ||
		cmd_reset() ||
		cmd_battery() ||
		cmd_toner() ||
		cmd_temps() ||
		cmd_hrkstat() ||
		cmd_hpstat() ||
		cmd_errstat() ||
		cmd_enoughribbon())
		return 1;
	return 0;
}

static u8 cmd_data(const u8 *data, const u8 len) {
	u8 status;
	if (len && len < 61) {
		dev.cmdshort[0] = 0xfe02;
		dev.cmdbytes[2] = len;
		dev.cmdbytes[3] = 0;
		memcpy(&dev.cmdbytes[4], data, len);
		status = cmd_send(len + 4);
		if (status == 0) {
			cmd_recv();
			return dev.statusbytes[0] != 6;
		}
	}
	return 1;
}

static u8 cmd_print() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 0xc;

	status = cmd_send(2);
	if (status == 0) {
		cmd_recv();
		return dev.statusbytes[0] != 6;
	}
	return 1;
}

/*
static u8 cmd_print2(const u16 cols) {
	u8 status;

	dev.cmdint = 0x20e02;
	dev.cmdbytes[4] = cols;
	dev.cmdbytes[5] = cols >> 8;
	status = cmd_send(6);
	if (status == 0) {
		cmd_recv();
		return dev.statusbytes[0] != 6;
	}

	return 1;
}*/

static u8 cmd_printend() {
	u8 status;

	dev.cmdbytes[0] = 2;
	dev.cmdbytes[1] = 4;
	status = cmd_send(2);
	if (status == 0) {
		cmd_recv();
		return dev.statusbytes[0] != 6;
	}
	return 1;
}

static void convert_bitmap(const struct bitmap_t *in) {

	u32 x, y;
	for (y = 0; y < in->h; y++) {
		for (x = 0; x < in->w; x++) {
			if (in->data[y * in->w + x] >= 128)
				continue;
			dev.data[y / 8 + x * 16] |= 1 << (y % 8);
		}
	}
}

void device_info() {

	memset(&dev, 0, sizeof(dev));

	u8 status = cmd_prep();
	if (status) return;

	u32 i;
	for (i = 0; products[i].id; i++) {
		if (products[i].id == dev.id)
			break;
	}

	if (!dev.id) {
		printf("No Casio printers found.\n\n");
		return;
	}

	printf("Detected a %s\n\n", products[i].name);
	if (dev.id != 0x4104) // cw-75
		dev.id = 0;
}

u8 print(const struct bitmap_t *in) {

	if (!dev.id)
		return 1;
	convert_bitmap(in);

	struct libusb_config_descriptor *desc;
	if (libusb_get_config_descriptor(dev.usbdev, 0, &desc) < 0)
		return 1;

	libusb_set_auto_detach_kernel_driver(dev.usb, 1);
	//printf("%u interfaces\n", desc->bNumInterfaces);
	if (libusb_claim_interface(dev.usb, 0) < 0)
		return 1;

	const struct libusb_interface_descriptor *iface = desc->interface[0].altsetting;
	//printf("%u endpoints\n", iface->bNumEndpoints);
	if (iface->bNumEndpoints != 2)
		return 1;

	// USB setup done, print
	cmd_device();
	if (cmd_reset() ||
		cmd_battery() ||
		cmd_toner() ||
		cmd_media() ||
		cmd_slack() ||
		cmd_enoughribbon() ||
		cmd_temps())
		return 1;

	// Set density? TODO

	u32 i;
	const u8 *data = dev.data;
	for (i = ROWS * COLS / 8; i >= 60; i -= 60, data += 60) {
		if (cmd_data(data, 60))
			return 1;
	}

	if (i && cmd_data(data, i))
		return 1;

	if (cmd_print())
		return 1;

	cmd_errstat();
	cmd_printend();
	cmd_enoughribbon();

	cmd_eject();
	if (check_status())
		return 1;

	libusb_release_interface(dev.usb, 0);
	libusb_free_config_descriptor(desc);
	libusb_close(dev.usb);
	libusb_exit(NULL);
	return 0;
}
