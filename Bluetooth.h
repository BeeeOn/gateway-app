/**
 * @file Bluetooth.h
 * @Author BeeeOn team (MS)
 * @date October, 2016
 * @brief Availability monitoring Bluetooth devices
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

extern bool quit_global_flag;

#include <atomic>
#include <list>
#include <string>
#include <vector>

#include <Poco/Mutex.h>

#include "main.h"
#include "ModuleADT.h"
#include "utils.h"

class Aggregator;

class Bluetooth : public ModuleADT {
public:
	/**
	 * Set initial values and get informations from a configuration file
	 */
	Bluetooth(IOTMessage msg, std::shared_ptr<Aggregator> agg);

	/**
	 * Processing command from the server
	 */
	void parseCmdFromServer(const Command& cmd) override;

private:
	unsigned int m_wake_up_time;
	std::list<std::string> m_MAC_list;   // List of mac address saved locally
	/**
	 * @brief m_queries
	 * Number of attempts to get a list of paired devices from the server
	 * Number decreases with each attempt, or resets immediately in case of success.
	 */
	std::atomic<int> m_queries;
	std::string m_ifname;   // bluetooth interface name for hcitool
	Poco::Mutex m_mutex_msg;
	Poco::Mutex m_mutex_exec;

	/**
	 * Periodically send "data" messages with status Bluetooth device
	 *
	 * Naturally end when quit_global_flag is set to true,
	 * so the Adaapp is terminating
	 */
	void threadFunction() override;

	/**
	 * Try turn on the USB dongle
	 */
	bool turnOnDongle();

	/**
	 * Packing data to struct for send
	 */
	IOTMessage createMsg(const std::string &mac, float value, const std::string &name = "");

	/**
	 * Checks states of paired devices and sends to server
	 */
	void checkAndSendDevices();

	/**
	 * Basic Bash command execution
	 * @param result - stdout from program in terminal
	 * @param first	- program
	 * @param second - first agrument for program
	 * @param third - second agrument for program
	 * @return exit_code
	 *   return 0     => OK
	 *   return n > 0 => Some error
	 */
	int exec(std::string &result, const std::string &first, const std::string &second = "", const std::string &third = "");

	/**
	 * Send new device (found by scan of bluetooth network) to server.
	 */
	void sendNewDeviceToServer(const std::string &mac_address, const std::string &device_name);

	/**
	 * Parse and process the stdout message from hcitool scan.
	 * Example stdout:
	   Scanning ...
		   AA:BB:CC:DD:EE:FF	Device_name
	 */
	void parseScanAndSend(const std::string &scan);

	/**
	 * Full scan of bluetooth net
	 * Result is list of MAC addresses and their names
	 */
	void fullScan();

	/**
	 * Search the list for MAC
	 */
	bool hasMac(const std::string &mac);

	/**
	 * Test if euid is in list with paired devices
	 */
	bool isInList(euid_t euid);

	/**
	 * Insert colons between each pair of string to create MAC in correct format aa:bb:cc:dd:ee:ff
	 */
	std::string insertColons(const std::string &input);

	/**
	 * Convert (string) MAC address to (euid_t) EUID
	 * Remove ":" and convert to euid
	 */
	euid_t extractEUID(std::string mac);

	/**
	 * Convert EUID (hex number) to MAC address (string)
	 * Cuts prefix and insert ":".
	 */
	std::string extractMAC(euid_t euid);

	/**
	 * Distribution of string into the vector by divide string
	 */
	void split(std::vector<std::string> &output, const std::string &input, const std::string &div);

	/**
	 * Reset atomic value m_queries
	 * Adaapp will not ask more (until the next scan)
	 */
	void resetQueries();

	/**
	 * If MAC is valid & is'n in list, than add to list
	 */
	void addIfValid(const std::string &mac);

	/**
	 * Remove MAC address from local list, if exist
	 */
	void removeIfExist(const std::string &mac);

	/**
	 * Processes list of paired devices from server
	 * If exists any new item, add them to local list for scan state
	 */
	void listFromServer(const CmdParam &param);
};

#endif //BLUETOOTH_H
