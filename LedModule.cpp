/**
 * @file LedModule.cpp
 * 9/2016 @Author BeeeOn team - Richard Wolfert (wolfert.richard@gmail.com)
 * @date 9/2016
 * @brief
 */

#include <iostream>
#include <fstream>

#include <Poco/Net/SocketAddress.h>

#include "Aggregator.h"
#include "LedModule.h"

#define LED_MODULE_DEVICE_ID 24
#define SET_LEDS_ACTION "setLeds"
#define SET_LEDS_EVENT "usrSetLeds"
#define USER_PRIORITY 50

#define MODULE_ID_RED 0x00
#define MODULE_ID_GREEN 0x01
#define MODULE_ID_BLUE 0x02

#define IODAEMON_INTERFACE_ADDRESS "localhost"

using namespace std;

#define BUFF_SIZE 512

LedModule::LedModule(IOTMessage _msg, shared_ptr<Aggregator> _agg) :
	ModuleADT(_agg, "Adaapp-LM", MOD_LED_MODULE, _msg)
{
	sensor.euid = getEUID(msg.adapter_id);
	sensor.pairs = 3;
	sensor.device_id = LED_MODULE_DEVICE_ID;
	ioDaemonPort = cfg->getInt("iodaemon.port", -1);
	receivePort = cfg->getInt("led_module.port", -1);
}

/**
 * Main method of this thread.
 * Exits when the quit_global_flag is set to true.
 */
void LedModule::threadFunction()
{
	Poco::Net::SocketAddress socketAddress(IODAEMON_INTERFACE_ADDRESS, receivePort);
	pair<bool, Command> response;

	log.information("Starting with configuration " + socketAddress.toString());
	socket = Poco::Net::DatagramSocket(socketAddress);
	socket.setReceiveTimeout(Poco::Timespan(5, 0));

	bool is_initialized = false;
	while(!quit_global_flag) {
		// we keep asking server and resending messages
		// to ioDaemon until we got answer "update" message
		// from ioDaemon, which means it is alive and understands.
		if (!is_initialized) {
			ioDaemonMsg = createDaemonMsg();
			ledConfiguration ledConf;
			if (recoverLastValue(ledConf)) {
				ioDaemonMsg.setLedConf(ledConf);
				sendToDaemon(ioDaemonMsg);
			}
		}

		IODaemonMsg answerFromDaemon;
		IODaemonMsg ioMessage = createDaemonMsg();
		answerFromDaemon = receiveFromDaemon();

		if (answerFromDaemon.isUpdate() && answerFromDaemon.isValid()) {
			// once we get update from iodaemon
			// there is no need to asking server for last value
			is_initialized = true;
			ioMessage.setLedConf(answerFromDaemon.getLedConf());
			createServerMsg(ioMessage);
			response = agg->sendData(msg);
			if (response.first)
				parseCmdFromServer(response.second);
		}
	}
}

/**
 * Function for generating 4B EUID from adapter_id
 * @param adapter_id adapter_id
 * @return 4B EUID (hash) of this module
 */
long long int LedModule::getEUID(std::string adapter_id)
{
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
	hash = (((0xa4 << 24)) + (hash & 0xffffff)) & 0xffffffff;

	return (long long int) hash;
}

/**
 * @brief Hanlde answers from server.
 * @param cmd command struct with incomming command.
 */
void LedModule::parseCmdFromServer(const Command& cmd)
{
	if (cmd.state == "update") { // do nothing
		log.debug("Received command 'update' from server, ignoring...");
		return;
	}

	if (cmd.state == "set") {
		log.debug("Received command 'set' from server");
		for (unsigned int i = 0; i < cmd.values.size(); i++) {
			ledConfiguration ledConf;

			switch (cmd.values[i].first) {
			case MODULE_ID_RED:
				ledConf = ioDaemonMsg.getLedConf();
				ledConf.m_red = (bool)cmd.values[i].second;
				ioDaemonMsg.setLedConf(ledConf);
				break;
			case MODULE_ID_GREEN:
				ledConf = ioDaemonMsg.getLedConf();
				ledConf.m_green = (bool)cmd.values[i].second;
				ioDaemonMsg.setLedConf(ledConf);
				break;
			case MODULE_ID_BLUE:
				ledConf = ioDaemonMsg.getLedConf();
				ledConf.m_blue = cmd.values[i].second;
				ioDaemonMsg.setLedConf(ledConf);
				break;
			default:
				log.error("Unknown module_id received in 'set' command, module_id: " + to_string(cmd.values[i].first));
			}
		}

		sendToDaemon(ioDaemonMsg);
		return;
	}

	log.error("Received unexpected command from server: " + cmd.state);
}

