/**
 * @file utils.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef UTILS_H
#define	UTILS_H

/**
 * Library for shared functions and definitions
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <tuple>
#include <vector>

#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <Poco/AutoPtr.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/FileChannel.h>
#include <Poco/File.h>
#include <Poco/Formatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/Logger.h>
#include <Poco/NumberFormatter.h>
#include <Poco/NumberParser.h>
#include <Poco/PatternFormatter.h>
#include <Poco/SplitterChannel.h>
#include <Poco/Util/IniFileConfiguration.h>
#include <Poco/Util/XMLConfiguration.h>

#include "compat.h"

#ifndef FW_VERSION
	#define FW_VERSION "Undefined version"
#endif

#define MSG_SIZE 32
#define BUF_SIZE 512

#ifndef RELATIVE_PATH
  #define MODULES_DIR (std::string)"/etc/beeeon/"
#else
  #define MODULES_DIR (std::string) RELATIVE_PATH "/beeeon/"
#endif

#define EEPROM_PATH "/sys/devices/platform/soc@01c00000/1c2b000.i2c/i2c-1/1-0050/eeprom"
#define EEPROM_SIZE 2048

#define LED_LIME "/sys/devices/leds/leds/a10-olinuxino-lime:green:usr/"
#define LED_PAN  "/sys/devices/leds/leds/a10-olinuxino-lime:green:LED1_OL/"

// mainly for .ini files and logs
#define CONFIG_FILE MODULES_DIR "AdaApp.ini"
#define LOGGERS_CONFIG_FILE MODULES_DIR "AdaAppLogging.ini"

#define MOD_PAN             (std::string)"pan"
#define MOD_VIRTUAL_SENSOR  (std::string)"virtual_sensor"
#define MOD_PRESSURE_SENSOR (std::string)"pressure_sensor"
#define MOD_VPT_SENSOR      (std::string)"vpt_sensor"
#define MOD_MQTT            (std::string)"mqtt"
#define MOD_OPENHAB         (std::string)"openhab"
#define MOD_TCP         	(std::string)"tcp"
#define MOD_JSON         	(std::string)"json"
#define MOD_PARAM			(std::string)"parameters"

#define DEFAULT_LOG_FORMAT	(std::string)"[%Y-%m-%d %H:%M:%S, %q%q, %s, %T] %t"

#define MOSQ_TOPICS_PREFIX  (std::string)"BeeeOn/"
#define MOSQ_TOPIC_DATA   MOSQ_TOPICS_PREFIX + (std::string)"sensors"
#define MOSQ_TOPIC_INFO   MOSQ_TOPICS_PREFIX + (std::string)"service"
#define MOSQ_TOPIC_ERROR  MOSQ_TOPICS_PREFIX + (std::string)"failure"
#define MOSQ_TOPIC_CMD    MOSQ_TOPICS_PREFIX + (std::string)"commands"

#define PROTOCOL_VERSION (std::string)"1.1"

#define A_TO_S 0
#define INIT   1
#define PARAM  2

#define HEX_NUMBER 16

#define DELAY_TIME 10000        // microseconds -- main.cpp
#define LOCK_DELAY_TIME 5000   // microseconds -- main.cpp
#define DIST_THREAD_SLEEP 100000

// Defined commands (not all of them are used)
#define ZEROS                0x00
#define ZEROA                0x0A
#define BREAK                0x54
#define UNKNOWN              0xFF
#define NVM_MYPANID          0x81
#define NVM_CURRCHANNEL      0x82
#define NVM_CONMODE          0x83
#define NVM_CONTABLE         0x84
#define NVM_CONTABLEINDEX    0x85
#define NVM_OUTFRAMECOUNTER  0x86
#define NVM_SHORTADDRESS     0x87
#define NVM_PARENT           0x88
#define NVM_ROUTING          0x89
#define NVM_NEIGHBOURROUTING 0x8A
#define NVM_FAMILYTREE       0x8B
#define NVM_ROLE             0x8C
#define DBGPRINT             0x90  // TODO
#define RELOAD_NETWORK       0x91  // TODO
#define FROM_SENSOR_MSG      0x92
#define SET_JOIN_MODE        0x93
#define RESET_JOIN_MODE      0x94
#define JOIN_RESP            0x95
#define JOIN_REQUEST         0x96
#define TO_SENSOR_MSG        0x97  // TODO
#define RESTORE_YES          0x98
#define RESTORE_NO           0x99

#define SENSOR_SLEEP         0x9D
#define WAIT_FOR_ADAPTER     0x9F
#define PAN_RESET            0xA0
#define UNPAIR_SENSOR        0xA1
#define SET_ACTUATORS        0xB0

/**
 * Data type for button event (according to the length of press)
 */
