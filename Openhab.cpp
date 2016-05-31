/**
 * @file Openhab.cpp
 * @Author BeeeOn team (MS)
 * @date April, 2016
 * @brief
 */

#include "Openhab.h"
#include "main.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Runnable;
using Poco::Util::IniFileConfiguration;

OpenHAB::OpenHAB(IOTMessage _msg, std::shared_ptr<Aggregator> _agg) :
	agg(_agg),
	log(Poco::Logger::get("Adaapp-HAB")),
	msg(_msg)
{
	log.setLevel("trace"); // set default lowest level

	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_OPENHAB)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("OH: Exception with config file reading:\n" + ex.displayText());
		exit (EXIT_FAILURE);
	}
	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/

	msg.state = "data";
	msg.priority = MSG_PRIO_SENSOR;
	msg.offset = 0;

	checktime = 0;
}

bool OpenHAB::isOpenhabDevice(long long int dev_id){	//Aggregator
	if(dev_id == 5){ return true; }
	else return false;
}

long long int OpenHAB::MACtoID(string mac){
	long long int id;
	if(mac.length() < 12){
		id = 0;
	} else {
		id = (0xa5000000 | (std::stoul(mac.substr(5,11), nullptr, 16) & 0x00FFFFFF));
	}
	return id;
}

string OpenHAB::IDtoMAC(long long int ide){	// only last 6 (hex)numbers!

	if(ide < 1000000000){ return "0"; }

	std::stringstream sstream;
	sstream << std::hex << ide;
	string mac = sstream.str().substr(2,8);

	return mac;
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
		} else if(output[i].compare("name") == 0){
			pack.name = (output[i+1]);
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
	sensor.name = pack.name;
	msg.device = sensor;
	msg.time = time(NULL);
	return true;
}

bool OpenHAB::cmdFromServer(Command cmd){
	string mac = "";

	if(cmd.state == "listen") { // iwannalistofbluetooth
		log.information("Server wants BT list, state: " + cmd.state);
		agg->sendToMQTT("iwannalistofbluetooth", topic);
		this->checktime = 120;
	}
	else if(cmd.state == "clean"){ // removedevicefromlist;00ABCDEF0123
		log.information("Request for remove device, state: " + cmd.state);
		mac = IDtoMAC(cmd.euid);
		agg->sendToMQTT( ("removedevicefromlist;"+mac), topic);
		for(unsigned i=0; i < this->local_list.size(); i++){
			if (this->local_list[i] == mac){
				this->local_list.erase(this->local_list.begin()+i);
				break;
			}
		}
	}

	return true;
}

void OpenHAB::checkNews(){
	CmdParam param;
	param.param_id = 1003;
	log.information("Check what is new = get paired list");
	param = agg->sendParam(param);
	if (! param.status)
		return;

	if (param.value.size()){
		for(auto item: param.value){
			log.information("Server answer: " + item.first + " typ = " + item.first.substr(0,4));
			if (item.first.find("0xa5") != std::string::npos){
				string polo_mac = item.first.substr(4,8);
				log.information("polo_mac = " + polo_mac);
				if ( find(this->local_list.begin(), this->local_list.end(), polo_mac) != this->local_list.end() ){
					//log.information("This is already in local_list: " + polo_mac);
				}
				else {
					this->local_list.push_back(polo_mac);
					log.information("local_list.push_back() and send to MQTT = adddevicetolist;" + polo_mac);
					agg->sendToMQTT( ("adddevicetolist;"+polo_mac), topic);
					this->checktime = 0;
				}
			}
		}
		log.information("Server's answers done.");
	}
}

/**
 * Main method of this thread.
 *
 * Naturally end when quit_global_flag is set to true,
 * so the Adaapp is terminating.
 */
void OpenHAB::run(){
	log.information("OH: Starting OpenHAB thread...");
	checkNews();	// get list of paired devices
	while(!quit_global_flag) {
		sleep(1);
		if (this->checktime > 0)
			this->checktime--;
		if ((this->checktime % 15) == 1){
			checkNews();	//ask the server
		}
	}
}
