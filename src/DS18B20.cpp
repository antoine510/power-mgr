#include "DS18B20.hpp"

#include <unistd.h>
#include <fcntl.h>

/* taken from glibc unistd.h */
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
	({ long int __result;                                                     \
	do __result = (long int) (expression);                                 \
	while (__result == -1L && errno == EINTR);                             \
	__result; })
#endif

unsigned DS18B20::TakeMeasure() const {
    static char buf[16];
    int fd = TEMP_FAILURE_RETRY(::open(_path.c_str(), O_RDONLY));
    if(fd < 0) return 0;

    TEMP_FAILURE_RETRY(::read(fd, buf, sizeof(buf)));
    unsigned result = std::stoi(buf);

    ::close(fd);
    return result;
}
