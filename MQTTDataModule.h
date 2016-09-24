/**
 * @file MQTTDataModule.h
 * @Author BeeeOn team - Peter Tisovcik <xtisov00@stud.fit.vutbr.cz>
 * @date September, 2016
 */

#ifndef MQTT_DATA_MODULE_H
#define MQTT_DATA_MODULE_H

#include <memory>
#include <string>

#include "MQTTDataParser.h"
#include "ModuleADT.h"
#include "device_table.h"
#include "utils.h"

#define MQTT_DATA_MODULE_TO_ADAAPP        (std::string)"BeeeOn/MQTTDataModule/ToAdaApp"
#define MQTT_DATA_MODULE_FROM_ADAAPP      (std::string)"BeeeOn/MQTTDataModule/FromAdaApp"

class Aggregator;

/*
 * The class implements interface for module within the Adaapp. It uses mosquitto
 * for communication in JSON format. MQTTDataModule provides support for listening,
 * clearing and setting actuator messages from server and sending data to server
 * from sensor.
 */
class MQTTDataModule : public ModuleADT
{
	void threadFunction() override;

public:
	MQTTDataModule(IOTMessage msg, std::shared_ptr<Aggregator> agg);

	void parseCmdFromServer(const Command & cmd) override;

	/*
	 * Send message from modul to MQTT
	 * @param msg message
	 */
	void sendConfigCmd(std::string msg);

	/*
	 * Receive message from MQTT
	 * @param received message
	 */
	void msgToMQTTDataModule(std::string msg);

	/*
	 * Create pairing message
	 * {"state": "listen"}
	 * @return Pairing message
	 */
	std::string createPairingMessage() const;

	/*
	 * Create delete message
	 * {"device": { "euid": "" }, "state": "clean" }
	 * @return Delete message
	 */
	std::string createDeleteMessage(const euid_t& euid) const;

	/*
	 * Create message which set actuator
	 * {"state": "set", "euid": "", "data": [{"module_id": "", "value:": ""}]}
	 * @return Set Message
	 */
	std::string createSetActuatorsMessage(const Command& cmd) const;

private:
	IOTMessage m_msg;
};
#endif
