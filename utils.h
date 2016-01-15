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

#ifndef FW_VERSION
	#define FW_VERSION "Undefined version"
#endif

#define MSG_SIZE 32
#define BUF_SIZE 512

#ifndef RELATIVE_PATHS
  #define MODULES_DIR (std::string)"/etc/beeeon/"
#else
  #define MODULES_DIR (std::string)"../../etc/beeeon/"
#endif

#define EEPROM_PATH "/sys/devices/platform/soc@01c00000/1c2b000.i2c/i2c-1/1-0050/eeprom"
#define EEPROM_SIZE 2048

#define LED_LIME "/sys/devices/leds/leds/a10-olinuxino-lime:green:usr/"
#define LED_PAN  "/sys/devices/leds/leds/a10-olinuxino-lime:green:LED1_OL/"

#ifndef RELATIVE_PATHS
  #define CONFIG_FILE "/etc/beeeon/AdaApp.ini"
#else
  #define CONFIG_FILE "../../etc/beeeon/AdaApp.ini"
#endif

// mainly for .ini files and logs
#define MOD_PAN             (std::string)"pan"
#define MOD_VIRTUAL_SENSOR  (std::string)"virtual_sensor"
#define MOD_PRESSURE_SENSOR (std::string)"pressure_sensor"
#define MOD_VPT_SENSOR      (std::string)"vpt_sensor"
#define MOD_MQTT            (std::string)"mqtt"
#define MOD_OPENHAB         (std::string)"openhab"
#define MOD_TCP         	(std::string)"tcp"
#define MOD_XML         	(std::string)"xmltool"
#define MOD_JSON         	(std::string)"json"

#define MOSQ_TOPICS_PREFIX  (std::string)"BeeeOn/"
#define MOSQ_TOPIC_DATA   MOSQ_TOPICS_PREFIX + (std::string)"sensors"
#define MOSQ_TOPIC_INFO   MOSQ_TOPICS_PREFIX + (std::string)"service"
#define MOSQ_TOPIC_ERROR  MOSQ_TOPICS_PREFIX + (std::string)"failure"
#define MOSQ_TOPIC_CMD    MOSQ_TOPICS_PREFIX + (std::string)"commands"

#define A_TO_S 0
#define INIT   1

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
#define SENSOR_FACTORY_RESET 0xA1
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
	MSG_PRIO_ACTUATOR= 2,   // Sensor-actuator messages (high-priority)
	MSG_PRIO_REG     = 6    // Regisration message (highest priority)
};

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

inline bool getDebugFlag (void) {
	Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg;
	try {
		cfg = new Poco::Util::IniFileConfiguration(CONFIG_FILE);
	}
	catch (Poco::Exception& ex) {
		std::cerr << "Exception with config file reading:" << std::endl;
		std::cerr << ex.displayText() << std::endl;
		return false;
	}

	return(cfg->getBool("adapter.debug", false));
}

inline bool getConsoleLogFlag (void) {
	Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg;
	try {
		cfg = new Poco::Util::IniFileConfiguration(CONFIG_FILE);
	}
	catch (Poco::Exception& ex) {
		std::cerr << "Exception with config file reading:" << std::endl;
		std::cerr << ex.displayText() << std::endl;
		return false;
	}

	return(cfg->getBool("adapter.console_print", false));
}

inline bool getLogFileFlag (void) {
	Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg;
	try {
		cfg = new Poco::Util::IniFileConfiguration(CONFIG_FILE);
	}
	catch (Poco::Exception& ex) {
		std::cerr << "Exception with config file reading:" << std::endl;
		std::cerr << ex.displayText() << std::endl;
		return false;
	}

	return(cfg->getBool("adapter.logfile_print", false));
}

inline bool getLoggingFlag (std::string component) {
	Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg;
	try {
		cfg = new Poco::Util::IniFileConfiguration(CONFIG_FILE);
	}
	catch (Poco::Exception& ex) {
		std::cerr << "Exception with config file reading:" << std::endl;
		std::cerr << ex.displayText() << std::endl;
		return false;
	}

	return(cfg->getBool("logging."+component, false));
}

inline void setLoggingLevel(Poco::Logger& log,Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg) {
	try{
		std::string level = cfg->getString("Logging.level");
		log.setLevel(level);
	}
	catch (Poco::Exception& ex) {
		log.setLevel("trace");
		log.error("Error at setting logging level, setting \"trace\" level default.");
	}

}

inline void setLoggingChannel(Poco::Logger& log,Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg) {
	Poco::AutoPtr<Poco::SplitterChannel> pSplitter(new Poco::SplitterChannel);
	bool console_log = cfg->getBool("Logging.log_to_console",false);
	if (console_log) {
		Poco::AutoPtr<Poco::ConsoleChannel> pCons(new Poco::ConsoleChannel);
		pSplitter->addChannel(pCons);
	}

	try{
		std::string log_path = cfg->getString("Logging.log_to_file");
		std::string log_file_rotation = cfg->getString("Logging.log_to_file_rotation");
		Poco::AutoPtr<Poco::FileChannel> pFile(new Poco::FileChannel(log_path));
		pFile->setProperty("path",log_path);
		pFile->setProperty("rotation",log_file_rotation);
		pSplitter->addChannel(pFile);

	}
	catch (Poco::Exception& ex) {

	}
	// TODO log to file
	log.setChannel(pSplitter);
}

/**
 * Returns default directory where logs will be stored. This atribute is stored in AdaApp.ini in section Logging. If this DIR does not exist, this function will create it with its full path (including parent directories).
 * @return Directory (needed with ending slash ("/") - e.g. "/tmp/")
 */
