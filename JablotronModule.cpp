/**
 * @file JablotronModule.cpp
 * @Author BeeeOn team - Peter Tisovcik <xtisov00@stud.fit.vutbr.cz>
 * @date August, 2016
 * @brief Definition class functions for Jablotron
 */

#include <algorithm>
#include <sstream>

#include <unistd.h>

#include <Poco/Exception.h>
#include <Poco/NumberParser.h>
#include <Poco/RegularExpression.h>
#include <Poco/String.h>

#include "Aggregator.h"
#include "JablotronModule.h"

#define AWAITED_TOKEN_COUN          3
#define DELAY_AFTER_SET_SWITCH      2
#define DELAY_BEETWEEN_CYCLES       400000
#define DELAY_BETWEEN_PARSE         10
#define DEVICE_NAME                 1
#define FULL_BATTERY                100
#define JABLOTRON_MAINS_OUTLET      0
#define LOW_BATTERY                 5
#define MAX_DEVICES_IN_JABLOTRON    32
#define MAX_NUMBER_FAILED_REPEATS   10
#define MIN_MESSAGE_SIZE            5
#define NUMBER_OF_RETRIES           3
#define SET_STATE                   2
#define TIME_BETWEEN_RESET_EVENT    2
#define UNKNOWN_STATE               0
#define UNSET_STATE                 1

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Runnable;
using Poco::StringTokenizer;
using Poco::Util::IniFileConfiguration;

JablotronModule::JablotronModule(IOTMessage _msg, shared_ptr<Aggregator> _agg) :
	ModuleADT(_agg, "JablotronModule", MOD_JABLOTRON, _msg),
	msg(_msg)
{
	AutoPtr <IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(MODULES_DIR + MOD_JABLOTRON + ".ini");
		serialDeviceJablotron = cfg->getString(MOD_JABLOTRON + ".dongle_path",
		"/dev/turris_dongle");
	}
	catch (Poco::Exception & ex) {
		log.log(ex, __FILE__, __LINE__);
		inicialized = false;
	}

	try {
		serial.reset(new SerialControl(serialDeviceJablotron));
	}
	catch (std::exception& ex) {
		log.fatal("serial reset: " + (string)ex.what(), __FILE__, __LINE__);
		inicialized = false;
	}

	tt = fillDeviceTable();

	if (inicialized)
		log.information("Initialization succesful");
	else
		log.warning("Initialization failed");
}

void JablotronModule::threadFunction()
{
	Poco::RegularExpression re("([a-zA-Z0-9]+)");
	vector<string> splitMsg;
	TJablotron jablotron_msg;
	bool loadDevices = false;

	msg.state = "data";
	sensor.values.clear();

	while (!quit_global_flag) {
		splitMsg.clear();

		if (!loadJablotronDevices(loadDevices)) {
			sleep(DELAY_BETWEEN_PARSE);
			continue;
		}

		jablotron_msg.serialMessage = serial->sread();

		if (jablotron_msg.serialMessage.length() < MIN_MESSAGE_SIZE)
			continue;

		// OK or ERROR sending dongle whether the sent message is valid or not.
		// Ok Send when the dongle has nothing to send too.
		// Documentation: https://www.turris.cz/gadgets/manual#obecne_principy_komunikace
		Poco::replaceInPlace(jablotron_msg.serialMessage, string("\nOK\n"), string(""));
		Poco::replaceInPlace(jablotron_msg.serialMessage, "\nERROR\n", "");

		jablotron_msg.serialMessage = Poco::trim(jablotron_msg.serialMessage);

		Poco::StringTokenizer token(jablotron_msg.serialMessage, " ");
		if (!parseSerialMessage(jablotron_msg, token))
			continue;

		jablotron_msg.deviceID = getDeviceID(jablotron_msg.serialNumber);
		jablotron_msg.deviceName = token[DEVICE_NAME];

		if (!isJablotronDevice(jablotron_msg.serialNumber))
			continue;

		if (!sendMessageToServer(jablotron_msg, token))
			continue;
	}
}

/**
 * Your event has been set but it is necessary to off what is achieved by adding
 * value to sensorEvent.values. First, send a message with Eventim agg->sendData(msg),
 * you need to change to not have the same timestamp (+2).
 */
