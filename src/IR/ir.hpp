#pragma once

#include "ir-encode.h"

int lirc_open(const char *device, rc_proto proto);
int lirc_send(int fd, rc_proto proto, unsigned scancode);
void lirc_close(int fd);
