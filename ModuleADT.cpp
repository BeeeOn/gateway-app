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
	using Poco::Util::IniFileConfiguration;

	try {
		cfg = new IniFileConfiguration(std::string(MODULES_DIR)+std::string(MODULE)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
		inicialized = false;
		return;
	}

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

/**
 * @brief Check if device with specific id is part of this module
 * can be overriden in specific child class.
 * @return true if it is
 */
bool ModuleADT::belongTo(euid_t sensor_id) {
	return (sensor.euid == sensor_id);
}

ModuleADT::~ModuleADT() {

}
