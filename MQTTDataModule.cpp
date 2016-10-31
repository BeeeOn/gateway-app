/**
 * @file MQTTDataModule.cpp
 * @Author BeeeOn team - Peter Tisovcik <xtisov00@stud.fit.vutbr.cz>
 * @date September, 2016
 */

#include <ctime>

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>

#include "MQTTDataModule.h"
#include "MQTTDataParser.h"
#include "main.h"
#include "utils.h"

using namespace std;
using Poco::AutoPtr;

MQTTDataModule::MQTTDataModule(IOTMessage msg, shared_ptr<Aggregator> agg) :
	ModuleADT(agg, "MQTTDataModule", MOD_MQTT_DATA, msg),
	m_msg(msg)
{
}

/**
 * Main method of this thread.
 *
 * Naturally end when quit_global_flag is set to true,
 * so the Adaapp is terminating.
 */
void MQTTDataModule::threadFunction()
{
	log.information("Starting MQTTDataModule thread...");

	while(!quit_global_flag) {
		sleep(1);
	}
}

void MQTTDataModule::sendConfigCmd(std::string msg)
{
	agg->sendFromMQTTDataModule(msg, MQTT_DATA_MODULE_FROM_ADAAPP);
}

void MQTTDataModule::msgToMQTTDataModule(string msg_text)
{
	MQTTDataParser msgParser;
	IOTMessage iot = msgParser.parseMessage(msg_text);
	iot.time = time(NULL);
	iot.adapter_id = m_msg.adapter_id;

	if (!iot.valid) {
		log.error("Invalid msg for MQTTDataModule", __FILE__, __LINE__);
		return;
	}

	if (iot.state == "data")
		agg->sendData(iot);
	else
		log.error("Unknown message state for MQTTDataModule", __FILE__, __LINE__);
}

void MQTTDataModule::parseCmdFromServer(const Command& cmd)
{
	if (cmd.state == "listen") {
		log.debug("Send sensor pairing state command.");
		sendConfigCmd(createPairingMessage());
	}
	else if (cmd.state == "clean") {
		log.debug("Clean command to delete sensor.");
		sendConfigCmd(createDeleteMessage(cmd.euid));
	}
	else if (cmd.state == "set") {
		log.debug("Set actuator value(s) comand");
		sendConfigCmd(createSetActuatorsMessage(cmd));
	}
	else if (cmd.state == "error") {
		log.error("Error state in command -> Nothing to send via MQTT",
			__FILE__, __LINE__);
	}
	else {
		log.error("Unknown state of incomming command from server! Command state = "
			+ cmd.state, __FILE__, __LINE__);
	}
}

string MQTTDataModule::createPairingMessage() const
{
	Poco::JSON::Object jsonMsg;
	jsonMsg.set("state", "listen");
	Poco::Dynamic::Var jsonVar = jsonMsg;

	return jsonVar.convert<std::string>();
}

string MQTTDataModule::createDeleteMessage(const euid_t& euid) const
{
	Poco::JSON::Object jsonMsg;
	Poco::JSON::Object data;

	data.set("euid", euid);
	jsonMsg.set("device", data);
	jsonMsg.set("state", "clean");
	Poco::Dynamic::Var jsonVar = jsonMsg;

	return jsonVar.convert<std::string>();
}

string MQTTDataModule::createSetActuatorsMessage(const Command& cmd) const
{
	Poco::JSON::Object jsonMsg;
	Poco::JSON::Object sensorValues;
	Poco::JSON::Object::Ptr data;
	Poco::JSON::Array arrayPtr;

	jsonMsg.set("state", "set");
	jsonMsg.set("device_id", std::to_string(cmd.device_id));
	jsonMsg.set("euid", std::to_string(cmd.euid));

	size_t i = 0;
	for (auto item : cmd.values) {
		data = new Poco::JSON::Object();

		data->set("module_id", to_string(item.first));
		data->set("value", to_string(item.second));

		arrayPtr.set(i++, data);
		jsonMsg.set("data", arrayPtr);
	}

	Poco::Dynamic::Var jsonVar = jsonMsg;
	return jsonVar.convert<std::string>();
}