bool JablotronModule::sendMessageToServer(TJablotron &jablotron_msg,
	StringTokenizer &token)
{
	sensor.values.clear();
	sensor.euid = getEUID(jablotron_msg.serialNumber);
	sensor.device_id = jablotron_msg.deviceID;
	msg.time = time(NULL);
	sensorEvent = sensor;

	if (!parseMessageFromDevice(jablotron_msg, token))
		return false;

	msg.device = sensor;
	agg->sendData(msg);

	if (event) {
		usleep(DELAY_BEETWEEN_CYCLES);
		msg.device = sensorEvent;
		msg.time += TIME_BETWEEN_RESET_EVENT;
		agg->sendData(msg);
		event = false;
	}

	return true;
}

bool JablotronModule::parseMessageFromDevice(const TJablotron &jablotron_msg,
	StringTokenizer &token)
{
	try {
		if (jablotron_msg.deviceID == AC88) {
			sensor.values.push_back({AC88_ModuleID::SENSOR_STATE, convert(token[2], false)});
			return true;
		}

		int batterySensorID = 2;
		if (jablotron_msg.deviceID == JA85ST)
			batterySensorID = 3;

		sensor.values.push_back({batterySensorID, float(getBatteryStatus(token[3]))});

		switch (jablotron_msg.deviceID) {
		case JA81M:
		case JA83M:
			parseMessageFromJA81M_JA83M(token);
			break;
		case JA83P:
		case JA82SH:
			parseMessageFromJA83P_JA82SH(token);
			break;
		case JA85ST:
			parseMessageFromJA85ST(token);
			break;
		case RC86K:
			parseMessageFromRC86K(token[2]);
			break;
		case TP82N:
			parseMessageFromTP82N(jablotron_msg.serialMessage, token[2]);
			break;
		default:
			log.error("Unknown device");
			return false;
		}

		return true;
	}
	catch (Poco::Exception& ex) {
		log.log(ex, __FILE__, __LINE__);
		return false;
	}
}

bool JablotronModule::parseSerialMessage(TJablotron& jablotron_msg,
	StringTokenizer& token)
{
	if (token.count() < 3)
		return false;

	try {
		jablotron_msg.serialNumber = Poco::NumberParser::parse64(token[0].substr(1, 8));
	}
	catch (out_of_range& ex) {
		log.error("substr could not be performed" + (string)ex.what(), __FILE__, __LINE__);
		return false;
	}
	catch (Poco::Exception& ex) {
		log.log(ex, __FILE__, __LINE__);
		return false;
	}

	return true;
}

void JablotronModule::parseMessageFromJA85ST(const Poco::StringTokenizer& token)
{
	if (token[2] == "SENSOR") {
		sensor.values.push_back({JA85ST_ModuleID::FIRE_SENSOR, FIRE_SENSOR_VALUE::STATE_ON});
		sensorEvent.values.push_back({JA85ST_ModuleID::FIRE_SENSOR, FIRE_SENSOR_VALUE::STATE_OFF});
		event = true;
	}
	else if (token[2] == "BUTTON")
		sensor.values.push_back({JA85ST_ModuleID::FIRE_SENSOR, SENSOR_VALUE::STATE_ON});
	else if (token[2] == "TAMPER")
		sensor.values.push_back({JA85ST_ModuleID::FIRE_SENSOR_ALARM, convert(token[4])});
	else if (token[2] == "DEFECT")
		sensor.values.push_back({JA85ST_ModuleID::FIRE_SENSOR_ERROR, convert(token[4])});
}

void JablotronModule::parseMessageFromJA83P_JA82SH(const Poco::StringTokenizer& token)
{
	if (token[2] == "TAMPER")
		sensor.values.push_back({JA83P_JA82SH_ModuleID::SENSOR_ALARM, convert(token[4])});
	else if (token[2] == "SENSOR") {
		sensor.values.push_back({JA83P_JA82SH_ModuleID::SENSOR, SENSOR_VALUE::STATE_ON});
		sensorEvent.values.push_back({JA83P_JA82SH_ModuleID::SENSOR, SENSOR_VALUE::STATE_OFF});
		event = true;
	}
}

