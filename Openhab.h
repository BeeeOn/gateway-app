/**
 * @file Openhab.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef OPENHAB_H
#define OPENHAB_H

#include <iostream>
#include <string>
#include <vector>

#include <cstdlib>//strtoul

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>

#include "utils.h"

struct Pack {
	long long int euid;
	uint32_t device_id;
	int module_id;
	float value;
};

class OpenHAB {
private:
	IOTMessage msg;
	Poco::Logger& log;
	Device sensor;

	long long int getEUI(std::string adapter_id);
	bool createMsg(Pack pack);
	uint32_t MACtoID(std::string mac);

public:
	OpenHAB(IOTMessage _msg);
	IOTMessage msgFromMqtt(std::string input);
	bool isOpenhabDevice(long long int dev_id);
	std::vector<std::string> cmdFromServer(Command cmd);

};

#endif //OPENHAB_H
