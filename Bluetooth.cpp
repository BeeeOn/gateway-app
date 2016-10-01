/**
 * @file Bluetooth.cpp
 * @Author BeeeOn team (MS)
 * @date October, 2016
 * @brief Availability monitoring Bluetooth devices
 */

#include <algorithm>

#include <Poco/Process.h>
#include <Poco/PipeStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/StringTokenizer.h>
#include <Poco/RegularExpression.h>

#include "Bluetooth.h"
#include "Parameters.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Mutex;
using Poco::Process;
using Poco::ProcessHandle;
using Poco::RegularExpression;
using Poco::StringTokenizer;
using Poco::trim;
using Poco::Util::IniFileConfiguration;

const int NUM_OF_ATTEMPTS = 8;
const unsigned int MIN_WAKE_UP_TIME = 15;

const long int BLUETOOTH_DEVICE_ID = 5;
const euid_t BT_EUID_PREFIX = 0xa600000000000000;
const int MAC_START = 6;
const int MAC_LENGTH = 12;

const float STATE_ON = 1.0;
const float STATE_OFF = 0.0;
const float STATE_UNKNOWN = -1.0;

const RegularExpression regex_mac("^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$");

Bluetooth::Bluetooth(IOTMessage msg, shared_ptr<Aggregator> agg) :
	ModuleADT(agg, "Bluetooth", MOD_BLUETOOTH, msg),
	m_wake_up_time(MIN_WAKE_UP_TIME),
	m_queries(NUM_OF_ATTEMPTS)
{
	sensor.device_id = BLUETOOTH_DEVICE_ID;

	AutoPtr <IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(MODULES_DIR + MOD_BLUETOOTH + ".ini");
		m_ifname = cfg->getString(MOD_BLUETOOTH + ".ifname", "hci0");
	}
	catch (Poco::Exception & ex) {
		log.log(ex, __FILE__, __LINE__);
		inicialized = false;
	}
}

void Bluetooth::parseCmdFromServer(const Command& cmd)
{
	if (cmd.state == "listen") {
		fullScan();
	}
	else if (cmd.state == "update") {
		if (!isInList(cmd.euid))
			return;

		if (cmd.time >= MIN_WAKE_UP_TIME) {
			log.notice("Wake_up_time adjusted to " + to_string(cmd.time), __FILE__, __LINE__);
			m_wake_up_time = cmd.time;
		}
		else {
			log.warning("Wake_up_time (" + to_string(cmd.time) + ") is too small", __FILE__, __LINE__);
		}

	}
	else if (cmd.state == "clean") {
		string mac = extractMAC(cmd.euid);
		if (mac.empty())
			return;
		log.notice("Remove item: " + mac, __FILE__, __LINE__);
		removeIfExist(mac);
	}
	else if (cmd.state == "parameters") {   // server will be send list of devices itself after every change
		if (cmd.params.status && cmd.params.param_id == Parameters::GW_GET_ALL_DEVS)
			listFromServer(cmd.params);
	}
}

void Bluetooth::threadFunction()
{
	log.information("Starting Bluetooth sensor thread...", __FILE__, __LINE__);

	if (turnOnDongle())
		log.information("Bluetooth dongle at " + m_ifname + " is ready", __FILE__, __LINE__);
	else
		log.error("Bluetooth dongle at " + m_ifname + " is not available", __FILE__, __LINE__);

	while (!quit_global_flag) {

		checkAndSendDevices();

		if (m_queries > 0) {
			log.debug("Get paired list from server for checking change", __FILE__, __LINE__);
			Parameters parameters = Parameters(*agg, msg, log);
			listFromServer(parameters.getAllPairedDevices());
			m_queries--;
		}

		for (unsigned int i = 0; i < m_wake_up_time; ++i) {
			if (quit_global_flag)
				break;
			sleep(1);
		}
	}
}

bool Bluetooth::turnOnDongle()
{
	log.notice("Turning on the dongle at " + m_ifname + " ...", __FILE__, __LINE__);
	string result = "";
	return (!exec(result, "hciconfig", m_ifname, "up"));
}

IOTMessage Bluetooth::createMsg(const string &mac, float value, const string &name)
{
	log.debug("creating message for: " + mac, __FILE__, __LINE__);

	Mutex::ScopedLock lock(m_mutex_msg);

	sensor.values.clear();
	sensor.values.push_back({0, value});
	sensor.euid = extractEUID(mac);
	sensor.name = name;

	IOTMessage msg_local = msg;
	msg_local.device = sensor;
	msg_local.time = time(NULL);

	return msg_local;
}

void Bluetooth::checkAndSendDevices()
{
	for (auto mac: m_MAC_list) {
		string result = "";
		IOTMessage iot_msg;

		if (exec(result, "hcitool", "name", mac)) {
			iot_msg = createMsg(mac, STATE_UNKNOWN);
			turnOnDongle();
		}
		else {
			if (result.size() > 0)
				iot_msg = createMsg(mac, STATE_ON);
			else
				iot_msg = createMsg(mac, STATE_OFF);
		}
		agg->sendData(iot_msg);

		if (quit_global_flag)
				break;
	}
}