void JablotronModule::parseMessageFromJA81M_JA83M(const Poco::StringTokenizer& token)
{
	if (token[2] == "SENSOR")
		sensor.values.push_back({JA81M_JA83M_ModuleID::DOOR_CONTACT, convert(token[4])});
	else if (token[2] == "TAMPER")
		sensor.values.push_back({JA81M_JA83M_ModuleID::DOOR_CONTACT_ALARM, convert(token[4])});
}

void JablotronModule::parseMessageFromRC86K(const string& data)
{
	if (data == "PANIC") {
		sensor.values.push_back({RC86K_ModuleID::REMOTE_CONTROL_ALARM, SENSOR_VALUE::STATE_ON});
		sensorEvent.values.push_back({RC86K_ModuleID::REMOTE_CONTROL_ALARM, SENSOR_VALUE::STATE_OFF});
		event = true;
	}
	else {
		sensor.values.push_back({RC86K_ModuleID::REMOTE_CONTROL, convert(data)});
	}
}

void JablotronModule::parseMessageFromTP82N(const string& serialMessage,
	const string& data)
{
	Poco::StringTokenizer tokenIntSet(data, ":");
	string temperature;

	if (serialMessage.size() < 26) {
		log.error("message is unexpectedly short: " + to_string(serialMessage.size()),
			__FILE__, __LINE__);
		return;
	}

	temperature = serialMessage.substr(22, 4);

	if (tokenIntSet[0] == "SET")
		sensor.values.push_back({TP82N_ModuleID::REQUESTD_ROOM_TEMPERATURE,
			float(Poco::NumberParser::parseFloat(temperature))});
	else
		sensor.values.push_back({TP82N_ModuleID::CURRENT_ROOM_TEMPERATURE,
			float(Poco::NumberParser::parseFloat(temperature))});
}

bool JablotronModule::isJablotronDevice(jablotron_sn sn)
{
	for (jablotron_sn device : devList) {
		if (device == sn)
			return true;
	}

	return false;
}

bool JablotronModule::isJablotronModule(euid_t euid)
{
	for (jablotron_sn serialNumber : devList) {
		if (euid == getEUID(serialNumber))
			return true;
	}

	return false;
}

float JablotronModule::getValue(const string& msg) const
{
	Poco::StringTokenizer token(msg, ":");
	return Poco::NumberParser::parseFloat(token[1]);
}

float JablotronModule::convert(const string& data, bool reverse) const
{
	if (getValue(data) == 1)
		return UNSET_STATE;

	if (reverse)
		return SET_STATE;

	return UNKNOWN_STATE;
}

int JablotronModule::getBatteryStatus(const string& msg) const
{
	bool battery = getValue(msg);

	if (battery)
		return LOW_BATTERY;
	else
		return FULL_BATTERY;
}

bool JablotronModule::loadJablotronDevices(bool& loadDevices)
{
	if (!serial->isValidFd()) {
		if(!serial->initSerial()) {
			log.error("Failed to open Jablotron device errno: " + to_string(errno)
				+ " "+ __FILE__ + to_string(__LINE__));
			loadDevices = false;
			return false;
		}
	}

	if (!loadDevices) {
		if (loadRegDevices())
			loadDevices = true;
		else
			return false;
	}

	return true;
}

bool JablotronModule::loadRegDevices()
{
	string msg;
	vector<string> tmps;
	stringstream stream;
	jablotron_sn deviceSN;
	devList.clear();

	Poco::RegularExpression re("\\[([0-9]+)\\]");

	for (int i = 0; i < MAX_DEVICES_IN_JABLOTRON; i++) {
		// we need 2-digits long value (zero-prefixed when needed) - MAX_DEVICES_IN_JABLOTRON
		stream << setfill('0') << setw(2) << i;
		serial->ssend("\x1BGET SLOT:" + stream.str() + "\n");

		msg = serial->sread();
		if (re.split(msg, tmps) == 2) {
			try {
				deviceSN = Poco::NumberParser::parse64(tmps[1]);
			}
			catch(Poco::Exception& ex) {
				log.error("No conversion could be performed: " + ex.displayText());
				log.log(ex, __FILE__, __LINE__);
				return false;
			}

			devList.push_back(deviceSN);
			log.debug("device: " + to_string(deviceSN));
		}

		tmps.clear();
		stream.str("");
	}

	usleep(DELAY_BEETWEEN_CYCLES);

	for (jablotron_sn device : devList) {
		if (getDeviceID(device) == AC88) {
			while (!obtainActuatorState(getEUID(device)) && !quit_global_flag)
				sleep(1);
		}
	}

	return true;
}

