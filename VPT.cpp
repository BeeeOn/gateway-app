/**
 * @file VPT.cpp
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Definition functions for VPTSensor
 */

#include <cassert>

#include <Poco/DigestStream.h>
#include <Poco/RegularExpression.h>
#include <Poco/SHA1Engine.h>

#include "main.h"
#include "VPT.h"

using namespace std;
using Poco::AutoPtr;
using Poco::DigestEngine;
using Poco::DigestOutputStream;
using Poco::Logger;
using Poco::RegularExpression;
using Poco::Runnable;
using Poco::SHA1Engine;
using Poco::Util::IniFileConfiguration;


#define VPT_DEFAULT_WAKEUP_TIME 15 /* seconds */
#define VPT_ID_PREFIX 0xa4000000
#define VPT_ID_MASK   0x00ffffff
#define SEND_RETRY 3

const string URL_PASSWORD_KEY = "&__HOSTPWD=";

static const string vpt_ini_file(void)
{
	return string(MODULES_DIR) + string(MOD_VPT_SENSOR) + ".ini";
}

static string extractRandomNumber(const string & content) {
	const static string prefix = "var randnum = ";
	string ret = "";
	RegularExpression regex(prefix + "([0-9]+)");
	RegularExpression::Match position;

	regex.match(content, 0, position);
	if (position.length > 0)
		ret = content.substr(position.offset+prefix.length(),position.length-prefix.length());

	return ret;
}

static string generateSHAHash(const string & password, const string & rand_number) {
	SHA1Engine sha1;
	DigestOutputStream str(sha1);

	str << rand_number + password;
	str.flush();

	const DigestEngine::Digest & digest = sha1.digest();
	return DigestEngine::digestToHex(digest);
}

VPTSensor::VPTSensor(IOTMessage _msg, shared_ptr<Aggregator> _agg) :
	agg(_agg),
	log(Poco::Logger::get("Adaapp-VPT")),
	msg(_msg)
{
	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(vpt_ini_file());
	}
	catch (Poco::Exception& ex) {
		log.fatal("Exception with config file reading:\n" + ex.displayText());
		exit (EXIT_FAILURE);
	}
	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/

	const string passwordKey(string(MOD_VPT_SENSOR)+".password");

	if (cfg->hasProperty(passwordKey)) {
		try {
			password = cfg->getString(passwordKey, "");
			log.notice("VPT: Using a shared password");
		}
		catch (...) {
			log.warning("VPT: Failed to load password (no password is used)");
			password = "";
		}
	} else {
		log.notice("VPT: No password is used");
		password = "";
	}

	msg.state = "data";
	msg.priority = MSG_PRIO_SENSOR;
	msg.offset = 0;
	tt = fillDeviceTable();
	json.reset(new JSONDevices);
	http_client.reset(new HTTPClient);
}

