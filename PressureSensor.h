/**
 * @file PressureSensor.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef PRESSURESENSOR_H
#define PRESSURESENSOR_H

extern bool quit_global_flag;

#include <ctime>
#include <iostream>
#include <memory>
#include <string>

#include <cstdlib>
#include <unistd.h>

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "device_table.h"
#include "utils.h"
#include "XMLTool.h"

#define PRESSURE_SENSOR_PATH "/sys/devices/platform/soc@01c00000/1c2b400.i2c/i2c-2/2-0077/pressure0_input"

class Aggregator;

class PressureSensor : public Poco::Runnable {
	private:
		unsigned int wake_up_time;
		IOTMessage msg;
		float pressureValue;
		std::shared_ptr<Aggregator> agg;
		Poco::Logger& log;
		TT_Table tt;
		Device sensor;

		long long int getEUI(std::string adapter_id);
		bool createMsg();
		bool refreshValue();

	public:
		PressureSensor(IOTMessage _msg, std::shared_ptr<Aggregator> _agg);
		virtual void run();
		bool isPressureSensor(euid_t sensor_id);
		void parseCmdFromServer(Command cmd);

};

#endif
