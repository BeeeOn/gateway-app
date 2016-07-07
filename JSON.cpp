/**
 * @file JSON.h
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Definition functions for class JSONDevices
 */


#include <algorithm>
#include <array>
#include <fstream>
#include <streambuf>

#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>

#include "JSON.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;


JSONDevices::JSONDevices(void) : log(Poco::Logger::get("Adaapp-VPT")) {
	devices_folder = MODULES_DIR + "json_devices/";
	cfg = new Poco::Util::IniFileConfiguration(std::string(MODULES_DIR)+std::string(MOD_JSON)+".ini");
}

void JSONDevices::loadDeviceConfiguration(string device_name, string page_version) {
	if (page_version.empty())
		page_version = "default";

	string device_file = cfg->getString(device_name+"."+page_version);
	device_name += "_" + page_version;
	log.debug("JSON: Loading device specification for " + device_name);
	Poco::DynamicStruct jsonStruct = loadFile(devices_folder+device_file);
	checkBaseFormat(jsonStruct);

	json_device device_struct;
	device_struct.id = jsonStruct["device_id"].extract<int>();
	vector<string> functions = loadFunctions(jsonStruct);

	if (std::find(functions.begin(), functions.end(), "sensors") != functions.end())
		loadSensorsConfiguration(jsonStruct, &device_struct);

	if (std::find(functions.begin(), functions.end(), "actuators") != functions.end())
		loadActuatorsConfiguration(jsonStruct, &device_struct);

	if (std::find(functions.begin(), functions.end(), "converter") != functions.end())
		loadConverterConfiguration(jsonStruct, &device_struct);

	devices.insert({device_name, device_struct});
	log.information("JSON: Successfully loaded device specification for " + device_name);
}

bool JSONDevices::isJSONFormat(std::string content) {
	bool state;
	Poco::JSON::Parser parser;

	try {
		parser.parse(content);
		state = true;
	}
	catch(...) {
		state = false;
	}

	return state;
}

std::string JSONDevices::generateRequestURL(string device_name, int id, float value) {
	string request_url = "";
	std::map<std::string,json_device>::iterator it = devices.find(device_name);

	if (it == devices.end())
		return request_url;

	std::map<int, actuator_values>::iterator actuator_it = it->second.actuators.find(id);

	if (actuator_it == it->second.actuators.end())
		return request_url;

	request_url = "/" + *(actuator_it->second.url) + "?" + actuator_it->second.variable + "=";
	if (actuator_it->second.action == NULL) {
		std::ostringstream ss;
		ss << value;
		request_url += std::string(ss.str());
	}
	else {
		if (actuator_it->second.action->compare("convert") == 0) {
			int value_enum = (int) value;
			request_url += convertEnumToValue(device_name, actuator_it->second.name, value_enum);
		}
	}

	return request_url;
}

int JSONDevices::getID(std::string device_name) {
	return devices.find(device_name)->second.id;
}

string JSONDevices::getParameterValuesFromContent(std::string parameter, std::string content) {
	Poco::JSON::Parser parser;
	Poco::DynamicStruct jsonStruct = *(parser.parse(content).extract<Poco::JSON::Object::Ptr>());
	return jsonStruct[parameter].toString();
}

