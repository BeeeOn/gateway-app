/**
 * @file PanInterface.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include "PanInterface.h"

using namespace std;
using namespace Poco::Net;
using Poco::AutoPtr;
using Poco::FastMutex;
using Poco::File;
using Poco::Logger;
using Poco::Mutex;
using Poco::Util::IniFileConfiguration;

PanInterface::PanInterface(IOTMessage _msg, shared_ptr<Aggregator> _agg) :
	msg(_msg),
	agg(_agg),
	log(Poco::Logger::get("Adaapp-SPI"))
{
	log.setLevel("trace"); // set default lowest level
	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_PAN)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.fatal("Exception with config file reading:\n" + ex.displayText());
		exit(EXIT_FAILURE);
	}
	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/

	msg.time = (long long int)time(0);

	tt = fillDeviceTable();
	log.information("Types Table loaded.");
}

void PanInterface::set_pan(shared_ptr<PanInterface> _pan) {
	pan = _pan;
}

void PanInterface::sendCmd(vector<uint8_t> cmd) {
	agg->sendToPANviaMQTT(cmd);
}

void PanInterface::sendCommandToPAN(Command cmd) {
	if (cmd.state == "update") {
		log.information( "Device " + toStringFromLongInt(cmd.euid) + " ( " + toStringFromLongHex(cmd.euid) + " ) -> Update in " + toStringFromInt(cmd.time) + "s.");
	}
	else if (cmd.state == "set") {
		log.information( "Device " + toStringFromLongHex(cmd.euid) + "-> Set actuator value(s) command:");
		setActuators(cmd);
	}
	else if (cmd.state == "listen") {
		log.information( "Sensor pairing state command.");
		setPairingMode();
	}
	else if (cmd.state == "clean") {
		log.information("Clean command to delete sensor.");
		deleteDevice(cmd.euid);		// TODO search for module_id refresh time first and sent to sensor then
	}
	else if (cmd.state == "reset") {
		log.information("Reset command to reset PAN coordinator to factory defaults.");
		sendCmd({PAN_RESET});		// TODO search for module_id refresh time first and sent to sensor then??
	}
	else if (cmd.state == "error") {
		log.error("Error state in command -> Nothing to send via SPI!");
	}
	else {
		log.error("Unknown state of incomming command from server! Command state = " + cmd.state );
	}
}

void PanInterface::setPairingMode() {
	sendCmd({SET_JOIN_MODE, 1});
}

void PanInterface::setActuators(Command cmd) {
	// Message format: TYPE, PROTO, ADDR(MSB), ADDR, ADDR, ADDR(LSB), COUNT, MODULE_ID, LENGTH, XB_DATA {, TYPE, LENGTH, XB_DATA...}
	log.information("Function set actuators");
	cmd.print();

	vector<uint8_t> msg;
	msg.push_back(SET_ACTUATORS);
	msg.push_back(static_cast<uint8_t>(stoi(cmd.protocol_version) & 0x00FF));
	if (cmd.state != "listen") {
		msg.push_back(static_cast<uint8_t>((cmd.euid & 0xFF000000) >> 24));
		msg.push_back(static_cast<uint8_t>((cmd.euid & 0xFF0000) >> 16));
		msg.push_back(static_cast<uint8_t>((cmd.euid & 0xFF00) >> 8));
		msg.push_back(static_cast<uint8_t>(cmd.euid & 0x00FF));
		msg.push_back(cmd.values.size());

		for (auto item : cmd.values) {
			unsigned int module_id = item.first;
			int size = 0;
			msg.push_back(module_id & 0xFF);

			TT_Module actuator_T;
			log.information("Looking up module \"" + toStringFromHex(module_id) + "\" of device with ID \"" + toStringFromHex(cmd.device_id) + "\" in TT_Table.");
			// Search for the device
			auto dev = tt.find(cmd.device_id);
			if (dev == tt.end()) {
				log.error("Device with ID " + toStringFromHex(cmd.device_id) + " was not found in TT_Table!");
				return;
			}

			// Search for the module
			auto mod = dev->second.modules.find(module_id);
			if (mod == dev->second.modules.end()) {
				log.error("Module with ID " + toStringFromHex(module_id) + " was not found in TT_Table for device " + toStringFromHex(cmd.device_id) + "!");
				return;
			}
			else {
				size = mod->second.size;
				actuator_T = mod->second;
			}

			msg.push_back(size);
			float val_f = item.second;
			val_f = agg->convertValue(actuator_T, val_f, true);

			int val = (int)round(val_f);

			for (int i = size-1; i >= 0; i--) {
				uint8_t tttt = val >> (8*i);
				uint8_t tmp_v = tttt & 0x0FF;
				msg.push_back(tmp_v);
			}
		}
	}

	sendCmd(msg);
}

void PanInterface::deleteDevice(long long int id) {
	log.error("Delete device called with sensor_id = " + to_string(id) + "| This is not implemented yet!");
}

#define U0 1.8 // Drained batteries [V]
#define Up 3.0 // New batteries[V]

/**
 * Conversion of batteries voltage to percents
 * @param bat_actual Current voltage [V]
 * @return Battery capacity in percents
 */
