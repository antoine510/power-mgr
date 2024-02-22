#pragma once

bool protocol_scancode_valid(unsigned scancode);
unsigned protocol_encode(unsigned scancode, unsigned *buf);
