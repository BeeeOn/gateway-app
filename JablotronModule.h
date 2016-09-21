/**
 * @file JablotronModule.h
 * @Author BeeeOn team - Peter Tisovcik <xtisov00@stud.fit.vutbr.cz>
 * @date August, 2016
 * @brief The class implements the work with Jablotron dongle. Reading value
 * from the device and stored in the Vector devList, parsing the
 * input string from serial port and transmission of value to the
 * server. AC88 allows reading the last value from the server.
 * Turris Dongle documentation: https://www.turris.cz/gadgets/manual .
 */

#ifndef JABLOTRON_MODULE_H
#define JABLOTRON_MODULE_H

#include <Poco/StringTokenizer.h>

#include "ModuleADT.h"
#include "SerialControl.h"

extern bool quit_global_flag;

const euid_t JABLOTRON_PREFIX = 0x0900000000000000;

typedef uint32_t jablotron_sn;

struct TJablotron {
	int serialNumber;
	std::string deviceName;
	jablotron_sn deviceID;
	std::string serialMessage;
};

class Aggregator;

class JablotronModule : public ModuleADT {
public:
	JablotronModule(IOTMessage _msg, std::shared_ptr <Aggregator> _agg);
	bool isJablotronModule(euid_t euid);
	void parseCmdFromServer(const Command& cmd) override;

	enum DeviceList {
		AC88 = 8,
		JA80L = 9,
		TP82N = 10,
		JA83M = 11,
		JA81M = 18,
		JA82SH = 19,
		JA83P = 20,
		JA85ST = 21,
		RC86K = 22,
	};

private:
	struct AC88_ModuleID {
		enum {
			SENSOR_STATE = 0,
		};
	};

	struct JA85ST_ModuleID {
		enum {
			FIRE_SENSOR = 0,
			FIRE_SENSOR_ALARM,
			FIRE_SENSOR_ERROR,
			BATTERY_STATE,
		};
	};

	struct JA83P_JA82SH_ModuleID {
		enum {
			SENSOR = 0,
			SENSOR_ALARM,
			BATTERY_STATE,
		};
	};

	struct JA81M_JA83M_ModuleID {
		enum {
			DOOR_CONTACT = 0,
			DOOR_CONTACT_ALARM,
			BATTERY_STATE,
		};
	};

	struct RC86K_ModuleID {
		enum {
			REMOTE_CONTROL = 0,
			REMOTE_CONTROL_ALARM,
			BATTERY_STATE,
		};
	};

	struct TP82N_ModuleID {
		enum {
			CURRENT_ROOM_TEMPERATURE = 0,
			REQUESTD_ROOM_TEMPERATURE,
			BATTERY_STATE,
		};
	};

	struct SENSOR_VALUE {
		enum {
			UNKNOWN_VALUE = 0,
			STATE_ON,
			STATE_OFF,
		};
	};

	struct FIRE_SENSOR_VALUE {
		enum {
			STATE_OFF = 0,
			STATE_ON,
		};
	};

	Poco::Mutex _mutex;
	IOTMessage msg;
	Device sensorEvent;
	std::string serialDeviceJablotron;
	std::vector<jablotron_sn> devList;

	/*
	 * Field X - output X
	 * Slot which contains the state of the first AC-88 device
	 */
	short pgx = 0;

	/*
	 * Field Y - output Y
	 * Slot which contains the state of the second AC-88 device
	 */
	short pgy = 0;

	std::unique_ptr<SerialControl> serial;
	bool event;

	void threadFunction() override;
	bool isJablotronDevice(jablotron_sn sn);

	/**
	 * @brief Method for requesting last value of specific module from server
	 * @param euid Device identifiers to set
	 * @return if value has valid data
	 */
	bool obtainActuatorState(euid_t euid);

