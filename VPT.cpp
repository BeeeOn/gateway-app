/**
 * @file VPT.cpp
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Definition functions for VPTSensor
 */

#include <iomanip>

#include <cassert>

#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <Poco/DigestStream.h>
#include <Poco/RegularExpression.h>
#include <Poco/SHA1Engine.h>
#include <Poco/Thread.h>
#include <Poco/Timezone.h>

#include "main.h"
#include "VPT.h"
#include "Parameters.h"

using namespace std;
using namespace Poco;
using Poco::AutoPtr;
using Poco::DigestEngine;
using Poco::DigestOutputStream;
using Poco::Logger;
using Poco::RegularExpression;
using Poco::Runnable;
using Poco::TimerCallback;
using Poco::SHA1Engine;
using Poco::Util::IniFileConfiguration;


#define VPT_EUID_PREFIX 0xa4000000
#define SEND_RETRY 3

#define HEX_NUMBER 16

#define SUB_MAC_START 6
#define SUB_MAC_LENGTH 6

enum ACTION {
	PAIR = 0x01,
	READ,
	SET,
	LAST, //do not use as type of action
};

const string URL_PASSWORD_KEY = "&__HOSTPWD=";

const unsigned PRESSURE_MODULE_ID = 62;

const unsigned CONVERT_VALUE = 1000;

const bool ONLY_PAIRED = true;

static float convertBarToHectopascals(float bar) {
	return bar * CONVERT_VALUE;
}

static const string vpt_ini_file(void)
{
	return string(MODULES_DIR) + string(MOD_VPT_SENSOR) + ".ini";
}

static string getTimeZoneWithAction(const int action)
{
	stringstream stream;
	string result;
	int32_t zone_action = ((int32_t) Timezone::tzd() << 2) | action;

	stream << hex << uppercase << setfill('0') << setw(2) << zone_action;

	result = stream.str();

	return result.substr(result.length()-2); //Get last byte
}

static string generateBeeeonTimestamp(long long int adapter_id,
		uint32_t ip_address,
		const int action)
{
	stringstream stream;
	string result;

	uint32_t time = (uint32_t) Timestamp().epochTime();

	//Format: 4B - IP, 7B - ADAPTER IP, 4B - TIME, 1B - ZONE AND ACTION
	//Hex: 8 + 14 + 8 + 2 = 32 characters
	stream << hex << uppercase << setfill('0') <<
			setw(8) << ip_address <<
			setw(14) << adapter_id <<
			setw(8) << time;

	result = stream.str() + getTimeZoneWithAction(action);

	return result;
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

VPTSensor::VPTSensor(IOTMessage _msg, shared_ptr<Aggregator> _agg, long long int _adapter_id) :
	adapter_id(_adapter_id),
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
	http_client.reset(new HTTPClient(cfg->getUInt("port", 80)));

	listen_cmd_timer.setPeriodicInterval(0);
	listen_cmd_timer.setStartInterval(0);

	detect_devs_timer.setPeriodicInterval(0);
	detect_devs_timer.setStartInterval(0);
}

void VPTSensor::fetchAndSendMessage(map<euid_t, VPTDevice>::iterator &device)
{
	bool error = false;

	if (device->second.paired && device->second.active <= VPTDevice::INACTIVE)
		return;

	try {
		pair<bool, Command> response;
		if (createMsg(device->second)) {
			log.notice("VPT: Sending values to server");
			response = agg->sendData(msg);
			if (response.first) {
				parseCmdFromServer(response.second);
			}
			updateTimestampOnVPT(device->second, ACTION::READ);

			if (!device->second.paired)
				device->second.active -= device->second.wake_up_time;

		}
		else {
			log.error("Can't load new value of VPT sensor, send terminated");
			error = true;
		}
	}
	catch (Poco::Exception & exc) {
		log.error(exc.displayText());
		error = true;
	}

	if (error)
		device->second.active -= device->second.wake_up_time;
}