enum BUTTON_EVENT {
	BUTTON_EVENT_SHORT = 0,
	BUTTON_EVENT_LONG = 1,
};

/**
 * Data type for message priority. It is used in cache when there is a network outage.
 */
enum MSG_PRIO {
	MSG_PRIO_HISTORY = 0,   // Messages from cache memory (low-priority)
	MSG_PRIO_SENSOR  = 1,   // On-line messages which come from sensor (normal-priority)
	MSG_PRIO_PARAM   = 2,   // Parameter messages (normal-priority)
	MSG_PRIO_ACTUATOR= 3,   // Sensor-actuator messages (high-priority)
	MSG_PRIO_REG     = 6    // Regisration message (highest priority)
};

typedef uint64_t euid_t;

#define EUID_MASK 0x00FFFFFF
#define EUID_PREFIX_MASK 0xFF000000

inline long long int toNumFromString(std::string num) {
	try {
		return(Poco::NumberParser::parseHex64(num));
	}
	catch (Poco::Exception& ex) {
		return -1;
	}
}

inline long long int toIntFromString(std::string num) {
	try {
		return(Poco::NumberParser::parse64(num));
	}
	catch (Poco::Exception& ex) {
		return -1;
	}
}

inline float toFloatFromString(std::string num) {
	try {
		return(Poco::NumberParser::parseFloat(num));
	}
	catch (Poco::Exception& ex) {
		return -1;
	}
}

inline std::string toStringFromInt(int num) {
	return (static_cast<std::ostringstream*>( &(std::ostringstream() << num) )->str());
}

inline std::string toStringFromLongInt(long long int num) {
	return (static_cast<std::ostringstream*>( &(std::ostringstream() << num) )->str());
}

inline std::string toStringFromLongHex (long long int num, int width=6) {
	return (static_cast<std::ostringstream*>( &(std::ostringstream() << "0x" << std::internal << std::setfill('0') << std::setw(width) << std::hex << num) )->str());
}

inline std::string toStringFromFloat (float num) {
	std::ostringstream os;
	os.precision(2);
	return (static_cast<std::ostringstream*>( &(os << std::fixed << num) )->str());
}

inline std::string toStringFromHex (int num, int width = 2, bool upper_case = false) {
	return (static_cast<std::ostringstream*>( &(std::ostringstream() << "0x" << std::internal << std::setfill('0') << std::setw(width) << std::hex << (upper_case ? std::uppercase : std::nouppercase) << num) )->str());
}

struct EEPROM_ITEMS {
	bool valid;                                                               // if eeprom contains 0xad byte as first byte
	unsigned int version;                                                     // version of EEPROM data structure
	std::vector<std::pair<unsigned int, long long int> > items;            // array for items in EEPROM - type (1B), data($length B))
	void print(void) const {
		std::cout << "Is valid? " << (valid ? "YES!" : "NO!") << std::endl;
		std::cout << "Version: " << version << std::endl;
		std::cout << "Values:" << std::endl;
		for (std::pair<unsigned int, long long int> item : items) {
			std::cout << "  Type: " << toStringFromHex(item.first) << ", value: " << toStringFromHex(item.second) << std::endl;
		}
	}
};

