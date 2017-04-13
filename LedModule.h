/**
 * @file LedModule.h
 * @Author BeeeOn team - Richard Wolfert (wolfert.richard@gmail.com)
 * @date 9/2016
 * @brief
 */

#pragma once

extern bool quit_global_flag;

#include <iostream>
#include <memory>
#include <string>

#include <cstdlib>
#include <ctime>
#include <unistd.h>

#include <Poco/Logger.h>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Runnable.h>

#include "IODaemonMsg.h"
#include "device_table.h"
#include "ModuleADT.h"
#include "utils.h"

class Aggregator;

class LedModule : public ModuleADT {
private:
	Poco::Net::DatagramSocket socket;
	int ioDaemonPort;
	int receivePort;
	IODaemonMsg ioDaemonMsg;

	long long int getEUID(std::string adapter_id);
	void threadFunction() override;
	void sendToDaemon(const IODaemonMsg &ioMessage);
	IODaemonMsg receiveFromDaemon();
	bool createServerMsg(const IODaemonMsg &ioMessage);
	IODaemonMsg createDaemonMsg();
	bool createMsg();
	bool recoverLastValue(struct ledConfiguration &new_config);
	bool obtainLastValue(int module_id, int &value);

public:
	LedModule(IOTMessage _msg, std::shared_ptr<Aggregator> _agg);
	void parseCmdFromServer(const Command& cmd) override;
};
