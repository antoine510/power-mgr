/*
*  ir-ctl.c - Program to send and record IR using lirc interface
*
*  Copyright (C) 2016 Sean Young <sean@mess.org>
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, version 2 of the License.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "LIRCException.hpp"
#include "ir.hpp"

#include <linux/lirc.h>

/* taken from glibc unistd.h */
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
	({ long int __result;                                                     \
	do __result = (long int) (expression);                                 \
	while (__result == -1L && errno == EINTR);                             \
	__result; })
#endif

/* See drivers/media/rc/ir-lirc-codec.c line 23 */
#define LIRCBUF_SIZE	512


int IR::lirc_open(const char *device, ir_proto proto) {
	int fd;
	unsigned features;

	fd = TEMP_FAILURE_RETRY(open(device, O_RDONLY));
	if (fd == -1) throw LIRCException(device, "couldn't open lirc device");

	try {
		int rc = ioctl(fd, LIRC_GET_FEATURES, &features);
		if (rc) throw LIRCException(device, "failed to get lirc features");

		if (!(features & LIRC_CAN_SEND_PULSE)) throw LIRCException(device, "cannot send raw ir");
		if (!(features & LIRC_CAN_SET_SEND_CARRIER)) throw LIRCException(device, "not able to set carrier frequency");

		unsigned carrier = IR::protocol_carrier(proto);
		rc = ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier);
		if (rc) throw LIRCException(device, "error setting carrier frequency");

		int mode = LIRC_MODE_PULSE;
		if (ioctl(fd, LIRC_SET_SEND_MODE, &mode)) throw LIRCException(device, "failed to set send mode pulse");

	} catch(const std::exception&) {
		close(fd);
		throw;
	}

	return fd;
}

void IR::lirc_send(int fd, ir_proto proto, unsigned scancode) {
	if(!protocol_scancode_valid(proto, scancode)) throw LIRCException("Invalid IR scancode");

	unsigned len;
	unsigned buf[LIRCBUF_SIZE];
	len = IR::protocol_encode(proto, scancode, buf);

	size_t size = len * sizeof(unsigned);
	ssize_t ret = TEMP_FAILURE_RETRY(write(fd, buf, size));
	if (ret != size) throw LIRCException("Failed to send IR");
}

void IR::lirc_close(int fd) {
	close(fd);
}
