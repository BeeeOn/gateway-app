/**
 * @file MQTTDataParser.h
 * @Author BeeeOn team - Peter Tisovcik <xtisov00@stud.fit.vutbr.cz>
 * @date September, 2016
 */

#ifndef MQTT_DATA_PARSER_H
#define MQTT_DATA_PARSER_H

#include <iostream>
#include <string>

#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Logger.h>

#include "utils.h"

/*
 * The class impelements JSON parser for MQTTDataParser. Parses
 * JSON messages from mosquitto and mosquitto generates messages
 * that came from the server
 */
class MQTTDataParser {
public:
	MQTTDataParser();

	/*
	 * It parses JSON message from MQTT to IOTMessage struct.
	 * @param &data JSON message string
	 * @return Parsed message - IOTMessage struct
	 */
	IOTMessage parseMessage(const std::string& data) const;

private:
	Poco::Logger& log;

	/*
	 * It parses message and return JSON Object.
	 * @param &data JSON message string
	 * @return JSON Object
	 */
	Poco::JSON::Object::Ptr parseJsonObject(const std::string& data) const;

	/*
	 * Get string from JSON Object.
	 * @param jsonObject Object which contains parsed JSON message.
	 * @parma &key JSON attribute which contains value
	 * @return searched value
	 */
	std::string extractString(Poco::JSON::Object::Ptr jsonObject,
		const std::string& key) const;
};
#endif
