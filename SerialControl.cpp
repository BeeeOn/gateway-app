/**
 * @file SerialControl.cpp
 * @Author BeeeOn team - Peter Tisovcik <xtisov00@stud.fit.vutbr.cz>
 * @date August, 2016
 * @brief Definition functions for SerialControl
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <unistd.h>

#include <fcntl.h>
#include <termios.h>

#include "SerialControl.h"

#define READ_TIMEOUT        30000
#define MAX_BUFF_SIZE       100
#define MAX_REOPEN_COUNT    10
#define DELAY_AFTER_OPEN    3

using Poco::Mutex;

SerialControl::SerialControl(std::string serialDevice):
	m_serialFd(-1),
	m_serialDevice(serialDevice)
{
}

bool SerialControl::isValidFd()
{
	return m_serialFd >= 0;
}

bool SerialControl::initSerial()
{
	m_serialFd = sopen();
	return isValidFd();
}

int SerialControl::sopen()
{
	struct termios newtermios;
	int fd;

	fd = open(m_serialDevice.c_str(), O_RDWR | O_NOCTTY);
	newtermios.c_cflag = CBAUD | CS8 | CLOCAL | CREAD;
	newtermios.c_iflag = IGNPAR;
	newtermios.c_oflag = 0;
	newtermios.c_lflag = 0;
	newtermios.c_cc[VMIN] = 1;
	newtermios.c_cc[VTIME] = 0;
	cfsetospeed(&newtermios, B57600);
	cfsetispeed(&newtermios, B57600);

	if (tcflush(fd, TCIFLUSH) == -1 || tcflush(fd, TCOFLUSH) == -1 ||
			tcsetattr(fd, TCSANOW, &newtermios) == -1) {
		closeSerial();
		return -1;
	}

	//Wait until the serial line ready for reading
	sleep(DELAY_AFTER_OPEN);
	return fd;
}

void SerialControl::closeSerial()
{
	if (!isValidFd())
		close(m_serialFd);

	m_serialFd = -1;
}

bool SerialControl::ssend(const std::string& data)
{
	Mutex::ScopedLock lock(_mutex);
	return write(m_serialFd, data.c_str(), data.length()) < 0;
}

std::string SerialControl::sread()
{
	fd_set fds;
	struct timeval timeout;
	int count = 0;
	int ret = -1;
	int  n = 0;
	char buffer[MAX_BUFF_SIZE + 1] = {0};
	int i = 0;

	Mutex::ScopedLock lock(_mutex);

	if (!isValidFd()) {
		m_serialFd = sopen();
	}

	timeout.tv_sec = 0;
	timeout.tv_usec = READ_TIMEOUT;

	do {
		FD_ZERO(&fds);
		FD_SET(m_serialFd, &fds);

		ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

		if (ret == 1) {
			n = read(m_serialFd, buffer + count, MAX_BUFF_SIZE -count);

			if (n <= 0) {
				closeSerial();

				if (i++ > MAX_REOPEN_COUNT)
					return "";
			}

			count += n;
			buffer[count] = '\0';
		}
	} while ((count < MAX_BUFF_SIZE) && (ret == 1));

	return std::string(buffer);
}

SerialControl::~SerialControl()
{
	closeSerial();
}
