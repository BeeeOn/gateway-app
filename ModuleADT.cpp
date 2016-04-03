/**
 * @file ModuleADT.cpp
 * @Author Martin Cechmanek
 * @date December, 2015
 * @brief Abstract Data Type class for common interface for all modules
 */

#include "ModuleADT.h"

ModuleADT::ModuleADT(std::shared_ptr<Aggregator> agg_, std::string logger_name, std::string MODULE, IOTMessage msg_) :
	agg(agg_),
	log(Poco::Logger::get(logger_name)),
	msg(msg_)
{
	using Poco::AutoPtr;
	using Poco::Util::IniFileConfiguration;

	log.setLevel("trace"); // set default lowest level
	AutoPtr<IniFileConfiguration> cfg;

	try {
		cfg = new IniFileConfiguration(std::string(MODULES_DIR)+std::string(MODULE)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
		inicialized = false;
		return;
	}
	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/

	sensor.version = 1;
	sensor.pairs = 1;

	msg.state = "data";
	msg.priority = MSG_PRIO_SENSOR;
	msg.offset = 0;
	tt = fillDeviceTable();
	inicialized = true;
}

/**
 * @brief Starting point for module thread function.
 */
void ModuleADT::start() {
	if (inicialized) {
		module_thread = std::thread(&ModuleADT::threadFunction, this);
	}
}

/**
 * @brief End point of module thread.
 */
void ModuleADT::stop() {
	if (inicialized) {
		module_thread.join();
	}
}

ModuleADT::~ModuleADT() {

}
