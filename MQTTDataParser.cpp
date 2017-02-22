/**
 * @file MQTTDataParser.cpp
 * @Author BeeeOn team - Peter Tisovcik <xtisov00@stud.fit.vutbr.cz>
 * @date September, 2016
 */

#include <Poco/Dynamic/Var.h>

#include "MQTTDataParser.h"
#include "utils.h"

using namespace Poco::JSON;
using namespace std;
using Poco::AutoPtr;
using Poco::Util::IniFileConfiguration;

MQTTDataParser::MQTTDataParser():
	log(Poco::Logger::get("MQTTDataParser"))
{
}

IOTMessage MQTTDataParser::parseMessage(const string& data) const
{
	Device device;
	Object::Ptr parsedJson;
	IOTMessage message;

	try {
		parsedJson = parseJsonObject(data);

		message.state = extractString(parsedJson, "state");
		device.device_id = toIntFromString(extractString(parsedJson, "device_id"));
		device.euid = Poco::NumberParser::parseUnsigned64(extractString(parsedJson, "euid"));

		Array::Ptr dataArray = parsedJson->getArray("data");

		for (size_t i = 0; i < dataArray->size(); ++i) {
			Object::Ptr item = dataArray->getObject(i);
			string value = extractString(item, "value");
			string moduleID = extractString(item, "module_id");
			device.values.push_back({signed(Poco::NumberParser::parseHex(moduleID)),
				toFloatFromString(value)});
		}

		message.device = device;
		message.valid = true;
		return message;
	}
	catch (Poco::SyntaxException e) {
		log.error("Parsing message failed: " + (string)e.what());
		log.log(e, __FILE__, __LINE__);
		message.valid = false;
		return message;
	}
	catch (Poco::InvalidAccessException e) {
		log.error("Parsing message failed: missing an attribute");
		log.log(e, __FILE__, __LINE__);
		message.valid = false;
		return message;
	}
	catch (Poco::Exception e) {
		log.error("Parsing message failed: "+ (string)e.what());
		log.log(e, __FILE__, __LINE__);
		message.valid = false;
		return message;
	}
}

std::string MQTTDataParser::extractString(Poco::JSON::Object::Ptr jsonObject,
	const std::string& key) const
{
	Poco::Dynamic::Var item;
	item = jsonObject->get(key);

	return item.convert<string>();
}

Object::Ptr MQTTDataParser::parseJsonObject(const std::string& data) const
{
	Parser jsonParser;
	jsonParser.parse(data);
	Poco::Dynamic::Var parsedJsonResult = jsonParser.result();

	return parsedJsonResult.extract<Object::Ptr>();
}
