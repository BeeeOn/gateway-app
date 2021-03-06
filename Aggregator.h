/**
 * @file Aggregator.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef AGGREGATOR_H
#define	AGGREGATOR_H

extern bool quit_global_flag;

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

#include <Poco/AutoPtr.h>
#include <Poco/Event.h>
#include <Poco/Logger.h>
#include <Poco/Mutex.h>
#include <Poco/Net/SecureStreamSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Runnable.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "Belkin_WeMo.h"
#include "Bluetooth.h"
#include "MQTTDataModule.h"
#include "Distributor.h"
#include "LedModule.h"
#include "MosqClient.h"
#include "Parameters.h"
#include "PressureSensor.h"
#include "ServerConnector.h"
#include "TCP.h"
#include "VPT.h"
#include "VirtualSensorModule.h"
#include "XMLTool.h"
#include "TCP.h"
#include "JablotronModule.h"
#include "utils.h"

struct Cache_Key {

	Cache_Key() : time(0), priority(MSG_PRIO_HISTORY) {}
	Cache_Key(MSG_PRIO p, long long int t) : time(t), priority (p) {}

	bool operator<(const Cache_Key& key) const {
		// The most important is the highest priority
		if (priority > key.priority)
			return true;
		else if (priority < key.priority)
			return false;
		else {
			if (time < key.time)
				return true;
			else
				return false;
		}
	}

	bool operator>(const Cache_Key& key) const {
		if (priority < key.priority)
			return true;
		else if (priority > key.priority)
			return false;
		else {
			if (time > key.time)
				return true;
			else
				return false;
		}
	}

	long long int time;
	MSG_PRIO priority;
};


class PanInterface;
class VirtualSensorModule;
class Distributor;
class IOTReceiver;
class Parameters;
class Bluetooth;

/**
 * Class to watch invalid time. If there is no valid time, messages are not sent to the server and offset to blackout is computed. Correct time is computed after restoration of time.
 */
class TimeWatchdog : public Poco::Runnable {
	std::chrono::steady_clock::time_point system_start_point;
	std::chrono::steady_clock::time_point blackout_start_point;
	bool active;
	Aggregator* agg;

public:
	TimeWatchdog(std::chrono::steady_clock::time_point _start);

	bool isActive() { return active; }
	void setAgg(Aggregator* _agg) { agg = _agg; }

	long long int getOffset() {
		return (active ? std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - blackout_start_point).count() : 0);
	}

	void setBlackouStartPoint(std::chrono::steady_clock::time_point _bStart) {
		blackout_start_point = _bStart;
	}
	void run();
};

/**
 * Class for server communication. Sends data to server, stores and loads persistent cache. Distribution module is its part.
 */
class Aggregator: public Poco::Runnable {
public:
	Aggregator(IOTMessage _msg, std::shared_ptr<MosqClient> _mq);
	void run();
	std::pair<bool, Command> sendData(IOTMessage _msg);
	virtual ~Aggregator();

	void setVSM(std::shared_ptr<VirtualSensorModule> _vsm);
	void setPSM(std::shared_ptr<PressureSensor> _psm);
	void setLedModule(std::shared_ptr<LedModule> lm);
	void setPAN(std::shared_ptr<PanInterface> _pan);
	void setVPT(std::shared_ptr<VPTSensor> _vpt);
	void setTCP(std::shared_ptr<ServerConnector> _tcp);
	void setJablotronModule(std::shared_ptr<JablotronModule> jablotron);
	void setMQTTDataModule(std::shared_ptr<MQTTDataModule> mqtt_data_module);
	void setBluetooth(std::shared_ptr<Bluetooth> bluetooth);
	void setBelkinWemo(std::shared_ptr<Belkin_WeMo> belkinWemo);

	void storeCache();
	void loadCache(void);
	void parseCmd(Command cmd);
	void validateAllMessages(long long int now, long long int duration);
	static void buttonCallback(int event_type);

	void sendToPANviaMQTT(std::vector<uint8_t> msg);
	void sendFromMQTTDataModule(std::string msg, std::string topic);
	void sendToMQTTDataModule(std::string msg);
	void sendFromPAN(uint8_t type, std::vector<uint8_t> msg);
	float convertValue(TT_Module type, float old_val, bool reverse=false);

	void setAgg(std::shared_ptr<Aggregator> _agg);
	CmdParam sendParam(CmdParam param);

private:
	std::unique_ptr<Poco::FastMutex> cache_lock;
	std::multimap <Cache_Key, IOTMessage> cache;
	Poco::Logger& log;
	std::unique_ptr<Distributor> dist;

	std::shared_ptr<PressureSensor> psm;
	std::shared_ptr<VirtualSensorModule> vsm;
	std::shared_ptr<LedModule> ledModule;
	std::shared_ptr<PanInterface> pan;
	std::shared_ptr<VPTSensor> vpt;
	std::shared_ptr<ServerConnector> tcp;
	std::shared_ptr<Parameters> param;
	std::shared_ptr<JablotronModule> m_jablotron;
	std::shared_ptr<MQTTDataModule> m_mqtt_data_module;
	std::shared_ptr<Bluetooth> m_bluetooth;
	std::shared_ptr<Belkin_WeMo> m_belkinWemo;

	std::thread button_t;

	unsigned int cache_minimal_items;
	unsigned int cache_minimal_time;
	long long int cache_last_storing_time;
	std::string permanent_cache_path;

	IOTMessage msg_default;
	TimeWatchdog watchdog;
	std::shared_ptr<MosqClient> mq;

	void printCache(bool verbose);

};

#endif	/*AGGREGATOR_H */
