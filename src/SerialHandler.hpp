#pragma once

#include <string>
#include <vector>
#include <stdexcept>

class SerialHandler {
public:
	SerialHandler(const std::string& path, int baudrate = 9600, size_t read_buf_sz = 1024);
	~SerialHandler() noexcept;

    class ReadTimeoutException : public std::runtime_error {
	public:
		ReadTimeoutException() : std::runtime_error("Read timed-out") {}
	};

    class WriteException : public std::runtime_error {
	public:
		WriteException() : std::runtime_error("Write failure") {}
	};

	std::vector<uint8_t> Read() const;
	void Write(const uint8_t* buf, size_t sz) const;

private:
	int _fd = -1;
	struct timespec _timeout = { 0, 100000000 };	// 100 ms
    
    size_t _read_buf_sz = 1024;
    unsigned char* _read_buf = nullptr;
};
