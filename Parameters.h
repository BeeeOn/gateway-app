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
	Parameters(std::shared_ptr<Aggregator> _agg, IOTMessage _msg);
	bool cmdFromServer(Command cmd);
	CmdParam askServer(CmdParam request);

private:
	std::string ada_type;
	Poco::Logger& log;
	std::shared_ptr<Aggregator> agg;
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