int JablotronModule::getDeviceID(jablotron_sn sn) const
{
	if ((sn >= 0xCF0000) && (sn <= 0xCFFFFF))
		return AC88;

	if ((sn >= 0x580000) && (sn <= 0x59FFFF))
		return JA80L;

	if ((sn >= 0x240000) && (sn <= 0x25FFFF))
		return TP82N;

	if ((sn >= 0x1C0000) && (sn <= 0x1DFFFF))
		return JA83M;

	if ((sn >= 0x180000) && (sn <= 0x1BFFFF))
		return JA81M;

	if ((sn >= 0x7F0000) && (sn <= 0x7FFFFF))
		return JA82SH;

	if ((sn >= 0x640000) && (sn <= 0x65FFFF))
		return JA83P;

	if ((sn >= 0x760000) && (sn <= 0x76FFFF))
		return JA85ST;

	if ((sn >= 0x800000) && (sn <= 0x97FFFF))
		return RC86K;

	return 0;
}

jablotron_sn JablotronModule::getDeviceSN(euid_t euid) const
{
	for (jablotron_sn sn : devList) {
		if (euid == getEUID(sn))
			return sn;
	}

	return -1;
}

euid_t JablotronModule::getEUID(jablotron_sn sn) const
{
	return JABLOTRON_PREFIX + sn;
}

void JablotronModule::parseCmdFromServer(const Command& cmd)
{
	if (cmd.state == "set" && (getDeviceID(getDeviceSN(cmd.euid)) == AC88)) {
		for (size_t i = 0; i < cmd.values.size(); i++) {
			setSwitch(cmd.euid, cmd.values[i].second);
		}
	}
	else {
		log.error("Received invalid or unknown parameters command");
	}
}

/* Retransmission packet status is recommended to be done 3 times with
 * a minimum gap 200ms and 500ms maximum space (for an answer) - times
 * in the space, it is recommended to choose random.
 */
void JablotronModule::retransmissionPacket(const string& msg) const
{
	int maximum = 0;
	for (short i = 0; i < NUMBER_OF_RETRIES && maximum < MAX_NUMBER_FAILED_REPEATS; i++) {
		if (!serial->ssend(msg)) {
			maximum++;
			i--;
			continue;
		}

		log.debug("Changed:" + msg);
		usleep(DELAY_BEETWEEN_CYCLES);
	}

	sleep(DELAY_AFTER_SET_SWITCH);
}

void JablotronModule::setSwitch(euid_t euid, short sw)
{
	jablotron_sn ac88_sn_pgx = 0;

	for (euid_t device : devList) {
		if ((getDeviceID(device) == AC88) && (ac88_sn_pgx == 0))
			ac88_sn_pgx = device;
	}

	if (getEUID(ac88_sn_pgx) == euid)
		pgx = sw;
	else
		pgy = sw;

	retransmissionPacket("\x1BTX ENROLL:0 PGX:" + to_string(pgx) +
			" PGY:" + to_string(pgy) + " ALARM:0 BEEP:FAST\n");
}

bool JablotronModule::obtainActuatorState(euid_t euid)
{
	// Try to get state time from server
	log.trace("Getting last state");
	Parameters parameters(*agg.get(), msg, log);
	CmdParam request, answer;
	request.euid = euid;
	request.param_id = Parameters::GW_GET_DEV_MOD_LAST_VALUE;
	request.module_id = JABLOTRON_MAINS_OUTLET;

	answer = parameters.askServer(request);
	if (!answer.status) {
		log.warning("Could not retrieve latest state value from server, will try again");
		return false;
	}

	if (answer.module_id == JABLOTRON_MAINS_OUTLET) {
		try {
			int value = stoi(answer.value[0].first);
			setSwitch(euid, value);
		}
		catch(...) {
			log.error("Received invalid state value: " + answer.value[0].first);
			return false;
		}
	}
	else {
		log.notice("No latest state value found on server, setting default");
		setSwitch(euid, 0);
	}

	return true;
}
