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

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "ir-encode.hpp"

#define NS_TO_US(x) (((x)+500)/1000)

using namespace IR;

static int nec_encode(ir_proto proto, unsigned scancode, unsigned *buf)
{
	const int nec_unit = 562500;
	int n = 0;

	constexpr auto add_byte = [](unsigned* buffer, int n, unsigned bits)
	{
		int i;
		for (i=0; i<8; i++) {
			buffer[n++] = NS_TO_US(nec_unit);
			if (bits & (1 << i))
				buffer[n++] = NS_TO_US(nec_unit * 3);
			else
				buffer[n++] = NS_TO_US(nec_unit);
		}
	};

	buf[n++] = NS_TO_US(nec_unit * 16);
	buf[n++] = NS_TO_US(nec_unit * 8);

	switch (proto) {
	default:
		return 0;
	case RC_PROTO_NEC:
		add_byte(buf, n, scancode >> 8);
		add_byte(buf, n, ~(scancode >> 8));
		add_byte(buf, n, scancode);
		add_byte(buf, n, ~scancode);
		break;
	case RC_PROTO_NECX:
		add_byte(buf, n, scancode >> 16);
		add_byte(buf, n, scancode >> 8);
		add_byte(buf, n, scancode);
		add_byte(buf, n, ~scancode);
		break;
	case RC_PROTO_NEC32:
		/*
		 * At the time of writing kernel software nec decoder
		 * reverses the bit order so it will not match. Hardware
		 * decoders do not have this issue.
		 */
		add_byte(buf, n, scancode >> 24);
		add_byte(buf, n, scancode >> 16);
		add_byte(buf, n, scancode >> 8);
		add_byte(buf, n, scancode);
		break;
	}

	buf[n++] = NS_TO_US(nec_unit);

	return n;
}

static int jvc_encode(ir_proto proto, unsigned scancode, unsigned *buf)
{
	const int jvc_unit = 525000;
	int i;

	/* swap bytes so address comes first */
	scancode = ((scancode << 8) & 0xff00) | ((scancode >> 8) & 0x00ff);

	*buf++ = NS_TO_US(jvc_unit * 16);
	*buf++ = NS_TO_US(jvc_unit * 8);

	for (i=0; i<16; i++) {
		*buf++ = NS_TO_US(jvc_unit);

		if (scancode & 1)
			*buf++ = NS_TO_US(jvc_unit * 3);
		else
			*buf++ = NS_TO_US(jvc_unit);

		scancode >>= 1;
	}

	*buf = NS_TO_US(jvc_unit);

	return 35;
}

static int sanyo_encode(ir_proto proto, unsigned scancode, unsigned *buf)
{
	const int sanyo_unit = 562500;

	auto add_bits = [](unsigned* buffer, int bits, int count)
	{
		int i;
		for (i=0; i<count; i++) {
			*buffer++ = NS_TO_US(sanyo_unit);

			if (bits & (1 << i))
				*buffer++ = NS_TO_US(sanyo_unit * 3);
			else
				*buffer++ = NS_TO_US(sanyo_unit);
		}
	};

	*buf++ = NS_TO_US(sanyo_unit * 16);
	*buf++ = NS_TO_US(sanyo_unit * 8);

	add_bits(buf, scancode >> 8, 13);
	add_bits(buf, ~(scancode >> 8), 13);
	add_bits(buf, scancode, 8);
	add_bits(buf, ~scancode, 8);

	*buf = NS_TO_US(sanyo_unit);

	return 87;
}

static int sharp_encode(ir_proto proto, unsigned scancode, unsigned *buf)
{
	const int sharp_unit = 40000;

	auto add_bits = [](unsigned* buffer, int bits, int count)
	{
		int i;
		for (i=0; i<count; i++) {
			*buffer++ = NS_TO_US(sharp_unit * 8);

			if (bits & (1 << i))
				*buffer++ = NS_TO_US(sharp_unit * 50);
			else
				*buffer++ = NS_TO_US(sharp_unit * 25);
		}
	};

	add_bits(buf, scancode >> 8, 5);
	add_bits(buf, scancode, 8);
	add_bits(buf, 1, 2);

	*buf++ = NS_TO_US(sharp_unit * 8);
	*buf++ = NS_TO_US(sharp_unit * 1000);

	add_bits(buf, scancode >> 8, 5);
	add_bits(buf, ~scancode, 8);
	add_bits(buf, ~1, 2);
	*buf++ = NS_TO_US(sharp_unit * 8);

	return (13 + 2) * 4 + 3;
}

static int sony_encode(ir_proto proto, unsigned scancode, unsigned *buf)
{
	const int sony_unit = 600000;
	int n = 0;

	auto add_bits = [](unsigned* buffer, int n, int bits, int count)
	{
		int i;
		for (i=0; i<count; i++) {
			if (bits & (1 << i))
				buffer[n++] = NS_TO_US(sony_unit * 2);
			else
				buffer[n++] = NS_TO_US(sony_unit);

			buffer[n++] = NS_TO_US(sony_unit);
		}
	};

	buf[n++] = NS_TO_US(sony_unit * 4);
	buf[n++] = NS_TO_US(sony_unit);

	switch (proto) {
	case RC_PROTO_SONY12:
		add_bits(buf, n, scancode, 7);
		add_bits(buf, n, scancode >> 16, 5);
		break;
	case RC_PROTO_SONY15:
		add_bits(buf, n, scancode, 7);
		add_bits(buf, n, scancode >> 16, 8);
		break;
	case RC_PROTO_SONY20:
		add_bits(buf, n, scancode, 7);
		add_bits(buf, n, scancode >> 16, 5);
		add_bits(buf, n, scancode >> 8, 8);
		break;
	default:
		return 0;
	}

	/* ignore last space */
	return n - 1;
}

static const struct {
	char name[10];
	unsigned scancode_mask;
	unsigned max_edges;
	unsigned carrier;
	int (*encode)(ir_proto proto, unsigned scancode, unsigned *buf);
} encoders[RC_PROTO_COUNT] = {
	{ "jvc", 0xffff, 35, 38000, jvc_encode },
	{ "sony12", 0x1f007f, 25, 40000, sony_encode },
	{ "sony15", 0xff007f, 31, 40000, sony_encode },
	{ "sony20", 0x1fff7f, 41, 40000, sony_encode },
	{ "nec", 0xffff, 67, 38000, nec_encode },
	{ "necx", 0xffffff, 67, 38000, nec_encode },
	{ "nec32", 0xffffffff, 67, 38000, nec_encode },
	{ "sanyo", 0x1fffff, 87, 38000, sanyo_encode },
	{ "sharp", 0x1fff, 63, 38000, sharp_encode }
};

unsigned IR::protocol_carrier(ir_proto proto)
{
	return encoders[proto].carrier;
}

unsigned IR::protocol_max_size(ir_proto proto)
{
	return encoders[proto].max_edges;
}

unsigned IR::protocol_scancode_mask(ir_proto proto)
{
	return encoders[proto].scancode_mask;
}

bool IR::protocol_scancode_valid(ir_proto p, unsigned s)
{
	if (s & ~encoders[p].scancode_mask)
		return false;

	if (p == RC_PROTO_NECX) {
		return (((s >> 16) ^ ~(s >> 8)) & 0xff) != 0;
	} else if (p == RC_PROTO_NEC32) {
		return (((s >> 24) ^ ~(s >> 16)) & 0xff) != 0;
	}

	return true;
}

unsigned IR::protocol_encode(ir_proto proto, unsigned scancode, unsigned *buf)
{
	return encoders[proto].encode(proto, scancode, buf);
}