int Bluetooth::exec(string &result, const string &first, const string &second, const string &third)
{
	vector<string> args;
	if (!second.empty())
		args.push_back(second);
	if (!third.empty())
		args.push_back(third);

	Mutex::ScopedLock lock(m_mutex_exec);

	Poco::Pipe outPipe;
	ProcessHandle ph = Process::launch(first, args, 0, &outPipe, 0);
	Poco::PipeInputStream istr(outPipe);
	Poco::StreamCopier::copyToString(istr, result);

	Process::requestTermination(ph.id());
	int exit_code = ph.wait();

	return exit_code;
}

void Bluetooth::sendNewDeviceToServer(const string &mac_address, const string &device_name)
{
	IOTMessage iot_msg;
	iot_msg = createMsg(mac_address, STATE_UNKNOWN, device_name);

	agg->sendData(iot_msg);
}

void Bluetooth::parseScanAndSend(const string &scan)
{
	const RegularExpression re("^(([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2})\t((.)+)$");

	vector<string> lines;
	split(lines, scan, "\n");

	for (auto line: lines) {
		line = trim(line);

		RegularExpression::MatchVec posVec;
		if (!re.match(line, 0, posVec))
			continue;

		const string mac_address = line.substr(posVec[1].offset, posVec[1].length);
		const string device_name = line.substr(posVec[3].offset, posVec[3].length);

		log.information("Found: " + mac_address + " " + device_name, __FILE__, __LINE__);
		sendNewDeviceToServer(mac_address, device_name);
	}
}


void Bluetooth::fullScan()
{
	log.information("Scaning Bluetooth network ...", __FILE__, __LINE__);

	string result = "";
	if (exec(result, "hcitool","scan")) {
		turnOnDongle();
		return;
	}

	parseScanAndSend(result);
	m_queries = NUM_OF_ATTEMPTS;
}

bool Bluetooth::hasMac(const string &mac)
{
	return find(m_MAC_list.begin(), m_MAC_list.end(), mac) != m_MAC_list.end();
}

bool Bluetooth::isInList(euid_t euid)
{
	string mac = extractMAC(euid);
	if (mac.empty())
		return false;

	return hasMac(mac);
}

string Bluetooth::insertColons(const string &input)
{
	string output = "";
	for (unsigned int i = 0; i < input.length(); ++i) {
		output += input[i];
		if (isEven(i + 1) && (i + 1) < input.length())
			output += ":";
	}
	return output;
}

euid_t Bluetooth::extractEUID(string mac)
{
	euid_t id = 0;
	mac = Poco::replace(mac, ":", "");

	try {
		id = std::stoull(mac.c_str(), NULL, HEX_NUMBER);
	}
	catch (std::exception& ex) {
		log.error("extractEUID: " + (string)ex.what(), __FILE__, __LINE__);
	}
	id = BT_EUID_PREFIX | (id & MAC_MASK);

	log.debug("Extracted EUID " + to_string(id) + " from MAC " + mac, __FILE__, __LINE__);
	return id;
}

string Bluetooth::extractMAC(euid_t euid)
{
	const euid_t mac_num = euid & MAC_MASK;
	const euid_t prefix_num = euid & EUID_PREFIX_MASK64;

	if (prefix_num != BT_EUID_PREFIX)
		return "";   // when it's not the bluetooth prefix

	string mac_str = toHexString(mac_num, "");

	mac_str = insertColons(mac_str);
	log.debug("converted EUID " + to_string(euid) + " to MAC " + mac_str, __FILE__, __LINE__);
	return mac_str;
}

void Bluetooth::split(vector<string> &output, const string &input, const string &div)
{
	StringTokenizer st(input, div);

	for (auto item: st)
		output.push_back(item);
}

void Bluetooth::resetQueries()
{
	m_queries = 0;
}

void Bluetooth::addIfValid(const string &mac)
{
	if (!mac.empty() && !hasMac(mac) && regex_mac.match(mac)) {
		m_MAC_list.push_back(mac);
		log.notice("Device " + mac + " added to list", __FILE__, __LINE__);
		resetQueries();
	}
}

void Bluetooth::removeIfExist(const string &mac)
{
	m_MAC_list.remove(mac);
}

void Bluetooth::listFromServer(const CmdParam &param)
{
	if (!param.status)
		return;

	if (param.value.size()) {
		for (auto item: param.value) {
			log.debug("Server answer: " + item.first, __FILE__, __LINE__);

			euid_t id_from_list = toNumFromString(item.first);
			string mac_from_list = extractMAC(id_from_list);

			addIfValid(mac_from_list);
		}
		log.debug("Server's answers done.", __FILE__, __LINE__);
	}
}
