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
#include "Parameters.h"

using namespace std;
using Poco::AutoPtr;

#define REFRESH_MODULE_ID 0x01
#define SECONDS_IN_DAY 86400

PressureSensor::PressureSensor(IOTMessage _msg, shared_ptr<Aggregator> _agg) :
	ModuleADT(_agg, "Adaapp-PS", MOD_PRESSURE_SENSOR, _msg),
	wake_up_time(5)
{
	sensor.euid = getEUI(msg.adapter_id);
	sensor.pairs = 2;
	sensor.device_id = 2;
	send_wake_up_flag = false;
}

/**
 * Main method of this thread.
 * Periodically send "data" messages with current pressure sensor value.
 *
 * Naturally end when quit_global_flag is set to true,
 * so the Adaapp is terminating.
 */
void PressureSensor::threadFunction(){

	log.information("Starting Pressure sensor thread...");
	//if obtaining failed, don't send refresh_time value
	send_wake_up_flag = obtainRefreshTime();

	while(!quit_global_flag) {
		//try to get refresh value again
		if (!send_wake_up_flag)
			send_wake_up_flag = obtainRefreshTime();
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
	if (send_wake_up_flag) {
		sensor.values.push_back({1, (float)wake_up_time});
		sensor.pairs = 2;
	}
	else {
		sensor.pairs = 1;
	}
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
 * @brief Method for handling answer from server.
 * Can't contain anything, but "update" or "set" command.
 * Only log error if anything else will come.
 * @param cmd command struct with incomming command from server and delivered to PS
 */
void PressureSensor::parseCmdFromServer(const Command& cmd){
	if (cmd.state == "update") { // do nothing
		return;
	}
	if (cmd.state == "set"){
		for( unsigned int i =0; i < cmd.values.size(); i++){
			if (cmd.values[i].first == 1) { //Change refresh time
				setNewRefresh(cmd.values[i].second);
			}
			else {
				log.error("Set unknown module id, module id = "+ std::to_string(cmd.values[i].first));
			}
		}
		return;
	}
	log.error("Unexpected answer from server, received command: " + cmd.state);
}

void PressureSensor::setNewRefresh(int refresh)
{

	if ((refresh < 1) || (refresh > SECONDS_IN_DAY)) {
		log.error("Incorrect wakeup time set: " + to_string(refresh) + " seconds");
	}
	else {
		log.information("Changed refresh time to " + to_string(refresh) + " seconds" );
		wake_up_time = refresh;
		wake_up_counter = refresh;
	}
}

bool PressureSensor::obtainRefreshTime()
{
	// Try to get refresh time from server
	log.information("Getting last refresh value from server.");
	Parameters parameters(*agg.get(), msg, log);
	CmdParam request, answer;
	request.euid = sensor.euid;
	request.param_id = Parameters::GW_GET_DEV_MOD_LAST_VALUE;
	request.module_id = REFRESH_MODULE_ID;
	answer = parameters.askServer(request);

	if (!answer.status) {
		log.warning("Could not retrieve latest refresh time value from server, will try again");
		return false;

	}

	if (answer.module_id == REFRESH_MODULE_ID) {
		try {
			int new_refresh = stoi(answer.value[0].first);
			setNewRefresh(new_refresh);
		}
		catch(...) {
			log.error("Received invalid refresh time value");
			log.error("Value of refresh time: "+answer.value[0].first);
			return false;
		}
	}
	else {
		log.information("No latest refresh value found on server, setting default");
		return true; // this is Ok, sensor is new so there are no latest data.
	}

	return true;
}
