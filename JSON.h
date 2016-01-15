/**
 * @file JSON.h
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Class for support json format
 */

#ifndef JSON_H
#define JSON_H


#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <Poco/AutoPtr.h>
#include <Poco/Exception.h>
#include <Poco/Logger.h>

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>

#include "utils.h"


/**
 * A structure represent options, for one value which is saved into converter
 */
struct converter_option {
	std::vector<std::string> options_name;
	std::vector<int> options_id;
};

/**
 * A structure represent incomplete description for one actuator (for full description is missing id)
 */
struct actuator_values {
	//@{
	std::string variable; /**< Variable name for HTTP GET request */
	std::shared_ptr<std::string> url; /**< URL path for HTTP requests */
	std::string name; /**< Actuator name is additional variable, which is used only for converter */
	std::shared_ptr<std::string> action; /**< Indicates special function, which is needed execute before sending value */
	//@}
};

/**
 * A structure represent device specification
 */
struct json_device {
	//@{
	int id;
	std::map<std::string,std::vector<std::tuple<std::string, int, std::string>>> sensors; /**<Group name contains sensors, which have parameters:
																								name, identification number, name  action */
	std::map<int, actuator_values> actuators; /**< Full description actuator with identification number */
	std::map<std::string, std::shared_ptr<converter_option>> converter; /**< Converter contains pair: supported name for sensor/actuator
																				and convert options */
	//@}
};


/**
 * Class represent devices, which have output in json format
 */
class JSONDevices {
public:
	JSONDevices();

	void loadDeviceConfiguration(std::string);

	std::string generateRequestURL(std::string device_name, int id, float value);

	int getID(std::string device_name);
	std::string getParameterValuesFromContent(std::string parameter, std::string content);
	std::vector<std::pair<int, float>> getSensors(std::string content, std::string device_name = "");

private:
	std::string devices_folder; ///< Folder contains files with description device
	std::map<std::string,json_device> devices; ///< Device name is mapped to structure, which contains device specification
	Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg; /**< Config file with additional options.
																Primary use: mapping names of devices to file with description device */
	Poco::Logger& log;
	Poco::JSON::Parser parser;


	std::string convertEnumToValue(std::string device_name, std::string value_name, int value);
	int convertValueToEnum(std::string device_name, std::string value_name, std::string value);

	void checkBaseFormat(Poco::DynamicStruct & jsonStruct);
	void checkFunctionExist(Poco::DynamicStruct & jsonStruct);

	Poco::DynamicStruct loadFile(std::string);

	void loadActuatorsConfiguration(Poco::DynamicStruct & jsonStruct, json_device * device_struct);
	void loadConverterConfiguration(Poco::DynamicStruct & jsonStruct, json_device * device_struct);
	void loadSensorsConfiguration(Poco::DynamicStruct & jsonStruct, json_device * device_struct);

	std::vector<std::string> loadFunctions(Poco::DynamicStruct & jsonStruct);
	std::vector<std::string> loadSensorsGroups(Poco::DynamicStruct & jsonStruct);
};
#endif
