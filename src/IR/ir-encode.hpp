#pragma once

namespace IR {

enum ir_proto {
	RC_PROTO_JVC,
	RC_PROTO_SONY12,
	RC_PROTO_SONY15,
	RC_PROTO_SONY20,
	RC_PROTO_NEC,
	RC_PROTO_NECX,
	RC_PROTO_NEC32,
	RC_PROTO_SANYO,
	RC_PROTO_SHARP,
	RC_PROTO_COUNT
};

unsigned protocol_carrier(ir_proto proto);
unsigned protocol_max_size(ir_proto proto);
bool protocol_scancode_valid(ir_proto proto, unsigned scancode);
unsigned protocol_scancode_mask(ir_proto proto);
unsigned protocol_encode(ir_proto proto, unsigned scancode, unsigned *buf);

}