inline EEPROM_ITEMS parseEEPROM(void) {
	EEPROM_ITEMS eeprom;
	eeprom.valid = false;

	Poco::File x(EEPROM_PATH);
	if (!x.exists()) {
		return eeprom;
	}

#ifndef PC_PLATFORM
	std::ifstream eeprom_file;
	char buff[EEPROM_SIZE];
	eeprom_file.open(EEPROM_PATH);
	if (eeprom_file.good()) {
		eeprom_file.read(buff, EEPROM_SIZE);
		// Check EEPROM header
		if ((unsigned int)buff[0] != 0xad) {
			std::cout << "EEPROM has wrong header. There is no \'0xad\' at first position" << std::endl;
			return eeprom;
		}
		// Get EEPROM structure version
		eeprom.version = (unsigned int)buff[1];
		unsigned int i = 2;
		while ((i < EEPROM_SIZE) && ((unsigned int)buff[i] != 0xfe)) {
			unsigned int type = buff[i++];
			unsigned int len = buff[i++];
			long long int data = 0;
			for (unsigned int k = 0; (i+k < EEPROM_SIZE) && ((unsigned int)buff[i+k] != 0xfe) && (k < len); k++) {
				data = (data << 8) + (unsigned int)buff[i+k];
			}
			eeprom.items.push_back(std::pair<unsigned int, long long int>(type, data));
			i += len;
		}
		eeprom.valid = true;
	}
	eeprom_file.close();
#endif
	return eeprom;
}

/**
 * Struct for parameters messages.
 */
struct CmdParam {
	int param_id;			// id of requirement
	int module_id;			// optional
	euid_t euid;			// optional
	bool status;			// true = all is ok; false = something is wrong (server or format message)
	std::vector<std::pair<std::string, std::string> > value;	// array of strings
			// pair (firts = value, second = attribute (optional))
	CmdParam() :
		param_id(0),
		module_id(-1),
		euid(0),
		status(false)
	{ };

	inline std::vector<euid_t> getEuides(euid_t prefix = 0)
	{
		std::vector<euid_t> euides;
		for (auto const& it: value) {
			euid_t euid = std::stoull(std::get<0>(it), nullptr, HEX_NUMBER);
			if (!prefix)
				euides.push_back(euid);
			else if ((euid & EUID_PREFIX_MASK) == prefix)
				euides.push_back(euid);
		}
		return euides;
	}
};

struct Value {
	int mid;		// module_id
	float value;	// value (float)
	bool status;	// default: value is valid
	Value():
		mid(0),
		value(0),
		status(true)
	{ }
	Value(int _mid, float _value):
		mid(_mid),
		value(_value),
		status(true)
	{ }
	Value(int _mid, float _value, bool _status):
		mid(_mid),
		value(_value),
		status(_status)
	{ }
	void print(){
		std::cout << "module_id: " << mid << std::endl;
		std::cout << "value    : " << value << std::endl;
		std::cout << "status   : " << status << std::endl;
	}
	bool operator==(const Value& rhs) const {
	return mid == rhs.mid &&
		   value == rhs.value &&
		   status == rhs.status;
	}
};

struct Device {
	int version;		// Version of PAN<->adapter protocol
	euid_t euid;
	long int device_id;	// ID to device table
	int pairs;		// Number of pairs
	std::string name;	// Own name of device
	std::vector<Value> values;
	Device():
		version(0),
		euid(0),
		device_id(0),
		pairs(0),
		name("")
	{ }

	void print() {
		std::cout << "proto_v  :" << version << std::endl;
		std::cout << "EUID     :" << euid << std::endl;
		std::cout << "device_id:" << device_id << std::endl;
		std::cout << "pairs    :" << pairs << std::endl;
		std::cout << "name     :" << name << std::endl;
		for (Value i : values) {
			std::cout << "  module_id:" << i.mid << ", value:" << i.value << std::endl;
		}
	}
};

struct IOTMessage {
	std::string protocol_version;// XML protocol version (adapter<=>server)
	std::string state;           // Type of message, how to handle it
	std::string adapter_id;      // ID of adapter
	std::string fw_version;      // Adapter version
	long long int time;     // Timestamp when the event started
	Device device;
	bool debug;             // Flag for adding debugging information to the XML message
	MSG_PRIO priority;
	long long int offset;   // Offset to compute timstamp (used if there is a non vaid time)
	bool valid;             // Flag to mark non valid time in the message
	long long int tt_version;// Version of table types, used in registration messages
	CmdParam params;		// Struct with parameters

