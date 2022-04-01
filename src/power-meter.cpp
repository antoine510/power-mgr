#include "power-meter.hpp"
#include "power-meter-data.h"
#include <iostream>
#include <vector>
#include <thread>
#include <cstring>

#ifdef WINDOWS
#define _WINSOCKAPI_
#include <windows.h>
#include <winsock2.h>
#ifndef MINGW
#pragma comment(lib, "Ws2_32.lib") // Without this, Ws2_32.lib is not included in static library.
#endif
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h> // for close()
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#endif

#ifndef WINDOWS
#define GET_ERROR_STR std::string(strerror(errno))
#define GET_ERROR errno
#else
#define GET_ERROR_STR std::string(GetLastErrorStdStr().c_str())
#define GET_ERROR GetLastError()

// Taken from:
// https://coolcowstudio.wordpress.com/2012/10/19/getlasterror-as-stdstring/
std::string GetLastErrorStdStr() {
	DWORD error = GetLastError();
	if (error) {
		LPVOID lpMsgBuf;
		DWORD bufLen = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0,
			NULL);
		if (bufLen) {
			LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
			std::string result(lpMsgStr, lpMsgStr + bufLen);

			LocalFree(lpMsgBuf);

			return result;
		}
	}
	return std::string();
}
#endif

#ifndef WINDOWS
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
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	case 500000:
		return B500000;
	case 576000:
		return B576000;
	case 921600:
		return B921600;
	case 1000000:
		return B1000000;
	case 1152000:
		return B1152000;
	case 1500000:
		return B1500000;
	case 2000000:
		return B2000000;
	case 2500000:
		return B2500000;
	case 3000000:
		return B3000000;
	case 3500000:
		return B3500000;
	case 4000000:
		return B4000000;
	default:
		throw std::runtime_error("Unknown baudrate " + std::to_string(baudrate));
	}
}
#endif


PowerMeter::PowerMeter(const std::string& path, int baudrate) {
	int readBufferSize = 1024;
	int writeBufferSize = 1024;

#ifndef WINDOWS
	// open() hangs on macOS or Linux devices(e.g. pocket beagle) unless you give it O_NONBLOCK
	_fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (_fd == -1) throw std::runtime_error("Open failed: " + GET_ERROR_STR);

	// We need to clear the O_NONBLOCK again because we can block while reading
	// as we do it in a separate thread.
	if (fcntl(_fd, F_SETFL, 0) == -1) throw std::runtime_error("fcntl failed: " + GET_ERROR_STR);
#else
	std::wstring wide(path.begin(), path.end());

	_handle = CreateFileW(
		wide.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0, // exclusive-access
		NULL, //  default security attributes
		OPEN_EXISTING,
		0,
		NULL); //  hTemplate must be NULL for comm devices

	if (_handle == INVALID_HANDLE_VALUE) throw std::runtime_error("CreateFile failed with: " + GET_ERROR_STR);
#endif

#ifndef WINDOWS
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

#else

	DCB dcb;
	SecureZeroMemory(&dcb, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);

	if (!GetCommState(_handle, &dcb)) throw std::runtime_error("GetCommState failed with error: " + GET_ERROR_STR);

	dcb.BaudRate = baudrate;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;
	dcb.fBinary = TRUE;
	dcb.fNull = FALSE;
	dcb.fDsrSensitivity = FALSE;

	if (!SetCommState(_handle, &dcb)) throw std::runtime_error("SetCommState failed with error: " + GET_ERROR_STR);

	if (!PurgeComm(_handle, PURGE_RXCLEAR | PURGE_TXCLEAR) ||
		!SetupComm(_handle, readBufferSize, writeBufferSize)) {
		throw std::runtime_error("SetupComm failed with error: " + GET_ERROR_STR);
	}

	COMMTIMEOUTS timeouts;
	// FIXME: The windows api docs are not very clear about read timeouts,
	// and we have to simulate infinite with a big value (uint.MaxValue - 1)
	timeouts.ReadIntervalTimeout = 10;
	timeouts.ReadTotalTimeoutMultiplier = 1;
	timeouts.ReadTotalTimeoutConstant = 100;

	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 0;

	if (!SetCommTimeouts(_handle, &timeouts)) throw std::runtime_error("SetCommTimeouts failed with error: " + GET_ERROR_STR);

	if (!EscapeCommFunction(_handle, CLRDTR) ||
		!EscapeCommFunction(_handle, CLRRTS)) {
		throw std::runtime_error("EscapeCommFunction failed with error: " + GET_ERROR_STR);
	}

#endif
}

PowerMeter::~PowerMeter() noexcept {
#ifndef WINDOWS
	if (_fd > -1) ::close(_fd);
	_fd = -1;
#else
	if (_handle) CloseHandle(_handle);
	_handle = nullptr;
#endif
}

#define READ_U16 (uint16_t)((data[i++] << 8) | data[i++])
#define READ_U32 (uint32_t)(READ_U16 + (READ_U16 << 16))

PowerData PowerMeter::ReadAll() {
	write(Cmd_ReadAll, sizeof(Cmd_ReadAll));
	PowerData res;
	auto data = read();
	int i = 2;
	if(data[i++] != 0x12) throw std::runtime_error("Wrong response count!");
	res.voltage_dv = READ_U16;
	res.current_ma = READ_U32;
	res.power_dw = READ_U32;
	res.energy_wh = READ_U32;
	res.frequency_dhz = READ_U16;
	res.power_factor = READ_U16;
	return res;
}

std::vector<uint8_t> PowerMeter::read() const {
	int read_len = 0;
	fd_set rx_fd_set{};

	uint8_t _readBuffer[1024];
#ifndef WINDOWS
	FD_ZERO(&rx_fd_set);
	FD_SET(_fd, &rx_fd_set);

	int res = pselect(_fd + 1, &rx_fd_set, nullptr, nullptr, &_timeout, nullptr);

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	if(res > 0) {
		read_len = ::read(_fd, _readBuffer, sizeof(_readBuffer));
	} else if(res < 0) {
		std::cerr << "Select failed: " << GET_ERROR_STR << std::endl;
	}
#else
	if (!ReadFile(_handle, _readBuffer, sizeof(_readBuffer), LPDWORD(&read_len), NULL)) {
		if (GET_ERROR != ERROR_IO_PENDING) {
			std::cerr << "ReadFile failed: " << GET_ERROR_STR << std::endl;
			read_len = 0;
		}
	}
#endif

	if (read_len <= 0) throw ReadTimeoutException();

	auto ret = std::vector<uint8_t>(read_len);
	std::memcpy(ret.data(), _readBuffer, read_len);
	return ret;
}

void PowerMeter::write(const uint8_t* cmd_buf, size_t sz) const {
#ifndef WINDOWS
	int sent_len = static_cast<int>(::write(_fd, cmd_buf, sz));
	if (sent_len != sz) {
		std::cerr << "Write wrong count: " << sent_len << " excpected " << sz << std::endl;
		return;
	}
#else
	DWORD bytesWritten = 0;
	if (!WriteFile(_handle, cmd_buf, (DWORD)sz, LPDWORD(&bytesWritten), NULL)) {
		if (GET_ERROR != ERROR_IO_PENDING) {
			std::cerr << "WriteFile failed: " << GET_ERROR_STR << std::endl;
			return;
		}

		if (bytesWritten != sz) {
			std::cerr << "Write wrong count: " << bytesWritten << " excpected " << sz << std::endl;
			return;
		}
	}
#endif
}
