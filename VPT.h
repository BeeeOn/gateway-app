/**
 * @file VPT.h
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Class for devices from company Thermona, which use json outputs via HTTP
 */

#ifndef VPTSENSOR_H
#define VPTSENSOR_H

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Net/IPAddress.h>
#include <Poco/Runnable.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "device_table.h"
#include "HTTP.h"
#include "JSON.h"
#include "utils.h"
#include "XMLTool.h"

extern bool quit_global_flag;

struct str_device {
	std::string name;
	std::string ip;
};

class Aggregator;

class VPTSensor: public Poco::Runnable {
public:
	VPTSensor(IOTMessage _msg, std::shared_ptr<Aggregator>agg);

	virtual void run();

	void detectDevices();
	bool isVPTSensor(long long int sensor_id);
	void parseCmdFromServer(Command cmd);

private:
	std::shared_ptr<Aggregator> agg;
	std::unique_ptr<HTTPClient> http_client;
	std::unique_ptr<JSONDevices> json;
	Poco::Logger& log;
	IOTMessage msg;
	std::map<uint32_t, str_device> map_devices;
	Device sensor;
	TT_Table tt;
	unsigned int wake_up_time;

	bool createMsg(std::map<uint32_t, str_device>::iterator & device);
	void fetchAndSendMessage(std::map<uint32_t, str_device>::iterator &device);
	void pairDevices();
	uint32_t parseDeviceId(std::string &content);
	void updateDeviceWakeUp(long long int euid, unsigned int time);
	void processCmdSet(Command cmd);
	void processCmdListen(void);
};

#endif
