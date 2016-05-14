/**
 * @file Parameters.cpp
 * @Author BeeeOn team (MS)
 * @date April, 2016
 * @brief
 */

#include "Parameters.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Util::IniFileConfiguration;
using Poco::Net::NetworkInterface;
using Poco::Net::IPAddress;

Parameters::Parameters(shared_ptr<Aggregator> _agg, IOTMessage _msg) :
	log(Poco::Logger::get("Adaapp-Param")),
	agg(_agg),
	msg(_msg)
{
	log.setLevel("trace"); // set default lowest level
	ada_type = "other";

	AutoPtr<IniFileConfiguration> cfg_param;
	try {
		cfg_param = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_PARAM)+".ini");
		setLoggingLevel(log, cfg_param); // Set logging level from config file
		setLoggingChannel(log, cfg_param); // Set where to log ( console, file, etc.)
		ada_type = cfg_param->getString(string(MOD_PARAM) + ".type", "other");
	}
	catch (Poco::Exception& ex) {
		log.error("Error: Exception with config file reading: " + (string)ex.displayText());
		ada_type = "other";
	}

	msg.state = "getparameters";
	msg.priority = MSG_PRIO_PARAM;
	msg.offset = 0;

	log.information("=== Adaapp Started ===");
}

/**
 * Processing the request from the server.
 * Return true in case of success or false if a message is not supported.
 */
bool Parameters::cmdFromServer(Command cmd)
{
	if (toFloatFromString(cmd.protocol_version) < 1.1) {
		return false;
	}
	if (cmd.state == "getparameters") { // new request from the server
		log.information("cmdFromServer | state == getparameters");
		CmdParam filled_params;		// (will be) filled struct for send
		switch (cmd.params.param_id) {
			case 2000: filled_params = getDummy(cmd.params); break;
			case 2001: filled_params = getLocalIp(cmd.params); break;
			case 2002: filled_params = getGatetype(cmd.params); break;
			default: return false;
		}
		msg.params = filled_params;	// add parameters block (struct)
		msg.state = "parameters";	// for answer is this state
		msg.time = time(NULL);		// set actual time
		agg->sendData(msg);			// send (response) to server
		return true;
	}
	else if (cmd.state == "parameters") { // response from the server or other information
		log.information("cmdFromServer | state == parameters");
		switch (cmd.params.param_id) {
			case 1000: log.information("Server PING me"); break;
			case 1001: /*label*/ justPrintToLog(cmd.params); break;
			case 1002: /*room*/ justPrintToLog(cmd.params); break;
			case 1003: allsensors(cmd.params); break;
			case 1004: /*vptpasswd*/ justPrintToLog(cmd.params); break;
			default: return false;
		}
	}
	return true;
}

/**
 * Sending a request to the server.
 * This function concurrently returns an answer from the server.
 * In case of an error returns the original unchanged structure.
 */
CmdParam Parameters::askServer(CmdParam cmd_request)
{
	if (cmd_request.param_id < 1000 || cmd_request.param_id > 1999){
		log.error("Ask the Server: Wrong param_id");
		return cmd_request;	// id of requirement must be specified
	}
	msg.params = cmd_request;		// add parameters block (struct)
	msg.state = "getparameters";	// for request is this state
	msg.time = time(NULL);			// set actual time
	log.information("Ask the Server | state = getparameters | param_id = " + toStringFromInt(cmd_request.param_id));
	pair<bool, Command> response = agg->sendData(msg);	// send to server
	if (response.first && response.second.state == "parameters"){
		log.information("Ask the Server | return OK");
		justPrintToLog(response.second.params);
		return response.second.params;	// prayers were answered
	}
	log.information("Ask the Server | return FAIL");
	return cmd_request;
}

/**
 * Returns a vector (list) of IP address (active connections).
 */
vector<string> Parameters::getIP()
{
	vector<string> list_of_ip;
	NetworkInterface::Map m = NetworkInterface::map(false, false);
	for (NetworkInterface::Map::const_iterator it = m.begin(); it != m.end(); ++it) {
		if ( ! it->second.isUp())
			continue;

		typedef NetworkInterface::AddressList List;
		const List& ipList = it->second.addressList();

		for (auto ip: ipList) {
			std::string tmp = ip.get<NetworkInterface::IP_ADDRESS>().toString();
			if ( (! ip.get<NetworkInterface::IP_ADDRESS>().isLoopback()) &&		// no localhost
				(ip.get<NetworkInterface::IP_ADDRESS>().family() == Poco::Net::IPAddress::IPv4) &&	// only IPv4
				(std::find(list_of_ip.begin(), list_of_ip.end(), tmp) == list_of_ip.end()) ) {	// not in list
					list_of_ip.push_back(tmp);		// add
			}
		}
	}
	return list_of_ip;
}

void Parameters::allsensors(CmdParam incomer)
{
	log.information("Refresh euid list");
	euides.clear();
	for(auto item: incomer.value){
		log.information("  " + item.first + " (" + item.second + ")");
		euides.push_back(item);
	}
}

void Parameters::justPrintToLog(CmdParam incomer)
{
	log.information("Parameters from server, param_id = ", incomer.param_id);
	for(auto item: incomer.value){
		log.information("  " + item.first + " (" + item.second + ")");
	}
}

CmdParam Parameters::getDummy(CmdParam incomer)
{
	log.information("get Dummy");
	incomer.value.push_back({"",""});
	return incomer;
}

CmdParam Parameters::getLocalIp(CmdParam incomer)
{
	log.information("get Local IP");
	vector<string> ip_list = getIP();
	for (auto item: ip_list){
		incomer.value.push_back({item,""});
	}
	return incomer;
}

CmdParam Parameters::getGatetype(CmdParam incomer)
{
	log.information("get Gate type");
	incomer.value.push_back({ada_type,""});
	return incomer;
}
