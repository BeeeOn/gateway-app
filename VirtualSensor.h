/**
 * @file VirtualSensor.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef VIRTUALSENSOR_H
#define	VIRTUALSENSOR_H

extern bool quit_global_flag;

#include <iostream>
#include <string>

#include <cstdlib>
#include <ctime>
#include <unistd.h>

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/RegularExpression.h>
#include <Poco/Runnable.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "PanInterface.h"
#include "utils.h"
#include "VirtualSensorValue.h"


class Aggregator;

/**
 * Class for virtual censor. Contains information about VS and its values.
 */
class VirtualSensor : public Poco::Runnable {
	private:
		unsigned int wake_up_time;
		IOTMessage msg;
		std::shared_ptr<Aggregator> agg;
		Poco::Logger& log;

		std::shared_ptr<std::thread> actuator_requests_thread;
		std::shared_ptr<Poco::Mutex> queue_mutex;
		std::shared_ptr<std::queue<std::pair<int,float> > > actuator_request_queue;

	public:
		Device sensor;          // Structure for conversion values from sensor to XML message
		std::vector<std::shared_ptr<VirtualSensorValue>> vsc;
		VirtualSensor(Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg, IOTMessage _msg, unsigned int sensor_num, std::shared_ptr<Aggregator> _agg);
		~VirtualSensor();

		bool initSensor(Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg, unsigned int sensor_num);
		int parseType(std::string type);
		std::shared_ptr<VirtualSensorValue> parseRegexp(std::string regexp, bool is_actuator);
		IOTMessage createMsg();

		void setWakeUpTime(long long int time) { wake_up_time = time; }
		void parseCmdFromServer(Command cmd);

		virtual void run();
};

#endif	/* VIRTUALSENSOR_H */
