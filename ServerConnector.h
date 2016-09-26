/**
 * @file ServerConnector.h
 * @Author BeeeOn team - Richard Wolfert (xwolfe00@stud.fit.vutbr.cz)
 * @date August, 2016
 * @brief Interface for server communication
 */
#pragma once

#include <memory>

#include <Poco/Runnable.h>

#include "utils.h"

class ServerConnector : public Poco::Runnable {
public:
	virtual std::pair<bool, Command> sendToServer(IOTMessage _msg) = 0;
	virtual void run() = 0;
};