int PanInterface::convertBattery(float bat_actual) {
	float bat_percent = 0;

	if (bat_actual > (U0 + 0.01))
		bat_percent = (bat_actual - U0) / (Up - U0) * 100;
	else
		bat_percent = 1;

	if (bat_percent > 130.0)
		log.warning("Battery level is over 130%, this can indicate error. battery level: " + to_string(bat_actual) +  "V, " +to_string(bat_percent)+"%");

	if (bat_percent > 100.0)
		bat_percent = 100.0;
	return round(bat_percent);
}

void PanInterface::msgFromPAN(uint8_t msg_type, std::vector<uint8_t> data) {
	log.information("New message from PAN: type =" + toStringFromHex(msg_type) + ", message length=" + std::to_string(data.size()));

	Device sensor;

	log.information("Name of received command: ");
	if (theBigSwitch(msg_type, log)) {
		return;
	}

	if (msg_type == WAIT_FOR_ADAPTER) {
		sendCmd({RESTORE_YES});
	}
	else if ((msg_type == FROM_SENSOR_MSG)) {
		try {
			// Iterator over bytes in the message
			unsigned int pos = 0;
			// data[0]
			sensor.version = data.at(pos++);
			// data[1..4]
			sensor.euid = 0;
			for(int i = 0; i < 4; i++) {
				sensor.euid = (sensor.euid << 8) + data.at(pos++);
			}
			// data[5]
			int rssi = data.at(pos++);
			// data[6..7]
			sensor.device_id = (data.at(pos) << 8) + data.at(pos+1);
			pos += 2;
			// data[8]
			sensor.pairs = data.at(pos++);

			// Search for respective device
			TT_Device dev;
			auto d_it = tt.find(sensor.device_id);
			if (d_it == tt.end()) {
					log.error("This device with id (" + toStringFromHex(sensor.device_id) + ") is not known!");
					return;
			}
			dev = d_it->second;

			// data[9..data.size()], pairs:
			// <module id(1B), value (xB according to TT)>

			log.information("Loaded device_id=" + toStringFromHex(sensor.device_id) + ", EUID=" + toStringFromLongHex(sensor.euid) + ", pairs: " + toStringFromInt(sensor.pairs));
			for (int i=0; i < sensor.pairs; i++) {
				unsigned int tmp_id   = data.at(pos++);			// 1B module_id to types_table

				// Search for respective device (to module)
				TT_Module module;
				try {
					module = getModuleFromIDs(sensor.device_id, tmp_id);    // from TT
				}
				catch (...) {
					log.error("This module_id (" + toStringFromHex(tmp_id) + ") is not known!");
					return;
				}

				int tmp_val = 0;
				for (int i = 0; i < module.size; i++)
					tmp_val = (tmp_val << 8) + data.at(pos++);

				float tmp_val_float = tmp_val;

				msg.priority = (module.module_is_actuator ? MSG_PRIO_ACTUATOR : MSG_PRIO_SENSOR);

				log.information("Loaded module_id " + toStringFromHex(tmp_id) + "(type \"" + toStringFromHex(module.module_type) + "\") with size [B]: " + std::to_string(module.size) + ". tmp_val: " + toStringFromFloat(tmp_val_float));

				tmp_val_float = agg->convertValue(module, tmp_val_float);

				// Handle special values
				// Batteries
				float new_battery;
				switch(module.module_id){
					case 0x03: 	//this module represents battery voltage
							// convert from [V] to [%]
						new_battery = convertBattery(tmp_val_float);
						log.information("Converted battery - orig value: "+toStringFromFloat(tmp_val_float)+"V, new: " + toStringFromFloat(new_battery) + "%.");
						sensor.values.push_back({tmp_id, new_battery});
						break;
					case 0x01: // outside temperature level
						if (tmp_val_float > 1000) // 127,255,255,255
							tmp_val_float = 0.0; // FIXME special case, where sensor is not connected
						sensor.values.push_back({tmp_id, tmp_val_float});
						break;
					default:
						sensor.values.push_back({tmp_id, tmp_val_float});
				}
			}
			// Add RSSI if it is in module of speficied device
			try {
				auto t = tt.find(sensor.device_id);
				for (auto mod : (*t).second.modules) {
					if (mod.second.module_type == 0x09)
						sensor.values.push_back({mod.second.module_id, rssi});
				}
			}
			catch (...) { }

			msg.device = sensor;
			msg.state = "data";
			msg.offset = 0;
			msg.device.pairs = msg.device.values.size(); // It might be changed because of special values
			msg.time = time(NULL);

			// TODO? Send just message header if there wasn't any sensor?
			if (msg.device.pairs > 0) {
				agg->sendData(msg);
			}
			log.information("Message sent to server.");
		}
		catch (std::out_of_range) {
			log.error("Data are in incorrect format (out_of_range exception)");
			return;
		}
	}
}