unsigned int VPTSensor::nextWakeup(void)
{
	unsigned int min_time = ~0;

	ScopedLock<Poco::Mutex> guard(devs_lock);
	for (auto it = map_devices.begin(); it != map_devices.end(); it++) {
		VPTDevice &device = it->second;

		if (device.time_left < min_time)
			min_time = device.time_left;
	}

	if (map_devices.empty())
		min_time = VPTDevice::DEFAULT_WAKEUP_TIME;

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

	initPairedDevices();

	devs_lock.lock();
	for (auto device = map_devices.begin(); device != map_devices.end(); device++)
		fetchAndSendMessage(device);
	devs_lock.unlock();

	vector<map<euid_t, VPTDevice>::iterator> delete_devices;
	while(!quit_global_flag) {
		delete_devices.clear();

		devs_lock.lock();
		for (auto it = map_devices.begin(); it != map_devices.end(); it++) {
			VPTDevice &device = it->second;

			if (device.time_left == 0) {
				fetchAndSendMessage(it);

				if (!device.paired && device.active <= VPTDevice::INACTIVE) {
					delete_devices.push_back(it);
					continue;
				}

				if (device.paired
					&& device.active <= VPTDevice::INACTIVE
					&& detect_devs_timer.getStartInterval() == 0)
				{
					detect_devs_timer.stop();
					detect_devs_timer.setStartInterval(VPTDevice::FIFTEEN_MINUTES);
					detect_devs_timer.start(TimerCallback<VPTSensor>(*this,
							&VPTSensor::checkPairedDevices));
				}

				device.time_left = device.wake_up_time;
			}

			assert(next_wakeup <= device.time_left);
			device.time_left -= next_wakeup;
		}
		devs_lock.unlock();

		deleteDevices(delete_devices);
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

euid_t VPTSensor::parseDeviceId(string &content)
{
	string id = json->getParameterValuesFromContent("id", content);
	euid_t euid = stoull(id.substr(SUB_MAC_START, SUB_MAC_LENGTH), nullptr, HEX_NUMBER);
	return VPT_EUID_PREFIX | (euid & EUID_MASK);
}

void VPTSensor::detectDevices(bool only_paired) {
	log.notice("VPT: Start device discovery");
	euid_t id;
	vector<string> devices = http_client->discoverDevices();
	VPTDevice device;
	device.active = VPTDevice::FIVE_MINUTES;

	ScopedLock<Mutex> guard(devs_lock);
	for (vector<string>::iterator it = devices.begin(); it != devices.end(); it++) {
		try {
			string content = http_client->sendRequest(*it);
			id = parseDeviceId(content);

			if (map_devices.find(id) != map_devices.end())
				device.paired = map_devices[id].paired;
			else
				device.paired = false;

			if (only_paired && !device.paired)
				continue;

			device.name = json->getParameterValuesFromContent("device", content);
			device.page_version = json->getParameterValuesFromContent("version", content);
			device.specification = device.name +"_"+ device.page_version;
			device.ip = *it;
			device.sensor.version = 1;
			device.sensor.euid = id;
			device.sensor.pairs = 0;
			device.sensor.values.clear();
			log.information("VPT: Detected device " + device.name + " with ip " + device.ip + " and version of web page " + device.page_version);
			map_devices[id] = device;
		}
		catch (...) {/* exceptions are cought in the caller */}
	}
	log.notice("VPT: Stop device discovery");
}

bool VPTSensor::isVPTSensor(euid_t euid) {
	return map_devices.find(euid) != map_devices.end();
}

void VPTSensor::initPairedDevices()
{
	Parameters parameter(*(agg.get()), msg, log);
	CmdParam result = parameter.getAllPairedDevices();
	VPTDevice device;

	if (!result.status)
		return;

	device.paired = true;

	try {
		ScopedLock<Mutex> guard(devs_lock);
		for (auto const& euid: result.getEuides(VPT_EUID_PREFIX)) {
			map_devices[euid] = device;
			log.information("Paired VPT with device euid: " + to_string(euid));
		}
	}
	catch (Poco::Exception& exc) {
		log.error("Initialization paired device failed with error: " + exc.displayText());
	}

	if (!map_devices.empty()) {
		detectDevices(ONLY_PAIRED);
		pairDevices();
	}
}

void VPTSensor::updateDeviceWakeUp(euid_t euid, unsigned int time)
{
	auto it = map_devices.find(euid);
	if (it == map_devices.end()) {
		log.warning("VPT: Setting wake up on unknown device " + to_string(euid));
		return;
	}

	VPTDevice &dev = it->second;

	if (time < VPTDevice::DEFAULT_WAKEUP_TIME)
		time = VPTDevice::DEFAULT_WAKEUP_TIME;

	dev.wake_up_time = time;
	dev.time_left = dev.wake_up_time;

	log.debug("VPT: Update resfresh time for device " + to_string(euid) + " on " + to_string(time) + " seconds");
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

	string url_value = json->generateRequestURL(dev.specification, value.first, value.second);
	if (url_value.empty()) {
		log.error("VPT: Setting actuator failed - device or actuator not found");
		return;
	}

	if (url_value.empty()) {
		log.error("VPT: Setting actuator failed - device or actuator not found");
		return;
	}

	if (sendSetRequest(dev, url_value))
		updateTimestampOnVPT(dev, ACTION::SET);

}

void VPTSensor::updateTimestampOnVPT(VPTDevice &dev, const int action) {
	string request = "/values.json?BEEE0=";

	if (action <= 0 || action >= ACTION::LAST) {
		log.warning("VPT: Generate timestamp with bad action code");
		return;
	}

	request += generateBeeeonTimestamp(adapter_id, http_client->findAdapterIP(dev.ip), action);
	log.debug("VPT: Set BeeeOn timestamp");
	sendSetRequest(dev, request);
}

bool VPTSensor::sendSetRequest(VPTDevice &dev, std::string url_value) {
	bool result = false;
	string request = url_value;
	if (!dev.password_hash.empty())
		request += URL_PASSWORD_KEY + dev.password_hash;

	std::string content;
	for(int i = 0; i < SEND_RETRY; i++) {
		try {
			content = http_client->sendRequest(dev.ip, request);
		}
		catch(Poco::Exception &exc) {
			log.error("VPT: " + exc.displayText());
		}

		if (!json->isJSONFormat(content)) {
			log.notice("VPT: Using password");
			dev.password_hash = buildPasswordHash(content);
			if (dev.password_hash.empty())
				continue;

			request = url_value + URL_PASSWORD_KEY + dev.password_hash;
		}
		else {
			result = true;
			break;
		}
	}

	return result;
}

void VPTSensor::processCmdListen(void)
{
	try {
		detectDevices();
		pairDevices();

		listen_cmd_timer.stop();
		listen_cmd_timer.setStartInterval(VPTDevice::THREE_MINUTES);
		listen_cmd_timer.start(TimerCallback<VPTSensor>(*this,
					&VPTSensor::checkPairedDevices));
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
		device.sensor.values = json->getSensors(website, device.specification);
	}
	catch (Poco::Exception & exc) {
		log.error("VPT: " + exc.displayText());
		return false;
	}

	convertPressure(device.sensor.values);
	device.sensor.pairs = device.sensor.values.size();
	device.sensor.device_id = json->getID(device.specification);
	msg.device = device.sensor;
	msg.time = time(NULL);
	return true;
}

void VPTSensor::convertPressure(vector<Value> &values) {
	for (auto itr = values.begin(); itr != values.end(); itr++) {
		if (itr->mid == PRESSURE_MODULE_ID) {
			itr->value = convertBarToHectopascals(itr->value);
			break;
		}
	}
}

void VPTSensor::checkPairedDevices(Timer& timer)
{
	bool find_devices_on_network = false;
	VPTDevice device;
	Parameters parameter(*(agg.get()), msg, log);
	CmdParam result = parameter.getAllPairedDevices();

	if (!result.status)
		return;

	setAllDevicesNotPaired();

	log.debug("Checking paired devices");

	device.paired = true;

	try {
		ScopedLock<Mutex> guard(devs_lock);

		for (auto const& euid: result.getEuides(VPT_EUID_PREFIX)) {
			map<euid_t, VPTDevice>::iterator it = map_devices.find(euid);
			if (it != map_devices.end()) {
				log.information("Device with euid " + to_string(euid)
						+ " set as paired");
				it->second.paired = true;

				if (it->second.active <= VPTDevice::INACTIVE)
					find_devices_on_network = true;

			}
			else {
				log.information("Device with euid " + to_string(euid)
						+ " was not detected on LAN in last 2 minutes");
				find_devices_on_network = true;
				map_devices[euid] = device;
			}
		}
	}
	catch (Poco::Exception& exc) {
		log.error("Checking paired devices failed with error: " + exc.displayText());
	}

	if (find_devices_on_network) {
		log.information("Trying detect paired devices, which can be detect in last "
				+ to_string((timer.getStartInterval() / 1000) / 60) + " minutes");
		detectDevices(ONLY_PAIRED);
		pairDevices();
	}

	log.debug("Checking paired devices is ending");
	timer.setStartInterval(0);
}

void VPTSensor::pairDevices(void) {
	vector<map<euid_t, VPTDevice>::iterator> devices_del;

	ScopedLockWithUnlock<Mutex> guard(devs_lock);
	for(auto device = map_devices.begin(); device != map_devices.end(); device++) {
		try {
			json->loadDeviceConfiguration(device->second.name, device->second.page_version);
			updateTimestampOnVPT(device->second, ACTION::PAIR);
		}
		catch (Poco::Exception &e) {

			if (!device->second.paired)
				devices_del.push_back(device);

			log.error("VPT: " + e.displayText());
		}
	}
	guard.unlock();

	deleteDevices(devices_del);
}

void VPTSensor::deleteDevices(vector<euid_t> euides)
{
	ScopedLock<Mutex> guard(devs_lock);
	for (auto euid: euides) {
		log.debug("Delete VPT with euid: " + to_string(euid));
		map_devices.erase(euid);
	}
}

void VPTSensor::setAllDevicesNotPaired()
{
	log.debug("Setting all devices as not paired");
	ScopedLock<Mutex> guard(devs_lock);
	for (auto& it: map_devices)
		it.second.paired = false;
}

void VPTSensor::deleteDevices(vector<map<euid_t, VPTDevice>::iterator> iterators)
{
	ScopedLock<Mutex> guard(devs_lock);
	for (const auto it: iterators) {
		log.debug("Delete VPT with euid: " + to_string(it->first));
		map_devices.erase(it);
	}
}
