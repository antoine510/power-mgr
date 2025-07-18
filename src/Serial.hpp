#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <mutex>

class Serial {
public:
	Serial(const std::string& path, int baudrate);
	~Serial() noexcept;

	class ReadTimeoutException : public std::runtime_error {
	public:
		ReadTimeoutException(const std::string& path) : std::runtime_error("Read timed-out for " + path) {}
	};

	class WriteException : public std::runtime_error {
	public:
		WriteException(const std::string& path) : std::runtime_error("Write failure for " + path) {}
	};

	auto SendCommandResponse(const uint8_t* command, size_t size, size_t responseSize = 0) const {
		std::scoped_lock lk(_serialMutex);
		Write(command, size);
		return Read(responseSize);
	}
	void SendCommand(const uint8_t* command, size_t size) const {
		std::scoped_lock lk(_serialMutex);
		Write(command, size);
	}

protected:
	bool WaitForData(int64_t timeout_ns = 100'000'000) const;
	std::vector<uint8_t> Read(size_t expectedSize) const;
	void Write(const uint8_t* buf, size_t sz) const;

	std::string _path;
	int _fd = -1;

	mutable std::mutex _serialMutex;
};
