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

#include "ir-encode.hpp"

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
#define IR_DEFAULT_TIMEOUT 125000


int open_lirc(const char *device, rc_proto proto)
{
	int fd;
	unsigned features;

	fd = TEMP_FAILURE_RETRY(open(device, O_RDONLY));
	if (fd == -1) {
		fprintf(stderr, "%s: cannot open: %m\n", device);
		return -1;
	}

	rc = ioctl(fd, LIRC_GET_FEATURES, &features);
	if (rc) {
		fprintf(stderr, "%s: failed to get lirc features: %m\n", device);
		close(fd);
		return -1;
	}

	if (!(features & LIRC_CAN_SEND_PULSE)) {
		fprintf(stderr, "%s: device cannot send raw ir\n", device);
		close(fd);
		return -1;
	}

	if (!(features & LIRC_CAN_SET_SEND_CARRIER)) {
		fprintf(stderr, "%s: does not support setting send carrier frequency\n", device);
		close(fd);
		return -1;
	}

	unsigned carrier = protocol_carrier(proto);
	rc = ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier);
	if (rc != 0) {
		fprintf(stderr, "warning: %s: set send carrier returned %d, should return 0\n", device, rc);
		close(fd);
		return -1;
	}

	int mode = LIRC_MODE_PULSE;
	if (ioctl(fd, LIRC_SET_SEND_MODE, &mode)) {
		fprintf(stderr, "%s: failed to set send mode: %m\n", device);
		close(fd);
		return -1;
	}

	return fd;
}

int lirc_send(int fd, rc_proto proto, unsigned scancode) {
	if(!protocol_scancode_valid(proto, scancode)) {
		fprintf(stderr, "Invalid scancode %d for given protocol\n", scancode);
		return -1;
	}

	unsigned len;
	unsigned buf[LIRCBUF_SIZE];
	len = protocol_encode(proto, scancode, buf);

	size_t size = len * sizeof(unsigned);
	ssize_t ret = TEMP_FAILURE_RETRY(write(fd, buf, size));
	if (ret < 0) {
		fprintf(stderr, "Failed to send IR: %m\n");
		return -1;
	}

	if (size < ret) {
		fprintf(stderr, "warning: Sent IR %zd out %zd edges\n", ret / sizeof(unsigned), len);
		return -1;
	}

	return 0;
}

void lirc_close(int fd) {
	close(fd);
}