inline std::string getLoggingDir () {
	Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg;
	try {
		cfg = new Poco::Util::IniFileConfiguration(CONFIG_FILE);
	}
	catch (Poco::Exception& ex) {
		std::cerr << "Exception with config file reading:" << std::endl;
		std::cerr << ex.displayText() << std::endl;
		return("/tmp/");
	}
	std::string logs_dir = cfg->getString("logging.logs_dir", "/tmp/");

	Poco::File x (logs_dir);
	if (!x.exists()) {
		x.createDirectories();      // if path does not exists, create new recursively (with all parent directories)
	}
	else if (!x.isDirectory()) {  // path exists but is not a directory
		throw ("Conflict with defined logging directory - file \"" + logs_dir + "\" exists but is not directory!!");
	}

	return logs_dir;
}

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

inline Poco::Logger& getLogger (std::string name, std::string path, bool print_source = false) {
	std::string logs_dir = getLoggingDir();
	std::string file_path = logs_dir + path + ".log";

	// Machanism to ensure that log will be created.
	int i = 0;
	while (true) {
		Poco::File x(file_path);
		if ((!x.exists()) || (x.canWrite()) || (i >= 10))
			break;

		file_path = logs_dir + path + std::to_string(i) + ".log";
		i++;
	}

	if (i >= 10)
			throw std::runtime_error("Can't create log \"" + path + ".log\"!!");

	Poco::AutoPtr<Poco::SplitterChannel> splitC(new Poco::SplitterChannel);
	Poco::AutoPtr<Poco::ConsoleChannel> consoleC(new Poco::ConsoleChannel);

	// Channel for writing to final file
	Poco::AutoPtr<Poco::FileChannel> fileC(new Poco::FileChannel);
	fileC->setProperty("path", file_path);
	fileC->setProperty("rotation", "daily");
	fileC->setProperty("purgeAge", "1 months");
	fileC->setProperty("archive", "timestamp");

	Poco::AutoPtr<Poco::PatternFormatter> formatter(new Poco::PatternFormatter);
	if (print_source)
		formatter->setProperty("pattern", "%H:%M:%S %s: %t");
	else
		formatter->setProperty("pattern", "%H:%M:%S: %t");

	// Crate channel which will print to the file
	Poco::AutoPtr<Poco::FormattingChannel> formatC(new Poco::FormattingChannel(formatter, fileC));

	Poco::AutoPtr<Poco::PatternFormatter> formatterConsole(new Poco::PatternFormatter);
	formatterConsole->setProperty("pattern", "%s: %t");
	Poco::AutoPtr<Poco::FormattingChannel> formatConsoleC(new Poco::FormattingChannel(formatterConsole, consoleC));

	if (getLogFileFlag())
		splitC->addChannel(formatC);

	if (getConsoleLogFlag())
		splitC->addChannel(formatConsoleC);

	// Get logger reference (assigned dynamically)
	Poco::Logger& logger = Poco::Logger::get(name);
	logger.setChannel(splitC);

	return logger;
}

inline Poco::Logger& getErrLogger () {
	Poco::AutoPtr<Poco::FileChannel> fileC(new Poco::FileChannel);
	fileC->setProperty("path", getLoggingDir() + (std::string)"error.log");
	fileC->setProperty("rotation", "daily");
	fileC->setProperty("purgeAge", "1 months");
	fileC->setProperty("archive", "timestamp");

	Poco::AutoPtr<Poco::PatternFormatter> formatter(new Poco::PatternFormatter);
	formatter->setProperty("pattern", "%H:%M:%S %s: [%p] %t");

	Poco::AutoPtr<Poco::FormattingChannel> formatC(new Poco::FormattingChannel(formatter, fileC));

	Poco::Logger& logger = Poco::Logger::get("Error");
	logger.setChannel(formatC);

	return logger;
}

struct Device {
	int version;		// Version of PAN<->adapter protocol
	long long int euid;
	long int device_id;	// ID to device table
	int pairs;		// Number of pairs
	std::vector<std::pair<int, float> > values;
	Device():
		version(0),
		euid(0),
		device_id(0),
		pairs(0)
	{ }

	void print() {
		std::cout << "proto_v  :" << version << std::endl;
		std::cout << "EUID     :" << euid << std::endl;
		std::cout << "device_id:" << device_id << std::endl;
		std::cout << "pairs    :" << pairs << std::endl;
		for (std::pair<int, float> i : values) {
			std::cout << "  id:" << toStringFromInt(i.first) << ", value:" << toStringFromFloat(i.second) << std::endl;
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

	IOTMessage() :
		protocol_version("0"),
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
	long long int euid;             // EUID of sensor for which that command belongs to
	long long int device_id;        // device_id typu zarizeni
	long long int time;             // when should the sensor wake up
	std::vector<std::pair<int, float> > values;

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
	 */
	TT_Module(int id, int mod_type, int sz, bool mod_act=false, std::string t_from="", std::string t_to="", std::pair<bool, float> v_min={false, 0}, std::pair<bool, float> v_max={false, 0}, std::vector<int> mod_vals={}) :
		module_id(id),
		module_type(mod_type),
		size(sz),
		module_is_actuator(mod_act),
		transform_from(t_from),
		transform_to(t_to),
		value_min(v_min),
		value_max(v_max),
		module_values(mod_vals)
	{ };

	TT_Module() :
		module_id(0),
		module_type(0),
		size(0),
		module_is_actuator(false),
		transform_from(""),
		transform_to(""),
		value_min({false, 0}),
		value_max({false, 0})
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
