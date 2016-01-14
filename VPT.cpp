/**
 * @file VPT.cpp
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Definition functions for VPTSensor
 */

#include "main.h"
#include "VPT.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Runnable;
using Poco::Util::IniFileConfiguration;

VPTSensor::VPTSensor(IOTMessage _msg, shared_ptr<Aggregator> _agg) :
	agg(_agg),
	log(Poco::Logger::get("Adaapp-VPT")),
	msg(_msg),
	wake_up_time(15)
{
	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(std::string(MODULES_DIR)+std::string(MOD_VPT_SENSOR)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
		exit (EXIT_FAILURE);
	}
	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/

	sensor.version = 1;
	sensor.pairs = 1;

	msg.state = "data";
	msg.priority = MSG_PRIO_SENSOR;
	msg.offset = 0;
	tt = fillDeviceTable();
	json.reset(new JSONDevices);
	http_client.reset(new HTTPClient);
}

/**
 * Main method of this thread.
 * Periodically send "data" messages with current pressure sensor value.
 *
 * Naturally end when quit_global_flag is set to true,
 * so the Adaapp is terminating.
 */
void VPTSensor::run(){
	detectDevices();
	pairDevices();
	while( !quit_global_flag ) {
		for (auto device = map_devices.begin(); device != map_devices.end(); device++ ) {
			try {
				pair<bool, Command> response;
				if (createMsg(device)) {
					log.information("VPT: Sending values to server");
					response = agg->sendData(msg);
					if (response.first) {
						parseCmdFromServer(response.second);
					}
				}
				else {
					log.error("Can't load new value of VPT sensor, send terminated");
				}
			}
			catch (Poco::Exception & exc) {
				log.error(exc.displayText());
			}
		}

		for (unsigned int i=0; i<wake_up_time; i++) {
			if (quit_global_flag)
				break;
			sleep(1);
		}
	}
}

#define VPT_ID_PREFIX 0xa4000000
#define VPT_ID_MASK   0x00ffffff

uint32_t VPTSensor::parseDeviceId(string &content)
{
	string id = json->getParameterValuesFromContent("id", content);
	uint32_t id32 = std::stoul(id.substr(5,11), nullptr, 16);
	return VPT_ID_PREFIX | (id32 & VPT_ID_MASK);
}

void VPTSensor::detectDevices(void) {
	log.information("VPT: Start device discovery");
	uint32_t id;
	vector<string> devices = http_client->discoverDevices();
	str_device device;
	for ( vector<string>::iterator it = devices.begin(); it != devices.end(); it++ ) {
		try {
			string content = http_client->sendRequest(*it);
			id = parseDeviceId(content);
			device.name = json->getParameterValuesFromContent("device", content);
			device.ip = *it;
			log.information("VPT: Detected device " + device.name + " with ip " + device.ip);
			map_devices.insert({id, device});
		}
		catch ( ... ) {/* exceptions are cought in the caller */}
	}
	log.information("VPT: Stop device discovery");
}

bool VPTSensor::isVPTSensor(long long int sensor_id) {
	return map_devices.find(sensor_id) != map_devices.end();
}

void VPTSensor::parseCmdFromServer(Command cmd){
	if (cmd.state == "update") {
		if (sensor.euid == cmd.euid) {
			if ( cmd.time < 15 ) {
				wake_up_time = 15;
			}
			else {
				wake_up_time = cmd.time;
			}
		}
		return;
	}
	else if (cmd.state == "set") {
		std::map<uint32_t, str_device>::iterator it_ptr;
		if ( (it_ptr = map_devices.find(cmd.euid)) != map_devices.end() ) {
			pair<int, float> value = cmd.values.at(0);
			string request_url = json->generateRequestURL(it_ptr->second.name, value.first, value.second);
			log.information("VPT: " + it_ptr->second.ip + ": Set actuator with ID:" + to_string(value.first) + " on " + to_string((int)value.second));
			http_client->sendRequest(it_ptr->second.ip, request_url);
		}
		return;
	}
	else if (cmd.state == "listen") {
		try {
			detectDevices();
			pairDevices();
		}
		catch ( Poco::Exception & exc ) {
			log.error("VPT: " + exc.displayText());
		}
		return;
	}
	log.error("Unexpected answer from server, received command: " + cmd.state);
}

bool VPTSensor::createMsg(map<uint32_t, str_device>::iterator & device) {
	string website = http_client->sendRequest(device->second.ip);
	sensor.euid = device->first;
	try {
		sensor.values = json->getSensors(website);
	}
	catch ( Poco::Exception & exc ) {
		log.error("VPT: " + exc.displayText());
		return false;
	}
	sensor.pairs = sensor.values.size();
	sensor.device_id = json->getID(device->second.name);
	msg.device = sensor;
	msg.time = time(NULL);
	return true;
}

void VPTSensor::pairDevices(void) {
	for( auto device = map_devices.begin(); device != map_devices.end(); device++ ) {
		try {
			json->loadDeviceConfiguration(device->second.name);
		}
		catch (Poco::Exception &e) {
			log.error("VPT: " + e.displayText());
		}
	}
}
