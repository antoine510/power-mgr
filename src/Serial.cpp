#include "Serial.hpp"

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <asm/termbits.h>
#include <sys/ioctl.h>

#define GET_ERROR_STR std::string(strerror(errno))
#define GET_ERROR errno

Serial::Serial(const std::string& path, int baudrate) {
	if (_fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_SYNC); _fd < 0)
		throw std::runtime_error("Open failed: " + GET_ERROR_STR);

	termios2 tc{};

	tc.c_iflag = 0;
	tc.c_oflag = 0;
	tc.c_lflag = 0;
	tc.c_cflag = CLOCAL | CS8 | BOTHER;
	tc.c_cc[VMIN] = 0;
	tc.c_cc[VTIME] = 0;

	/** HACK to really set the baudrate!! */
	tc.c_ispeed = 19200;
	tc.c_ospeed = 19200;

	if(ioctl(_fd, TCSETS2, &tc) != 0) {
		::close(_fd);
		throw std::runtime_error("TCSETS2 failed: " + GET_ERROR_STR);
	}
	/** END HACK */

	tc.c_ispeed = baudrate;
	tc.c_ospeed = baudrate;

	if(ioctl(_fd, TCSETS2, &tc) != 0) {
		::close(_fd);
		throw std::runtime_error("TCSETS2 failed: " + GET_ERROR_STR);
	}
}

Serial::~Serial() noexcept {
	if (_fd > -1) ::close(_fd);
	_fd = -1;
}

bool Serial::WaitForData(int64_t timeout_ns) const {
	fd_set rx_fd_set{};
	FD_SET(_fd, &rx_fd_set);

	timespec tout{timeout_ns / 1'000'000'000, timeout_ns % 1'000'000'000};
	int res = pselect(_fd + 1, &rx_fd_set, nullptr, nullptr, &tout, nullptr);
	if(res < 0) {
		if(errno == EINTR) return false;
		throw std::runtime_error("Select failed: " + GET_ERROR_STR);
	} else if(res == 0) {
		return false;
	}
	return true;
}

std::vector<uint8_t> Serial::Read(size_t expectedSize) const {
	static uint8_t buffer[4096];
	ssize_t offset = 0;
	do {
		ssize_t numRead = ::read(_fd, buffer + offset, sizeof(buffer) - offset);
		if(numRead < 0) throw std::runtime_error("Read failed: " + GET_ERROR_STR);
		offset += numRead;
		if(expectedSize && offset >= expectedSize) break;
	} while(WaitForData());
	if(offset == 0) throw ReadTimeoutException();
	if(expectedSize && offset < expectedSize) throw std::runtime_error("Invalid receive size: " + std::to_string(offset));
	std::vector<uint8_t> res(offset);
	std::memcpy(res.data(), buffer, offset);
	return std::move(res);
}

void Serial::Write(const uint8_t* buf, size_t sz) const {
	ioctl(_fd, TCFLSH, 2);	// Flush read and write kernel queues
	if(::write(_fd, buf, sz) != sz) throw WriteException();
}
