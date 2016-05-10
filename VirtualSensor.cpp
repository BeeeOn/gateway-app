/**
 * @file VirtualSensor.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include <Poco/NumberParser.h>
#include <Poco/ThreadPool.h>

#include "VirtualSensor.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::RegularExpression;
using Poco::Runnable;
using Poco::Util::IniFileConfiguration;

/**
 * Thread function for sending data to server (to not to block receiving thread)
 */
void processActuatorCommands(shared_ptr<std::queue<std::pair<int,float>>>actuator_request_queue,
			std::pair<shared_ptr<Aggregator>, Poco::Logger&> params,
			std::vector<std::shared_ptr<VirtualSensorValue>> vsc,
			shared_ptr<Poco::Mutex>queue_mutex)
{

	std::pair<int,float> actuator_value;
	while (!quit_global_flag) {
		shared_ptr<Aggregator> agg = params.first;
		Logger& log = params.second;
		bool hasRequest = false;

		if (quit_global_flag)
			break;

		queue_mutex->lock();

		if (!actuator_request_queue->empty()) {
			log.debug("THREAD ----> VirtualSensor queue is not empty - pop front item ");
			hasRequest = true;
			actuator_value = actuator_request_queue->front();
			actuator_request_queue->pop();
		}

		queue_mutex->unlock();

		if (hasRequest) {
			for (unsigned int i = 0; i < vsc.size(); i++) {
				std::shared_ptr<VirtualSensorValue> vs_val = vsc.at(i);
				if (vs_val && vs_val->module_id == actuator_value.first) {
					if(vs_val->is_actuator) {
						log.debug("THREAD ----> VirtualSensor received 'set' command for actuator module ID "+std::to_string(actuator_value.first));
						vs_val->setNewVal(actuator_value.second);
					}
					else {
						log.information("VirtualSensor received 'set' command for non-actuator module ID "+std::to_string(actuator_value.first)+" - ignore it");
					}
					break;
				}
			}
			continue;
		}
		sleep(1);
	}
}

VirtualSensor::VirtualSensor(AutoPtr<IniFileConfiguration> cfg, IOTMessage _msg, unsigned int sensor_num, shared_ptr<Aggregator> _agg) :
	wake_up_time(5),
	msg(_msg),
	agg(_agg),
	log(Poco::Logger::get("Adaapp-VS"))
{
	if (!initSensor(cfg, sensor_num))
		throw Poco::Exception("Init failure!");

	TT_Table tt = fillDeviceTable();
	auto dev = tt.find(sensor.device_id);
	if (dev == tt.end())
		throw Poco::Exception("Device ID is unknown!");

	unsigned int i = 1;

	std::map<int, TT_Module>::iterator it;

	while (!quit_global_flag) {
		try {
			int module_id = parseType(cfg->getString("Sensor_" + toStringFromInt(sensor_num) + ".module_id_" + toStringFromInt(i)));

			if (module_id < 0) {    // Failed to read sensor type
				i++;
				continue;
			}
			else if ((it = dev->second.modules.find(module_id)) == dev->second.modules.end()) {
				i++;
				log.warning("Module ID " + toStringFromHex(module_id) + " is unknown!");
				continue;
			}

			std::shared_ptr<VirtualSensorValue> vsv_tmp = parseRegexp(cfg->getString("Sensor_" + toStringFromInt(sensor_num) + ".value_" + toStringFromInt(i)), it->second.module_is_actuator);
			vsv_tmp->module_id = module_id;

			sensor.pairs++;

			if (it->second.module_is_actuator)
				log.information("Loaded sensor-actuator (Sensor_" + toStringFromInt(sensor_num) + ") with module_id " + toStringFromHex(vsv_tmp->module_id));
			else
				log.information("Loaded sensor (Sensor_" + toStringFromInt(sensor_num) + ") with module_id " + toStringFromHex(vsv_tmp->module_id));
			sensor.values.push_back({vsv_tmp->module_id, vsv_tmp->from});
			vsc.push_back(vsv_tmp);
			i++;
		}
		catch (Poco::Exception& ex) {
			if (i > 1) // Specified sensor were loaded
				break;
			else // There is no sensor, we don't need to start VS
				throw Poco::Exception("End");
		}
	}
}

/**
 * Algorithm to generate 4B EUID for VS based on adapter_id and order of VS.
 * @param adapter_id adapter_id
 * @param sensor_num Number of VS in configuration file
 * @return 4B EUID (hash)
 */
