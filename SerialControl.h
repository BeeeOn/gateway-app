/**
 * @file SerialControl.h
 * @Author BeeeOn team - Peter Tisovcik <xtisov00@stud.fit.vutbr.cz>
 * @date August, 2016
 * @brief This class implements work with serial port. Opens and closes
 * the serial port reads and writes data to the serial port.
 */

#ifndef SERIAL_OPEN_MODULE_H
#define SERIAL_OPEN_MODULE_H

#include <string>

#include <Poco/Mutex.h>

class SerialControl {
public:
	SerialControl(std::string serialDevice);
	~SerialControl();

	bool isValidFd();
	bool initSerial();

	/*
	 * It is safe to call even after closure
	 */
	void closeSerial();

	/*
	 * This will send message to Dongle. Failed transmission
	 * must be handled at a higher layer.
	 * @param &data text to be sent
	 * @return If the message was sent successfully
	 */
	bool ssend(const std::string& data);

	/*
	 * This will read message from Dongle.
	 * @return Read message
	 */
	std::string sread();

private:
	Poco::Mutex _mutex;
	int m_serialFd;
	std::string m_serialDevice;

	int sopen();
};

#endif /*SERIAL_OPEN_MODULE_H*/