	IOTMessage() :
		protocol_version(PROTOCOL_VERSION),
		state(""),
		adapter_id("0"),
		fw_version("0"),
		time(0),
		device(Device()),
		debug(false),
		priority(MSG_PRIO_SENSOR),
		offset(0),
		valid(false),
		tt_version(0)
	{ }

	void print() {
		std::cout << "valid  :" << valid << std::endl;
		std::cout << "ada_id :" << adapter_id << std::endl;
		std::cout << "proto_v:" << protocol_version << std::endl;
		std::cout << "state  :" << state << std::endl;
		std::cout << "time   :" << time << std::endl;
		std::cout << "fw_ver :" << fw_version << std::endl;
		std::cout << "tt_ver :" << tt_version << std::endl;
		device.print();
	}
};

struct Command {
	std::string protocol_version;	// XML protocol version (adapter<=>server)
	std::string state;		// Type of message, how to handle it
	euid_t euid;             // EUID of sensor for which that command belongs to
	long long int device_id;        // device_id typu zarizeni
	long long int time;             // when should the sensor wake up
	std::vector<std::pair<int, float> > values;
	CmdParam params;

	Command() :
		protocol_version(FW_VERSION),
		state("data"),
		euid(0),
		device_id(0),
		time(0)
	{	}

	void print() {
		std::cout << "dev_id :" << device_id << std::endl;
		std::cout << "EUID   :" << euid << std::endl;
		std::cout << "proto_v:" << protocol_version << std::endl;
		std::cout << "state  :" << state << std::endl;
		std::cout << "time   :" << time << std::endl;
		std::cout << "values :" << std::endl;
		for (auto i : values) {
			std::cout << "  module_id:" << toStringFromHex(i.first) << ", value:" << toStringFromFloat(i.second) << std::endl;
		}
	}
};

/**
 * Module of the device
 */
struct TT_Module {
	int module_id;		// sensor/actuator id
	int module_type;	// type of senzor value
	int size;		// size of the value (in bytes)
	bool module_is_actuator;	// Flag for actuator (true for actuator)
	std::string transform_from;
	std::string transform_to;
	std::pair<bool, float> value_min;
	std::pair<bool, float> value_max;
	std::vector<int> module_values;
	std::vector<int> unavailableValue; // value(s) defining the unavailability

	/**
	 * Constructor for module
	 * @param id ID of the sensor on multisensor
	 * @param mod_type Type of sensor value
	 * @param sz Size of measured value (bytes)
	 * @param mod_act Flag sensor/actuator
	 * @param t_from Transformation when sending from sensor to server
	 * @param t_to Transformation when sending from server to sensor
	 * @param v_min Minimal value for sensor - composed from pair <enabled, minimum>
	 * @param v_max Maximal value for sensor - composed from pair <enabled, maximum>
	 * @param mod_vals List of values which can be reported. It might be empty if not defined
	 * @param un_val Value defining the unavailability of module (or wrong value)
	 */
	TT_Module(int id, int mod_type, int sz, bool mod_act=false, std::string t_from="", std::string t_to="", std::pair<bool, float> v_min={false, 0}, std::pair<bool, float> v_max={false, 0}, std::vector<int> mod_vals={}, std::vector<int> un_val={}) :
		module_id(id),
		module_type(mod_type),
		size(sz),
		module_is_actuator(mod_act),
		transform_from(t_from),
		transform_to(t_to),
		value_min(v_min),
		value_max(v_max),
		module_values(mod_vals),
		unavailableValue(un_val)
	{ };

	TT_Module() :
		module_id(0),
		module_type(0),
		size(0),
		module_is_actuator(false),
		transform_from(""),
		transform_to(""),
		value_min({false, 0}),
		value_max({false, 0}),
		module_values({}),
		unavailableValue({})
	{ };

};

/**
 * Structure for devices.
 */
struct TT_Device {
	int  device_id;
	std::map<int, TT_Module> modules; // <module_id, module>

	TT_Device(int id, std::map<int, TT_Module> mods) :
		device_id(id),
		modules(mods)
	{ }

	TT_Device() :
		device_id(0)
	{	}

};

#endif	/* UTILS_H */
