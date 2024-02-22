#include "heat-pump.hpp"
#include "ir.hpp"
#include <stdexcept>

HeatPump::HeatPump(const std::string& path) {
    int fd = lirc_open(path.c_str(), rc_proto::RC_PROTO_NEC);
	if(fd < 0) throw std::runtime_error("Couldn't open lirc device");
}

bool HeatPump::SendCommand(Command cmd) {
    return lirc_send(fd, rc_proto::RC_PROTO_NEC, static_cast<unsigned>(cmd)) == 0;
}