	/**
	 * @brief It sends data from devices to server
	 * @param &jablotron_msg Device identifiers to set
	 * @param &token String contain the value
	 * @return if value has valid data
	 */
	bool sendMessageToServer(TJablotron& jablotron_msg, Poco::StringTokenizer& token);

	/*
	 * It checks message it was read successfully
	 * @param &jablotron_msg Struct contain jablotron data
	 * @param &token String will be divided according spaces
	 */
	bool parseSerialMessage(TJablotron& jablotron_msg, Poco::StringTokenizer& token);

	/*
	 * Parse a value which was read from usb dongle and send to server
	 * @param &jablotron_msg Struct contains jablotron data
	 * @param &token String will be diveded according spaces
	 *
	 */
	bool parseMessageFromDevice(const TJablotron& jablotron_msg, Poco::StringTokenizer& token);

	/*
	 * It sets a value which was read from TP-82N
	 * @param &serialMessage String contain the substring which was read from usb dongle
	 * @param &data String contains a temperature
	 * which can appear outdoor or indoor
	 */
	void parseMessageFromTP82N(const std::string& serialMessage, const std::string& data);

	/*
	 * It sets a value which was read from RC-86K
	 * @param &data String contains the values from the sensor
	 */
	void parseMessageFromRC86K(const std::string& data);

	/*
	 * It sets a value which was read from JA-81M and JA-83M
	 * @param &data String contains the values from the sensor
	 */
	void parseMessageFromJA81M_JA83M(const Poco::StringTokenizer& token);

	/*
	 * It sets a value which was read from JA-83P and JA-82SH
	 * @param &token String contains the values from the sensor
	 */
	void parseMessageFromJA83P_JA82SH(const Poco::StringTokenizer& token);

	/*
	 * It sets a value which was read from JA-85ST
	 * @param &token String contains the values from the sensor
	 */
	void parseMessageFromJA85ST(const Poco::StringTokenizer& token);

	/**
	 * @brief It converts string value to float
	 * @param &msg String contains a float value
	 * @return Float sensor value
	 */
	float getValue(const std::string& msg) const;

	/**
	 * @brief Convert jablotron state to BeeOn state
	 * @param &data String contains state value from sensor
	 * @param reverse If true/false state is replaced
	 * @return Float state value
	 */
	float convert(const std::string& data, bool reverse = true) const;

	/**
	 * @brief Convert Jablotron battery state to BeeOn value
	 * for example 0 => 0%, 1=>100%
	 * @param String value of battery
	 * @return Battery value
	 */
	int getBatteryStatus(const std::string& msg) const;

	/**
	 * @brief Loading of registered devices from the Turris Dongle
	 * @return if it has been loaded successfully
	 */
	bool loadRegDevices();

	/**
	 * @brief It checks if the Turris Dongle is connected.
	 * @param &loadDevices If devices has been loaded successfully or unsuccessful
	 * @return If it has been loaded succesfully and connected or if
	 * it could not load the device
	 */
	bool loadJablotronDevices(bool &loadDevices);

	/**
	 * @brief It locates the device ID
	 * @param sn The serial number of device
	 * @return Device ID
	 */
	int getDeviceID(jablotron_sn sn) const;

	/**
	 * @brief It locates the serial number of device
	 * @param euid Device EUID
	 * @return Serial number of device 
	 */
	jablotron_sn getDeviceSN(euid_t euid) const;

	/**
	 * @brief It converts serial number of device to Device EUID
	 * @param sn The serial number of device
	 * @return Device EUID
	 */
	euid_t getEUID(jablotron_sn sn) const;

	/**
	 * @brief It sendings data to the Turris Dongle according
	 * to the Jablotron protocol
	 * @param &msg Sending data
	 */
	void retransmissionPacket(const std::string& msg) const;

	/**
	 * @brief It sets the value of the switch
	 * @param euid Device EUID
	 * @param sw Switch value
	 */
	void setSwitch(euid_t euid, short sw);
};
#endif
