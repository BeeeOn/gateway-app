/**
 * @file Parameters.h
 * @Author BeeeOn team (MS)
 * @date April, 2016
 * @brief
 */

#ifndef PARAMETERS_H
#define	PARAMETERS_H

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Net/NetworkInterface.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "Aggregator.h"
#include "utils.h"

class Parameters {
public:
	Parameters(Aggregator &_agg, IOTMessage _msg, Poco::Logger &_log);
	bool cmdFromServer(Command cmd);
	CmdParam askServer(CmdParam request);
	CmdParam getAllPairedDevices();
	enum {
		GW_PING = 1000,
		GW_GET_DEV_NAME,
		GW_GET_DEV_ROOM,
		GW_GET_ALL_DEVS,
		GW_GET_DEV_CREDENTIALS,
		GW_GET_DEV_MOD_LAST_VALUE,
		GW_END, //stopper don't use in protocol
		SRV_PING = 2000,
		SRV_GET_GW_IP_LIST,
		SRV_GET_GW_TYPE,
		SRV_END, //stopper don't use in protocol
	};

private:
	std::string ada_type;
	Poco::Logger& log;
	Aggregator& agg;
	IOTMessage msg;
	std::vector<std::pair<std::string, std::string> > euides;

	std::vector<std::string> getIP();
	void allsensors(CmdParam incomer);
	void justPrintToLog(CmdParam incomer);
	CmdParam getDummy(CmdParam incomer);
	CmdParam getLocalIp(CmdParam incomer);
	CmdParam getGatetype(CmdParam incomer);
};

#endif //PARAMETERS_H