TT_Module PanInterface::getModuleFromIDs(long int device_id, int module_id) {
	auto devs_it = tt.find(device_id);
	if (devs_it == tt.end()) {
		log.error("PanInterface::getModuleFromIDs - Device with ID \"" + toStringFromHex(device_id) + "\" was not found in TT!");
		throw("Unknow device");
	}

	auto mods_it = devs_it->second.modules.find(module_id);
	if (mods_it == devs_it->second.modules.end()) {
		log.error("PanInterface::getModuleFromIDs - Module with ID \"" + toStringFromHex(module_id) + "\" was not found in Device with ID \"" + toStringFromHex(device_id) + "\"!");
		throw("Unknow device");
	}

	return (mods_it->second);
}

/**
 * Print message type
 * @param cmd Command number
 * @param log We need log to write outside of namespace
 * @return 0 if AdaApp knows the command, 1 otherwise
 */
int theBigSwitch(int cmd, Logger& log) {
	switch (cmd) {
		case (WAIT_FOR_ADAPTER) :
			log.information("WAIT_FOR_ADAPTER");
			break;

		case(NVM_MYPANID):
			log.information("NVM_MYPANID");
			break;

		case(NVM_CURRCHANNEL):
			log.information("NVM_CURRCHANNEL");
			break;

		case(NVM_CONMODE):
			log.information("NVM_CONMODE");
			break;

		case(NVM_CONTABLE):
			log.information("NVM_CONTABLE");
			break;

		case(NVM_CONTABLEINDEX):
			log.information("NVM_CONTABLEINDEX");
			break;

		case(NVM_OUTFRAMECOUNTER):
			log.information("NVM_OUTFRAMECOUNTER");
			break;

		case(NVM_SHORTADDRESS):
			log.information("NVM_SHORTADDRESS");
			break;

		case(NVM_PARENT):
			log.information("NVM_PARENT");
			break;

		case(NVM_ROUTING):
			log.information("NVM_ROUTING");
			break;

		case(NVM_NEIGHBOURROUTING):
			log.information("NVM_NEIGHBOURROUTING");
			break;

		case(NVM_FAMILYTREE):
			log.information("NVM_FAMILYTREE");
			break;

		case(NVM_ROLE):
			log.information("NVM_ROLE");
			break;

		case(RELOAD_NETWORK):
			log.information("RELOAD_NETWORK");
			break;

		case(FROM_SENSOR_MSG):
			log.information("FROM_SENSOR_MSG");
			break;

		case(SET_JOIN_MODE):
			log.information("SET_JOIN_MODE");
			break;

		case(RESET_JOIN_MODE):
			log.information("RESET_JOIN_MODE");
			break;

		case(JOIN_RESP):
			log.information("JOIN_RESP");
			break;

		case(JOIN_REQUEST):
			log.information("JOIN_REQUEST");
			break;

		case(TO_SENSOR_MSG):
			log.information("TO_SENSOR_MSG");
			break;

		case(RESTORE_YES):
			log.information("RESTORE_YES");
			break;

		case(RESTORE_NO):
			log.information("RESTORE_NO");
			break;

		case(SET_ACTUATORS):
			log.information("SET_ACTUATORS");
			break;

		case(SENSOR_SLEEP):
			log.information("SENSOR_SLEEP");
			break;

		default:
			log.information("Unknow command (" + toStringFromInt(cmd) + ")");
			return 1;
			break;
	}
	return 0;
}
