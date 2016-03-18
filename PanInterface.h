/**
 * @file PanInterface.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef PANINTERFACE_H
#define	PANINTERFACE_H

extern bool quit_global_flag;

#include <algorithm>    // std::min
#include <chrono>
#include <deque>
#include <fstream>
#include <queue>

#include <cstdbool>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <Poco/AutoPtr.h>
#include <Poco/Event.h>
#include <Poco/File.h>
#include <Poco/Logger.h>
#include <Poco/Mutex.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/SocketStream.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "Aggregator.h"
#include "device_table.h"
#include "utils.h"

class Aggregator;
class SpiInterface;

/**
 * Class for receiving data from PAN coordinator. Message is parsed from bytes and packed to XML for sending. Requests from server are converted to a byte array and added ti TxThread queue.
 */
class PanInterface {
	std::shared_ptr<PanInterface> pan;
	IOTMessage msg;
	std::shared_ptr<Aggregator> agg;
	Poco::Logger& log;
	TT_Table tt;

public:
	PanInterface(IOTMessage _msg, std::shared_ptr<Aggregator> _agg);
	void sendCmd(std::vector<uint8_t>);
	void deleteDevice(euid_t);
	void setPairingMode();
	void setActuators(Command cmd);
	void sendCommandToPAN(Command cmd);

	int convertBattery(float bat_actual);
	void msgFromPAN(uint8_t msg_type, std::vector<uint8_t> data);

	void sendToPAN(std::vector<uint8_t> msg);

	TT_Module getModuleFromIDs(long int device_id, int module_id);
	void set_pan(std::shared_ptr<PanInterface> _pan);

};
	int theBigSwitch(int, Poco::Logger&); //TODO class

#endif	/* PANINTERFACE_H */
