/*
 * ir-encode.c - encodes IR scancodes in different protocols
 *
 * Copyright (C) 2016 Sean Young <sean@mess.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * TODO: XMP protocol and MCE keyboard
 */

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "ir-encode.hpp"

#define NS_TO_US(x) (((x)+500)/1000)

const int nec_unit = 562500;

void add_byte(unsigned *buf, int n, unsigned bits)
{
	int i;
	for (i=0; i<8; i++) {
		buf[n++] = NS_TO_US(nec_unit);
		if (bits & (1 << i))
			buf[n++] = NS_TO_US(nec_unit * 3);
		else
			buf[n++] = NS_TO_US(nec_unit);
	}
}

static int nec_encode(unsigned scancode, unsigned *buf)
{
	int n = 0;

	buf[n++] = NS_TO_US(nec_unit * 16);
	buf[n++] = NS_TO_US(nec_unit * 8);

	add_byte(buf, n, scancode >> 8);
	add_byte(buf, n, ~(scancode >> 8));
	add_byte(buf, n, scancode);
	add_byte(buf, n, ~scancode);

	buf[n++] = NS_TO_US(nec_unit);

	return n;
}

static const struct {
	char name[10];
	unsigned scancode_mask;
	unsigned max_edges;
	unsigned carrier;
} encoder = { "nec", 0xffff, 67, 38000 };

bool protocol_scancode_valid(unsigned s)
{
	if (s & ~encoder.scancode_mask)
		return false;

	return (((s >> 16) ^ ~(s >> 8)) & 0xff) != 0;
}

unsigned protocol_encode(unsigned scancode, unsigned *buf)
{
	return nec_encode(scancode, buf);
}
