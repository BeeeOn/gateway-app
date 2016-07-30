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
#include <Poco/Mutex.h>
#include <Poco/Net/IPAddress.h>
#include <Poco/Runnable.h>
#include <Poco/Timer.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "device_table.h"
#include "HTTP.h"
#include "JSON.h"
#include "utils.h"

extern bool quit_global_flag;

typedef struct VPTDevice {
	std::string name;
	std::string ip;
	std::string page_version;
	std::string specification;
	std::string password_hash;
	Device sensor;
	unsigned int wake_up_time;
	unsigned int time_left;
	int active;
	bool paired;

	// Enum for time interval
	enum {
		INACTIVE = 0,
		ACTIVE = 1,
		DEFAULT_WAKEUP_TIME = 15, // 15 seconds
		THREE_MINUTES = 3 * 60 * 1000, //3 min in milliseconds
		FIVE_MINUTES = 5 * 60, // 5 min in seconds
		FIFTEEN_MINUTES = 15 * 60 * 1000, //15 min in milliseconds
	};

	VPTDevice()
	{
		active = INACTIVE;
		paired = false;
		wake_up_time = DEFAULT_WAKEUP_TIME;
		time_left = DEFAULT_WAKEUP_TIME;
	}
} VPTDevice;

class Aggregator;

class VPTSensor: public Poco::Runnable {
public:
	VPTSensor(IOTMessage _msg, std::shared_ptr<Aggregator>agg, long long int _adapter_id);

	virtual void run();

	/**
	 * Detect devices on LAN. If only_paired is true, only known already paired
	 * devices are detected and the other devices are ignored.
	 */
	void detectDevices(bool only_paired = false);
	bool isVPTSensor(euid_t sensor_id);
	void parseCmdFromServer(Command cmd);

private:
	std::string adapter_id;
	std::shared_ptr<Aggregator> agg;
	std::unique_ptr<HTTPClient> http_client;
	std::unique_ptr<JSONDevices> json;
	Poco::Logger& log;
	Poco::Mutex devs_lock;
	/*
	 * other threads must always call timer.stop() prior to setting this
	 * interval to avoid races.
	 */
	Poco::Timer listen_cmd_timer;
	Poco::Timer detect_devs_timer;
	IOTMessage msg;
	std::map<euid_t, VPTDevice> map_devices;
	std::string password;
	TT_Table tt;

	bool createMsg(VPTDevice &device);
	std::string buildPasswordHash(std::string content);
	void convertPressure(std::vector<Value> &values);
	void checkPairedDevices(Poco::Timer& timer);
	void deleteDevices(std::vector<euid_t> euides);
	void deleteDevices(std::vector<std::map<euid_t, VPTDevice>::iterator> iterators);
	unsigned int nextWakeup(void);
	void fetchAndSendMessage(std::map<euid_t, VPTDevice>::iterator &device);
	void initPairedDevices();
	void pairDevices();
	euid_t parseDeviceId(std::string &content);
	void updateDeviceWakeUp(euid_t euid, unsigned int time);
	void updateTimestampOnVPT(VPTDevice &dev, const std::string &action);
	bool sendSetRequest(VPTDevice &dev, std::string url_value);
	void setAllDevicesNotPaired();
	void processCmdSet(Command cmd);
	void processCmdListen(void);
};

#endif
