#pragma once

#include "ir-encode.hpp"

namespace IR {

int lirc_open(const char *device, ir_proto proto);
void lirc_send(int fd, ir_proto proto, unsigned scancode);
void lirc_close(int fd);

}