long long int getEUI(std::string adapter_id, unsigned int sensor_num) {
	std::string sensor_num_str = Poco::NumberFormatter::formatHex(sensor_num, 4);
	std::transform(sensor_num_str.begin(), sensor_num_str.end(), sensor_num_str.begin(), ::tolower);
	std::string key = adapter_id + sensor_num_str;

	uint32_t hash, i;
	for (hash = i = 0; i < key.length(); ++i) {
		hash += key.at(i);
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	hash = (((0xa3 << 24)) + (hash & 0xffffff)) & 0xffffffff;

	return (long long int)hash;
}

/**
 * Initialization of VS (fill with static values)
 * @retval success of the operation
 */
bool VirtualSensor::initSensor(AutoPtr<IniFileConfiguration> cfg, unsigned int sensor_num) {
	try {
		sensor.version = 1;
		sensor.device_id = cfg->getInt("Sensor_" + toStringFromInt(sensor_num) + ".device_id", 0);
		sensor.euid = getEUI(msg.adapter_id, sensor_num);
		sensor.pairs = 0;
	}
	catch (Poco::Exception& ex) {
		log.error("Exception when reading configuration file for Virtual Sensor");
		return false;
	}
	return true;
}

IOTMessage VirtualSensor::createMsg() {
	sensor.values.clear();

	for (unsigned int i=0; i < vsc.size(); i++) {
		sensor.values.push_back({vsc.at(i)->module_id, vsc.at(i)->actual_value});
	}
	sensor.pairs = sensor.values.size();

	msg.priority = MSG_PRIO_SENSOR;
	msg.device = sensor;
	msg.state = "data";
	msg.time = time(NULL);
	msg.offset = 0;

	return msg;
}

void VirtualSensor::run() {
	log.information("Running sensor " + toStringFromLongHex(sensor.euid) + " in time " + toStringFromLongInt(time(0)) + ", sleep for next " + toStringFromInt(wake_up_time) + "s.");

	queue_mutex.reset(new Poco::Mutex);
	actuator_request_queue.reset(new std::queue<std::pair<int,float> >);
	actuator_requests_thread.reset (new std::thread(processActuatorCommands, actuator_request_queue, std::pair<shared_ptr<Aggregator>, Poco::Logger&>(agg, log), vsc, queue_mutex));

	for (auto vsv : vsc) {
		if(vsv->generator_type != "initial")
		{
			log.debug("Starting timer for value generator, module_id = "+std::to_string(vsv->module_id));
			vsv->startTimer();
		}
		else {
			log.debug("Only actuator without value generator, module_id = "+std::to_string(vsv->module_id));
		}
	}

	sleep(1);

	while(!quit_global_flag) {
		log.information("Sending MSG from " + toStringFromLongHex(sensor.euid) + " in time " + toStringFromLongInt(time(0)) + ", sleep for next " + toStringFromInt(wake_up_time) + "s.");
		pair<bool, Command> response = agg->sendData(createMsg());
		if (response.first)
			parseCmdFromServer(response.second);

		for (unsigned int i = 0; i < wake_up_time; i++) {
			if (quit_global_flag)
				break;
			sleep(1);
		}
	}
	actuator_requests_thread->join();
	vsc.clear();
}

void VirtualSensor::parseCmdFromServer(Command cmd) {
	if (cmd.state == "update") {
		// Just for sure
		if (sensor.euid == cmd.euid) {
			wake_up_time = cmd.time;
			log.information("Setting new wake_up_time (" + toStringFromInt(cmd.time) + ") for sensor " + toStringFromLongHex(sensor.euid));
		}
	}
	else if (cmd.state == "set") {
		log.information("VirtualSensor (" + std::to_string(sensor.euid) + ") received 'set' command from server");

		// Just for sure
		if (sensor.euid == cmd.euid) {
			for (auto p : cmd.values) {
				queue_mutex->lock();
				log.information("Push set request to the queue for processing");
				actuator_request_queue->push(p);
				queue_mutex->unlock();
			}
		}
	}
}

int VirtualSensor::parseType(string type) {
	int ret = -1;
	if ((ret = toNumFromString(type)) >= 0)
		return ret;
	else {
		log.warning("Unsuported sensor type - you have to define type of senzor only by HEX number!!");
		return -1;
	}
}

std::shared_ptr<VirtualSensorValue> VirtualSensor::parseRegexp(string regexp_, bool is_actuator) {
	RegularExpression re("((linear|random|constant|triangle):)?(-?\\d+(\\.\\d+)?(->-?\\d+(\\.\\d+)?)?){1}(\\|step:-?\\d+(\\.\\d+)?)?(\\|time:\\d+(m|s|h){1})?");
	RegularExpression re1a("((linear|random|constant|triangle):)?(-?\\d+(\\.\\d+)?(->-?\\d+(\\.\\d+)?)?){1}(\\|step:-?\\d+(\\.\\d+)?)?(\\|time:\\d+(m|s|h){1})?(\\|switch_time:\\d+(m|s|h){1})?");
	RegularExpression re1b("(initial:)?(-?\\d+(\\.\\d+)?)(\\|switch_time:\\d+(m|s|h){1})?");

	string input(regexp_);

	if (is_actuator) {
		if (!re1a.match(input) && !re1b.match(input)) {
			log.error("Actuator regexp does not match: " + input);
			throw Poco::Exception("Actuator regexp does not match.");
		}
	}
	else {
		if (!re.match(input)) {
			log.error("Regexp does not match: " + input);
			throw Poco::Exception("Regexp does not match.");
		}
	}

	std::shared_ptr<VirtualSensorValue> v(new VirtualSensorValue());
	v->is_actuator = is_actuator;
	v->from = 0;
	v->change_time = 1;
	v->generator_type = "none";

	RegularExpression re2("(linear|random|constant|triangle|initial):");
	RegularExpression re3("(-?\\d+(\\.\\d+)?(->-?\\d+(\\.\\d+)?)?){1}");
	RegularExpression re3a("(-?\\d+(\\.\\d+)?)->");
	RegularExpression re3b("->-?\\d+(\\.\\d+)?");
	RegularExpression re4("(\\|step:-?\\d+(\\.\\d+)?)");
	RegularExpression re5("(\\|time:\\d+(m|s|h){1})");
	RegularExpression re6("(\\|switch_time:\\d+(m|s|h){1})");

	std::string ss;
	int position = 0;
	if ((re2.extract(input, position, ss)) && (ss != "")) {   // generator type - linear/random/constant/triangle
		position += ss.length();
		ss.erase(ss.end()-1); // remove colon
		v->generator_type = ss;
	}

	if ((re3.extract(input, position, ss)) && (ss != "")) {
		string ssa, ssb;
		position += ss.length();
		if (ss.find("->") != ss.npos) {         // If it is linear
			if (v->generator_type == "none")
				v->generator_type = "linear";
			else if (v->generator_type == "constant")
				throw Poco::Exception("Can't be constant while defining range.");
			else if (v->generator_type == "initial")
				throw Poco::Exception("Can't be initial while defining range.");

			re3a.extract(ss, ssa);
			re3b.extract(ss, ssb);
			ssa.erase(ssa.length()-2, ssa.length());
			ssb.erase(0, 2);
			v->from = (float) Poco::NumberParser::parseFloat(ssa);
			v->to = (float) Poco::NumberParser::parseFloat(ssb);
		}
		else {
			if (v->generator_type != "initial")
				v->generator_type = "constant";

			v->from = (float) Poco::NumberParser::parseFloat(ss);
			v->to = v->from;
		}
	}

	if ((re4.extract(input, position, ss)) && (ss != "")) {   // step parameter?
		position += ss.length();
		ss.erase(0,6);
		v->step = (float) Poco::NumberParser::parseFloat(ss);
	}
	else
		v->step = 1;

	if ((re5.extract(input, position, ss)) && (ss != "")) {  // time parameter?
		position += ss.length();
		ss.erase(0, 6);
		int multiplier = 1;

		if (ss.substr(ss.length()-1).compare("s") == 0) {
			multiplier = 1;
		}
		else if (ss.substr(ss.length()-1).compare("m") == 0) {
			multiplier = 60;
		}
		else if (ss.substr(ss.length()-1).compare("h") == 0) {
			multiplier = 60*60;
		}

		ss.erase(ss.length()-1);
		v->change_time =  Poco::NumberParser::parse(ss) * multiplier;
	}
	else {
		v->change_time = 1;
	}

	if ((re6.extract(input, position, ss)) && (ss != "")) {  // switch_time parameter?
		position += ss.length();
		ss.erase(0, 13);
		int multiplier = 1;

		if (ss.substr(ss.length()-1).compare("s") == 0) {
			multiplier = 1;
		}
		else if (ss.substr(ss.length()-1).compare("m") == 0) {
			multiplier = 60;
		}
		else if (ss.substr(ss.length()-1).compare("h") == 0) {
			multiplier = 60*60;
		}

		ss.erase(ss.length()-1);
		v->switch_time =  Poco::NumberParser::parse(ss) * multiplier;
	}
	else {
		v->switch_time = 0;
	}

	v->actual_value = v->from;

	return v;
}

VirtualSensor::~VirtualSensor() {
}
