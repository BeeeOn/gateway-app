/**
 * @file VPT.h
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Class for devices from company Thermona, which use json outputs via HTTP
 */

#ifndef VPTSENSOR_H
#define VPTSENSOR_H

#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <cstdlib>
#include <ctime>
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

extern bool quit_global_flag;

typedef struct VPTDevice {
	std::string name;
	std::string ip;
	std::string password_hash;
	Device sensor;
	unsigned int wake_up_time;
	unsigned int time_left;
} VPTDevice;

class Aggregator;

class VPTSensor: public Poco::Runnable {
public:
	VPTSensor(IOTMessage _msg, std::shared_ptr<Aggregator>agg);

	virtual void run();

	void detectDevices();
	bool isVPTSensor(euid_t sensor_id);
	void parseCmdFromServer(Command cmd);

private:
	std::shared_ptr<Aggregator> agg;
	std::unique_ptr<HTTPClient> http_client;
	std::unique_ptr<JSONDevices> json;
	Poco::Logger& log;
	IOTMessage msg;
	std::map<euid_t, VPTDevice> map_devices;
	std::string password;
	TT_Table tt;

	bool createMsg(VPTDevice &device);
	std::string buildPasswordHash(std::string content);
	void convertPressure(std::vector<Value> &values);
	unsigned int nextWakeup(void);
	void fetchAndSendMessage(std::map<euid_t, VPTDevice>::iterator &device);
	void pairDevices();
	euid_t parseDeviceId(std::string &content);
	void updateDeviceWakeUp(euid_t euid, unsigned int time);
	void processCmdSet(Command cmd);
	void processCmdListen(void);
};

#endif