vector<Value> JSONDevices::getSensors(std::string content, std::string device_name) {
	vector<Value> values;
	map<string, json_device>::iterator device_itr;
	json_device * device_info;
	Poco::JSON::Parser parser;

	log.debug("JSON: Get sensors values");
	Poco::DynamicStruct jsonStruct = *(parser.parse(content).extract<Poco::JSON::Object::Ptr>());

	if (device_name.empty())
		device_name = jsonStruct["device"].toString() + "_" + jsonStruct["version"].toString();

	if ((device_itr = devices.find(device_name)) == devices.end())
		Poco::Exception("JSON: Specification " + device_name + " not found");

	device_info = &(device_itr->second);
	string help_value;
	for (auto & it : device_info->sensors) {
		for (auto & sensor: it.second) {
			float number = numeric_limits<float>::infinity();
			help_value = jsonStruct["sensors"][it.first][get<0>(sensor)].toString();
			if (get<2>(sensor).empty()) {
				std::replace(help_value.begin(), help_value.end(), ',', '.');
				try {
					number = stof(help_value);
				}
				catch (...) { }
			}
			else if (! get<2>(sensor).compare("convert")) {
				try {
					number = (float) convertValueToEnum(device_name, get<0>(sensor), help_value);
				}
				catch (Poco::Exception & exc) {
					cout << exc.displayText() << endl;
				}
			}
			log.debug("JSON:\tID: " + to_string(get<1>(sensor)) + ", Value: " + to_string((int) number));

			if (number ==  numeric_limits<float>::infinity())
				values.push_back({get<1>(sensor), number, false});
			else
				values.push_back({get<1>(sensor), number});

		}
	}
	return values;
}


string JSONDevices::convertEnumToValue(string device_name, string value_name, int value) {
	shared_ptr<converter_option> options = devices.find(device_name)->second.converter.find(value_name)->second;
	for (unsigned index = 0; index < options->options_id.size(); index++) {

		if (options->options_id.at(index) == value)
			return options->options_name.at(index);

	}
	throw Poco::Exception("JSON: Can't find convert value " + value_name + " for device " + device_name);
}

int JSONDevices::convertValueToEnum(string device_name, string value_name, string value) {
	shared_ptr<converter_option> options = devices.find(device_name)->second.converter.find(value_name)->second;
	for (unsigned index = 0; index < options->options_name.size(); index++) {

		if (options->options_name.at(index).compare(value) == 0)
			return options->options_id.at(index);

	}
	throw Poco::Exception("JSON: Can't find convert value " + value_name + " for device " + device_name);
}

void JSONDevices::checkBaseFormat(Poco::DynamicStruct & jsonStruct) {
	vector<string> base_parameters = {"vendor", "device", "functions"};
	for (vector<string>::iterator it = base_parameters.begin(); it != base_parameters.end(); it++) {

		if (!jsonStruct.contains(*it))
			throw Poco::Exception("JSON: Invalid json format missing \"" + *it + "\"\n");

	}
	checkFunctionExist(jsonStruct);
}

void JSONDevices::checkFunctionExist(Poco::DynamicStruct & jsonStruct) {
	vector<string> functions = loadFunctions(jsonStruct);
	if (functions.empty()) {
		throw Poco::Exception("JSON: Invalid json format for vendor \""
							  + jsonStruct["vendor"] + "\" and device \"" + jsonStruct["device"] + "\"\n" +
							  "Device doesn't contains any functions\n",0);
	}
	for (vector<string>::iterator it = functions.begin(); it != functions.end(); it++) {
		if (!jsonStruct.contains(*it)) {
			throw Poco::Exception("JSON: Invalid json format for vendor \"" + jsonStruct["vendor"] +
								  "\" and device \"" + jsonStruct["device"] + "\"\n" +
								  "Device doesn't contains function \"" + *it + "\"\n",0);
		}
	}
}

Poco::DynamicStruct JSONDevices::loadFile(string file_path) {
	filebuf json_file;
	Poco::JSON::Parser parser;
	Poco::DynamicStruct jsonStruct;
	json_file.open(file_path, std::ios::in);

	if (!json_file.is_open())
		throw Poco::Exception("JSON: Can't open file for device \"" + file_path + "\"\n");

	istream is(&json_file);
	jsonStruct = *(parser.parse(is).extract<Poco::JSON::Object::Ptr>());
	json_file.close();
	return jsonStruct;
}