/**
 * @brief This will send message to ioDaemon
 */
void LedModule::sendToDaemon(const IODaemonMsg &ioMessage)
{
	string message = ioMessage.createDaemonMsg();
	Poco::Net::SocketAddress sa(IODAEMON_INTERFACE_ADDRESS, ioDaemonPort);
	Poco::Net::DatagramSocket dgs;

	dgs.connect(sa);
	dgs.sendBytes(message.data(), message.size());
}

/**
 * @brief Create message for server
 */
bool LedModule::createServerMsg(const IODaemonMsg &ioMessage)
{
	sensor.values.clear();
	sensor.values.push_back({MODULE_ID_RED,
					(float) ioMessage.getLedConf().m_red});
	sensor.values.push_back({MODULE_ID_GREEN,
					(float) ioMessage.getLedConf().m_green});
	sensor.values.push_back({MODULE_ID_BLUE,
					(float) ioMessage.getLedConf().m_blue});
	sensor.pairs = 3;

	msg.device = sensor;
	msg.time = time(NULL);
	return true;
}

/**
 * @brief Listen on its port for incomming messages
 */
IODaemonMsg LedModule::receiveFromDaemon()
{
	char buffer[BUFF_SIZE];
	int charsRead;
	IODaemonMsg message;

	Poco::Net::SocketAddress sender;
	try {
		charsRead = socket.receiveFrom(buffer, BUFF_SIZE-1, sender);
	}
	catch (Poco::TimeoutException e) {
		return IODaemonMsg();
	}

	buffer[charsRead] = '\0';

	log.debug("Received message: " + string(buffer));
	message.parse(string(buffer), log);

	return message;
}

/**
 * @brief Create Message for ioDaemon
 */
IODaemonMsg LedModule::createDaemonMsg()
{
	IODaemonMsg ioMessage;

	ioMessage.setEventName(SET_LEDS_EVENT);
	ioMessage.setAction(SET_LEDS_ACTION);
	ioMessage.setSenderName("Adaapp-LM");
	ioMessage.setPriority(USER_PRIORITY);
	ioMessage.setValidity(true);

	ledConfiguration ledConf;
	ledConf.m_red = false;
	ledConf.m_green = false;
	ledConf.m_blue = 0;
	ioMessage.setLedConf(ledConf);

	return ioMessage;
}

/*
 * @brief Method for recovering last configuration from server
 * @return true if recovery was successful
 */
bool LedModule::recoverLastValue(ledConfiguration &new_config)
{
	int value;
	if (!obtainLastValue(MODULE_ID_RED, value))
		return false;
	new_config.m_red = value;

	if (!obtainLastValue(MODULE_ID_GREEN, value))
		return false;
	new_config.m_green = value;

	if (!obtainLastValue(MODULE_ID_BLUE, value))
		return false;
	new_config.m_blue = value;

	return true;
}

/**
 * @brief Method for requesting last value of specific module from server
 * @param module_id id of requested module's last value
 * @param value Obtained value
 * @return if value has valid data
 */
bool LedModule::obtainLastValue(int module_id, int &value)
{
	log.information("Getting last actuator value from server");
	Parameters parameters(*(agg.get()), msg, log);
	CmdParam request, answer;
	request.euid = sensor.euid;
	request.param_id = Parameters::GW_GET_DEV_MOD_LAST_VALUE;
	request.module_id = module_id;
	answer = parameters.askServer(request);

	if (!answer.status) {
		log.warning("Could not retrieve latest actuator value from server, will try again");
		return false;
	}

	if (answer.module_id == module_id) {
		try {
			value = stoi(answer.value[0].first);
			log.information("Recovered value of module_id: "
					+ to_string(module_id) +", value: "+ to_string(value));
			return true;
		}
		catch(...) {
			log.error("Received invalid value: " + answer.value[0].first);
			return false;
		}
	}
	else {
		log.information("No latest value found on server, setting default");
		value = 0; //turn off all leds
		return true; // this is Ok, sensor is new so there are no latest data.
	}
}
