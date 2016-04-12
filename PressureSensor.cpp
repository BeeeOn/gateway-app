/**
 * @file PressureSensor.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include <iostream>
#include <fstream>

#include "main.h"
#include "PressureSensor.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Runnable;
using Poco::Util::IniFileConfiguration;

PressureSensor::PressureSensor(IOTMessage _msg, shared_ptr<Aggregator> _agg) :
	wake_up_time(5),
	msg(_msg),
	agg(_agg),
	log(Poco::Logger::get("Adaapp-PS"))
{
	log.setLevel("trace"); // set default lowest level

	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(std::string(MODULES_DIR)+std::string(MOD_PRESSURE_SENSOR)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
		exit (EXIT_FAILURE);
	}

	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/

	sensor.version = 1;
	sensor.euid = getEUI(msg.adapter_id);
	sensor.pairs = 2;
	sensor.device_id = 2;

	msg.state = "data";
	msg.priority = MSG_PRIO_SENSOR;
	msg.offset = 0;

	tt = fillDeviceTable();

}

/**
 * Main method of this thread.
 * Periodically send "data" messages with current pressure sensor value.
 *
 * Naturally end when quit_global_flag is set to true,
 * so the Adaapp is terminating.
 */
void PressureSensor::run(){

	log.information("Starting Pressure sensor thread...");

	while(!quit_global_flag) {
		pair<bool, Command> response;
		if (createMsg()) {
			response = agg->sendData(msg);
			if (response.first) {
				parseCmdFromServer(response.second);
			}
		}
		else {
			log.error("Can't load new value of pressure sensor, send terminated");
		}

		for (wake_up_counter = 0; wake_up_counter < wake_up_time; wake_up_counter++) {
			if (quit_global_flag)
				break;
			sleep(1);
		}
	}
}

/**
 * Check if the device is PS
 * @param sensor_id
 * @return true if they are equal
 */
bool PressureSensor::isPressureSensor(euid_t sensor_id) {
	return (sensor.euid == sensor_id);
}

/**
 * Function for generating 4B EUI of pressure sensor from adapter_id
 * @param adapter_id adapter_id
 * @return 4B EUI (hash) of pressure sensor
 */
long long int PressureSensor::getEUI(std::string adapter_id) {
	std::string key = adapter_id + "0001";
	uint32_t hash, i;

	for (hash = i = 0; i < key.length(); ++i) {
		hash += key.at(i);
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	hash = (((0xa2 << 24)) + (hash & 0xffffff)) & 0xffffffff;

	return (long long int) hash;
}

/**
 * Method create message with updated value from sensor.
 *
 * @return false if there was an error in creating message, else return true
 */
bool PressureSensor::createMsg() {
	sensor.values.clear();
	if (!refreshValue())
		return false;
	sensor.values.push_back({0, pressureValue});
	sensor.values.push_back({1, (float)wake_up_time});
	sensor.pairs = 2;
	msg.device = sensor;
	msg.time = time(NULL);
	return true;
}

/**
 * Method for refreshing value from sensor.
 *
 * @return false if there was an error in refreshing value (e.g: non-existent file)
 *  else return true
 */
bool PressureSensor::refreshValue() {
	std::fstream value_file(PRESSURE_SENSOR_PATH, std::ios_base::in);

	pressureValue = 0;
	value_file >> pressureValue;

	if (pressureValue==0)
		return false;
	else {
		pressureValue = pressureValue/100.0;
		return true;
	}
}

/**
 * Method for handling answer from server.
 * Can't contain anything, but "update" command.
 * Only log error if anything else will come.
 */
void PressureSensor::parseCmdFromServer(Command cmd){
	if (cmd.state == "update") { // do nothing
		return;
	}
	if (cmd.state == "set"){
		for( unsigned int i =0; i < cmd.values.size(); i++){
			if (cmd.values[i].first == 1) { //Change refresh time
				int new_wakeup = (int) cmd.values[i].second;
				if ((new_wakeup < 1) || (new_wakeup > 86400)){ // less than 1 second or more than 24 hours
					log.error("Incorrect wakeup time set: "+ std::to_string(new_wakeup)+" seconds!");
				}
				else{
					log.information("Changed refresh time to " + std::to_string(cmd.values[i].second) + "seconds" );
					wake_up_time = new_wakeup;
					wake_up_counter = new_wakeup;
				}
			} else
				log.error("Set unknown module id, module id = "+ std::to_string(cmd.values[i].first));
		}
	return;
	}
	log.error("Unexpected answer from server, received command: " + cmd.state);
}