void VPTSensor::fetchAndSendMessage(map<long long int, VPTDevice>::iterator &device)
{
	try {
		pair<bool, Command> response;
		if (createMsg(device->second)) {
			log.notice("VPT: Sending values to server");
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

unsigned int VPTSensor::nextWakeup(void)
{
	unsigned int min_time = ~0;

	for (auto it = map_devices.begin(); it != map_devices.end(); it++) {
		VPTDevice &device = it->second;

		if (device.time_left < min_time)
			min_time = device.time_left;
	}

	if (map_devices.empty())
		min_time = VPT_DEFAULT_WAKEUP_TIME;

	return min_time;
}

/**
 * Main method of this thread.
 * Periodically send "data" messages with current pressure sensor value.
 *
 * Naturally end when quit_global_flag is set to true,
 * so the Adaapp is terminating.
 */
void VPTSensor::run(){
	unsigned int next_wakeup = 0;
	detectDevices();
	pairDevices();

	for (auto device = map_devices.begin(); device != map_devices.end(); device++)
		fetchAndSendMessage(device);

	while(!quit_global_flag) {
		for (auto it = map_devices.begin(); it != map_devices.end(); it++) {
			VPTDevice &device = it->second;

			if (device.time_left == 0) {
				fetchAndSendMessage(it);
				device.time_left = device.wake_up_time;
			}

			assert(next_wakeup <= device.time_left);
			device.time_left -= next_wakeup;
		}

		next_wakeup = nextWakeup();

		for (unsigned int i = 0; i < next_wakeup; i++) {
			if (quit_global_flag)
				break;
			sleep(1);
		}
	}
}

string VPTSensor::buildPasswordHash(std::string content) {
	string hash = "";
	string random_number = extractRandomNumber(content);
	log.debug("VPT: Random number: " + random_number);
	if (random_number.empty())
		return hash;

	try {
		hash = generateSHAHash(password, random_number);
	}
	catch (...) {
		log.warning("VPT: HASH: Generate hash failed");
	}

	log.information("VPT: HASH: " + hash);
	return hash;
}

long long int VPTSensor::parseDeviceId(string &content)
{
	string id = json->getParameterValuesFromContent("id", content);
	long long int id32 = stoul(id.substr(5,11), nullptr, 16);
	return VPT_ID_PREFIX | (id32 & VPT_ID_MASK);
}

void VPTSensor::detectDevices(void) {
	log.notice("VPT: Start device discovery");
	long long int id;
	vector<string> devices = http_client->discoverDevices();
	VPTDevice device;
	for (vector<string>::iterator it = devices.begin(); it != devices.end(); it++) {
		try {
			string content = http_client->sendRequest(*it);
			id = parseDeviceId(content);
			device.name = json->getParameterValuesFromContent("device", content);
			device.ip = *it;
			device.sensor.version = 1;
			device.sensor.euid = id;
			device.sensor.device_id = json->getID(device.name);
			device.sensor.pairs = 0;
			device.sensor.values.clear();
			device.wake_up_time = VPT_DEFAULT_WAKEUP_TIME;
			device.time_left = VPT_DEFAULT_WAKEUP_TIME;
			log.information("VPT: Detected device " + device.name + " with ip " + device.ip);
			map_devices.insert({id, device});
		}
		catch (...) {/* exceptions are cought in the caller */}
	}
	log.notice("VPT: Stop device discovery");
}

bool VPTSensor::isVPTSensor(long long int euid) {
	return map_devices.find(euid) != map_devices.end();
}

void VPTSensor::updateDeviceWakeUp(long long int euid, unsigned int time)
{
	auto it = map_devices.find(euid);
	if (it == map_devices.end()) {
		log.warning("VPT: Setting wake up on unknown device " + to_string(euid));
		return;
	}

	VPTDevice &dev = it->second;

	if (time < VPT_DEFAULT_WAKEUP_TIME)
		dev.wake_up_time = VPT_DEFAULT_WAKEUP_TIME;
	else
		dev.wake_up_time = time;

	dev.time_left = dev.wake_up_time;
}

void VPTSensor::processCmdSet(Command cmd)
{
	auto it = map_devices.find(cmd.euid);
	if (it == map_devices.end()) {
		log.error("VPT: Setting actuator on unknown device " + to_string(cmd.euid));
		return;
	}

	VPTDevice &dev = it->second;
	pair<int, float> value = cmd.values.at(0);
	log.information("VPT: " + dev.ip + ": Set actuator with ID:" + to_string(value.first) + " on " + to_string((int)value.second));

	string url_value = json->generateRequestURL(dev.name, value.first, value.second);
	if (url_value.empty()) {
		log.error("VPT: Setting actuator failed - device or actuator not found");
		return;
	}

	string request = url_value;
	if (url_value.empty()) {
		log.error("VPT: Setting actuator failed - device or actuator not found");
		return;
	}

	if (!dev.password_hash.empty())
		request += URL_PASSWORD_KEY + dev.password_hash;

	std::string content;
	for(int i = 0; i < SEND_RETRY; i++) {
		content = http_client->sendRequest(dev.ip, request);

		if (!json->isJSONFormat(content)) {
			log.notice("VPT: Using password");
			dev.password_hash = buildPasswordHash(content);
			if (dev.password_hash.empty())
				continue;

			request = url_value + URL_PASSWORD_KEY + dev.password_hash;
		}
		else {
			break;
		}
	}
}

void VPTSensor::processCmdListen(void)
{
	try {
		detectDevices();
		pairDevices();
	}
	catch (Poco::Exception & exc) {
		log.error("VPT: " + exc.displayText());
	}
}

void VPTSensor::parseCmdFromServer(Command cmd){
	if (cmd.state == "update") {
		updateDeviceWakeUp(cmd.euid, cmd.time);
		return;
	}
	else if (cmd.state == "set") {
		processCmdSet(cmd);
		return;
	}
	else if (cmd.state == "listen") {
		processCmdListen();
		return;
	}
	log.error("Unexpected answer from server, received command: " + cmd.state);
}

bool VPTSensor::createMsg(VPTDevice &device) {
	string website = http_client->sendRequest(device.ip);

	try {
		device.sensor.values = json->getSensors(website);
	}
	catch (Poco::Exception & exc) {
		log.error("VPT: " + exc.displayText());
		return false;
	}

	device.sensor.pairs = device.sensor.values.size();
	device.sensor.device_id = json->getID(device.name);
	msg.device = device.sensor;
	msg.time = time(NULL);
	return true;
}

void VPTSensor::pairDevices(void) {
	for(auto device = map_devices.begin(); device != map_devices.end(); device++) {
		try {
			json->loadDeviceConfiguration(device->second.name);
		}
		catch (Poco::Exception &e) {
			log.error("VPT: " + e.displayText());
		}
	}
}
