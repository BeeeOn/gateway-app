/**
 * @file Openhab.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include "Openhab.h"
#include "main.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Util::IniFileConfiguration;

OpenHAB::OpenHAB(IOTMessage _msg):
	msg(_msg),
	log(Poco::Logger::get("Adaapp-HAB"))
{
	log.setLevel("trace"); // set default lowest level

	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(std::string(MODULES_DIR)+std::string(MOD_OPENHAB)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
		exit (EXIT_FAILURE);
	}

	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/

	sensor.version = 1;
	sensor.euid = getEUI(msg.adapter_id);
	sensor.pairs = 1;
	msg.state = "data";
	msg.priority = MSG_PRIO_SENSOR;
	msg.offset = 0;
}

bool OpenHAB::isOpenhabDevice(long long int dev_id){	//Aggregator
	if(dev_id == 5){ return true; }
	else return false;
}

uint32_t OpenHAB::MACtoID(string mac){
	uint32_t id;
	if(mac.length() < 12){
		id = 0;
	} else {
		id = (0xa5000000 | (std::stoul(mac.substr(5,11), nullptr, 16) & 0x00FFFFFF));
	}
	return id;
}

// Message from OpenHAB to server.
IOTMessage OpenHAB::msgFromMqtt(string input){	// called from MosqClient
	if (input.compare("") == 0) {
		msg.time = 0;
		log.warning("Message from OpenHAB is empty,");
		return msg;
	}

	vector<string> output;
	string div = ";";

    size_t found = std::string::npos;
    while((found = input.find(div)) != std::string::npos){
        output.push_back(input.substr(0,found));
        input.erase(0,found+1);
    }
    output.push_back(input);
	Pack pack;
	for(unsigned i=0; i < (output.size()-1); i=i+2){
		if(output[i].compare("euid") == 0){
			pack.euid = MACtoID(output[i+1]);
		} else if(output[i].compare("device_id") == 0){
			pack.device_id = toIntFromString(output[i+1]);
		} else if(output[i].compare("module_id") == 0){
			pack.module_id = strtoul(output[i+1].c_str(), NULL, 16);
		} else if(output[i].compare("value") == 0){
			pack.value = toFloatFromString(output[i+1]);
		}
	}
	if(pack.euid <= 0 || pack.device_id <= 0) {
		log.error("Format of message from OpenHAB is wrong.");
		msg.time = 0;
		return msg;
	}

	if( createMsg(pack) ){
		return msg;
	} else { msg.time = 0; return msg; }
}

bool OpenHAB::createMsg(Pack pack){
	sensor.values.clear();
	sensor.values.push_back({pack.module_id, pack.value});
	sensor.pairs = 1;
	sensor.device_id = pack.device_id;
	sensor.euid = pack.euid;
	msg.device = sensor;
	msg.time = time(NULL);
	return true;
}

vector<string> OpenHAB::cmdFromServer(Command cmd){
	log.information("New CMD from server.");
	vector<string> output;

	if( ! isOpenhabDevice(cmd.device_id)) { return output; }

	if(cmd.state == "data"){
	string msg = "";

		if(cmd.device_id == 5){
			for (auto i: cmd.values) { // std::vector<std::pair<int, float> > values;
				msg = toStringFromHex(i.first) + "_" + toStringFromFloat(i.second);
				output.push_back(msg);
			}
		}
	} else if(cmd.state == "create"){
		// create necessary files for communication in openhab
	} else if(cmd.state == "remove"){
		// remove unnecessary items in openhab
	}
	return output;
}// cannot directly send to mqtt (cyclic dependency during compilation if #include "MosqClient.h")

/**
 * Function for generating 4B EUI of pressure sensor from adapter_id
 * @param adapter_id adapter_id
 * @return 4B EUI (hash) of pressure sensor
 * */
long long int OpenHAB::getEUI(string adapter_id) {
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