void JSONDevices::loadActuatorsConfiguration(Poco::DynamicStruct & jsonStruct, json_device * device_struct) {
	actuator_values actuator;
	map<string, shared_ptr<string> > remap_string;
	map<string, shared_ptr<string> >::iterator it;
	vector<void *> help_pointers = {&(actuator.action),&(actuator.url)};
	for (int index = 0, count = jsonStruct["actuators"].size(); index < count; index++) {
		actuator.variable = jsonStruct["actuators"][index]["variable"].toString();
		if (jsonStruct["actuators"][index]["action"].isString()) {
			if ((it = remap_string.find(jsonStruct["actuators"][index]["action"].toString())) == remap_string.end()) {
				actuator.action = make_shared<string>(jsonStruct["actuators"][index]["action"].toString());
				remap_string.insert({jsonStruct["actuators"][index]["action"].toString(), actuator.action});
			}
			else {
				actuator.action = it->second;
			}

			if (actuator.action->compare("convert") == 0)
				actuator.name = jsonStruct["actuators"][index]["name"].toString();

		}
		else {
			actuator.action = NULL;
		}
		if ((it = remap_string.find(jsonStruct["actuators"][index]["url"])) == remap_string.end()) {
			actuator.url = make_shared<string>(jsonStruct["actuators"][index]["url"].toString());
			remap_string.insert({jsonStruct["actuators"][index]["url"].toString(), actuator.url});
		}
		else {
			actuator.url = it->second;
		}
		device_struct->actuators.insert({jsonStruct["actuators"][index]["id"].extract<int>(), actuator});
		actuator.name.clear();
	}
}

void JSONDevices::loadConverterConfiguration(Poco::DynamicStruct & jsonStruct, json_device * device_struct) {
	for (int index = 0, count = jsonStruct["converter"].size(); index < count; index++) {
		shared_ptr<converter_option> options = make_shared<converter_option>();
		for (int begin = 0, end = jsonStruct["converter"][index]["options"].size(); begin < end; begin++) {
			options->options_name.push_back(jsonStruct["converter"][index]["options"][begin][0].toString());
			options->options_id.push_back(jsonStruct["converter"][index]["options"][begin][1].extract<int>());
		}

		for (int begin = 0, end = jsonStruct["converter"][index]["support"].size(); begin < end; begin++)
			device_struct->converter.insert({jsonStruct["converter"][index]["support"][begin].toString(),options});

	}
}

void JSONDevices::loadSensorsConfiguration(Poco::DynamicStruct & jsonStruct, json_device * device_struct) {
	vector<tuple<string, int, string> > values;
	vector<string> groups = loadSensorsGroups(jsonStruct);
	for (vector<string>::iterator it = groups.begin(); it != groups.end(); it++) {
		for (int index = 0, count = jsonStruct["sensors"][*it].size(); index < count; index++) {
			if (jsonStruct["sensors"][*it][index]["action"].isString()) {
				values.push_back(make_tuple(
										 jsonStruct["sensors"][*it][index]["name"].toString(),
										 jsonStruct["sensors"][*it][index]["id"].extract<int>(),
										 jsonStruct["sensors"][*it][index]["action"].toString())
								);
			}
			else {
				values.push_back(make_tuple(
										 jsonStruct["sensors"][*it][index]["name"].toString(),
										 jsonStruct["sensors"][*it][index]["id"].extract<int>(),
										 "")
								);
			}
		}
		device_struct->sensors.insert({*it, values});
		values.clear();
	}
}

vector<string> JSONDevices::loadFunctions(Poco::DynamicStruct & jsonStruct) {
	vector<string> functions;

	for (int index = 0, count = jsonStruct["functions"].size(); index < count; index++)
		functions.push_back(jsonStruct["functions"][index].toString());

	return functions;
}

vector<string> JSONDevices::loadSensorsGroups(Poco::DynamicStruct & jsonStruct) {
	vector<string> groups;

	for (int count = jsonStruct["sensors"]["groups"].size(), index = 0; index < count; index++)
		groups.push_back(jsonStruct["sensors"]["groups"][index].toString());

	if (groups.empty()) {
		throw Poco::Exception("JSON: Invalid json format for vendor \"" + jsonStruct["vendor"].toString() +
							  "\" and device \"" + jsonStruct["device"].toString() + "\"\n");
	}
	return groups;
}
