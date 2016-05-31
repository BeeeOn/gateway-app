/**
 * @file Openhab.cpp
 * @Author BeeeOn team (MS)
 * @date April, 2016
 * @brief
 */

#ifndef OPENHAB_H
#define OPENHAB_H

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream> // hex to string
#include <string>
#include <vector>

#include <cstdlib>	//strtoul

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "device_table.h"
#include "utils.h"
#include "XMLTool.h"

extern bool quit_global_flag;

const std::string topic = "BeeeOn/openhab/bt";

struct Pack {
	long long int euid;
	uint32_t device_id;
	int module_id;
	float value;
	std::string name;
};

class Aggregator;

class OpenHAB: public Poco::Runnable {
public:
	OpenHAB(IOTMessage _msg, std::shared_ptr<Aggregator> _agg);
	virtual void run();

	IOTMessage msgFromMqtt(std::string input);
	bool isOpenhabDevice(long long int dev_id);
	bool cmdFromServer(Command cmd);

private:
	std::shared_ptr<Aggregator> agg;
	Device sensor;
	Poco::Logger& log;
	IOTMessage msg;
	int checktime;
	std::vector< std::string > local_list;

	bool createMsg(Pack pack);
	long long int MACtoID(std::string mac);
	std::string IDtoMAC(long long int ide);
	void checkNews();

};

#endif //OPENHAB_H
