/*
 * @file IODaemonMsg.cpp
 * @Author BeeeOn team - Richard Wolfert (wolfert.richard@gmail.com)
 * @date July 2016
 * @brief IO message representation class
 */

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/SocketAddress.h>

#include "IODaemonMsg.h"

#define MSG_ACTION "Action"
#define MSG_EVENT_NAME "EventName"
#define MSG_LEDS_BLUE "Blue"
#define MSG_LEDS_CONFIGURATION "LedsConfiguration"
#define MSG_LEDS_GREEN "Green"
#define MSG_LEDS_RED "Red"
#define MSG_PRIORITY "Priority"
#define MSG_SENDER "Sender"

using namespace std;
using Poco::JSON::Object;
using Poco::JSON::Parser;

IODaemonMsg::IODaemonMsg():
	m_priority(0),
	m_validity(false)
{
}

void IODaemonMsg::debug_print(Poco::Logger& log)
{
	log.debug(getEventName() + ", "
			+ getAction() + ", "
			+ getSenderName() + ", "
			+ to_string(getPriority()) + ", "
			+ to_string(isValid()) + ", "
			+ to_string(getLedConf().m_red) + ", "
			+ to_string(getLedConf().m_green) + ", "
			+ to_string(getLedConf().m_blue));
}

void IODaemonMsg::parse(const string &data, Poco::Logger& log)
{
	Object::Ptr parsedJson;
	try {
		parsedJson = parseJsonObject(data);
		setEventName(extractString(parsedJson, MSG_EVENT_NAME));
		setAction(extractString(parsedJson, MSG_ACTION));
		setSenderName(extractString(parsedJson, MSG_SENDER));
		setPriority(extractChar(parsedJson, MSG_PRIORITY));

		if (m_action == "setLeds")
			setLedConf(parseLedConfiguration(parsedJson, MSG_LEDS_CONFIGURATION));
		if (getEventName() == "updateLeds")
			setLedConf(parseLedConfiguration(parsedJson, MSG_LEDS_CONFIGURATION));
	}
	catch (Poco::Exception &e) {
		log.log(e, __FILE__, __LINE__);
		setValidity(false);
		return;
	}

	setValidity(true);
}

Object::Ptr IODaemonMsg::parseJsonObject(const string &data)
{
	Parser jsonParser;
	jsonParser.parse(data);
	Poco::Dynamic::Var parsedJsonResult = jsonParser.result();
	return parsedJsonResult.extract<Object::Ptr>();
}

string IODaemonMsg::extractString(const Object::Ptr &jsonObject, const string &key)
{
	Poco::Dynamic::Var item;
	item = jsonObject->get(key);
	return item.convert<string>();
}

unsigned char IODaemonMsg::extractChar(const Object::Ptr &jsonObject, const string &key)
{
	Poco::Dynamic::Var item;
	item = jsonObject->get(key);
	return item.convert<unsigned char>();
}

ledConfiguration IODaemonMsg::parseLedConfiguration(const Object::Ptr &jsonObject, const string &key)
{
	ledConfiguration ledConf;
	Object::Ptr jsonLedsConf;

	jsonLedsConf = jsonObject->getObject(key);

	ledConf.m_red = (bool)jsonLedsConf->get(MSG_LEDS_RED).convert<unsigned char>();
	ledConf.m_green = (bool)jsonLedsConf->get(MSG_LEDS_GREEN).convert<unsigned char>();
	ledConf.m_blue = jsonLedsConf->get(MSG_LEDS_BLUE).convert<unsigned char>();

	return ledConf;
}

/**
 * @brief Method for creating JSON message for iodaemon
 */
string IODaemonMsg::createDaemonMsg() const
{
	Poco::JSON::Object jsonMsg;
	Poco::JSON::Object jsonLedConf;

	jsonMsg.set(MSG_ACTION, m_action);
	jsonMsg.set(MSG_EVENT_NAME, m_eventName);
	jsonMsg.set(MSG_PRIORITY, m_priority);
	jsonMsg.set(MSG_SENDER, m_senderName);
	jsonLedConf.set(MSG_LEDS_RED, (int)m_ledConf.m_red);
	jsonLedConf.set(MSG_LEDS_GREEN, (int)m_ledConf.m_green);
	jsonLedConf.set(MSG_LEDS_BLUE, m_ledConf.m_blue);
	jsonMsg.set(MSG_LEDS_CONFIGURATION, jsonLedConf);

	Poco::Dynamic::Var jsonVar = jsonMsg;
	return jsonVar.convert<string>();
}
