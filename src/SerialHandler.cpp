#include "SerialHandler.hpp"
#include <vector>
#include <memory>
#include <cstring>
#include <thread>

#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h> // for close()
#include <fcntl.h>
#include <termios.h>
#include <string.h>

#define GET_ERROR_STR std::string(strerror(errno))
#define GET_ERROR errno

static int _define_from_baudrate(int baudrate) {
	switch (baudrate) {
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	default:
		throw std::runtime_error("Unknown baudrate " + std::to_string(baudrate));
	}
}

SerialHandler::SerialHandler(const std::string& path, int baudrate, size_t read_buf_sz) :
    _read_buf_sz(read_buf_sz),
    _read_buf(new unsigned char[_read_buf_sz]) {

	// open() hangs on macOS or Linux devices(e.g. pocket beagle) unless you give it O_NONBLOCK
	_fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (_fd == -1) throw std::runtime_error("Open failed: " + GET_ERROR_STR);

	// We need to clear the O_NONBLOCK again because we can block while reading
	// as we do it in a separate thread.
	if (fcntl(_fd, F_SETFL, 0) == -1) throw std::runtime_error("fcntl failed: " + GET_ERROR_STR);

	struct termios tc;
	bzero(&tc, sizeof(tc));

	if (tcgetattr(_fd, &tc) != 0) {
		::close(_fd);
		throw std::runtime_error("tcgetattr failed: " + GET_ERROR_STR);
	}

	tc.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
	tc.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
	tc.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG | TOSTOP);
	tc.c_cflag &= ~(CSIZE | PARENB | CRTSCTS);
	tc.c_cflag |= CS8;

	tc.c_cc[VMIN] = 0; // We are ok with 0 bytes.
	tc.c_cc[VTIME] = 10; // Timeout after 1 second.

	tc.c_cflag |= CLOCAL; // Without this a write() blocks indefinitely.

	const int baudrate_or_define = _define_from_baudrate(baudrate);

	if (cfsetispeed(&tc, baudrate_or_define) != 0) {
		::close(_fd);
		throw std::runtime_error("cfsetispeed failed: " + GET_ERROR_STR);
	}

	if (cfsetospeed(&tc, baudrate_or_define) != 0) {
		::close(_fd);
		throw std::runtime_error("cfsetospeed failed: " + GET_ERROR_STR);
	}

	if (tcsetattr(_fd, TCSANOW, &tc) != 0) {
		::close(_fd);
		throw std::runtime_error("tcsetattr failed: " + GET_ERROR_STR);
	}
}

SerialHandler::~SerialHandler() noexcept {
    if (_read_buf) delete[] _read_buf;
	if (_fd > -1) ::close(_fd);
	_fd = -1;
}

std::vector<uint8_t> SerialHandler::Read() const {
	{
		fd_set rx_fd_set{};
		FD_SET(_fd, &rx_fd_set);

		int res = pselect(_fd + 1, &rx_fd_set, nullptr, nullptr, &_timeout, nullptr);
		if(res < 0) {
			throw std::runtime_error("Select failed: " + GET_ERROR_STR);
		} else if(res == 0) {
			throw ReadTimeoutException();
		}
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));	// Wait for all data

	int read_len = ::read(_fd, _read_buf, _read_buf_sz);
	if (read_len <= 0) throw ReadTimeoutException();

	auto ret = std::vector<uint8_t>(read_len);
	std::memcpy(ret.data(), _read_buf, read_len);
	return ret;
}

void SerialHandler::Write(const uint8_t* buf, size_t sz) const {
	if (::write(_fd, buf, sz) != sz) throw WriteException();
}
