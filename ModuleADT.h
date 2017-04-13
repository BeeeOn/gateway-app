/**
 * @file ModuleADT.h
 * @Author Martin Cechmanek
 * @date December, 2015
 * @brief Abstract Data Type class for common interface for all modules.
 */

#ifndef MODULEADT_H
#define	MODULEADT_H

#include <memory>
#include <thread>

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "device_table.h"
#include "utils.h"

class Aggregator;

class ModuleADT {
public:
		ModuleADT(std::shared_ptr<Aggregator> agg_, std::string logger_name, std::string MODULE, IOTMessage msg_);
		ModuleADT(const ModuleADT& orig) = delete;				// Forced disabling copy constructor
		virtual ~ModuleADT();

		virtual void parseCmdFromServer(const Command& cmd) = 0;
		virtual bool belongTo(euid_t sensor_id);
		void start();
		void stop();
		virtual void threadFunction() = 0;							// this is thread function for running module service - makes whole class abstract

protected:
	Poco::AutoPtr<Poco::Util::IniFileConfiguration> cfg;
	std::shared_ptr<Aggregator> agg;
	bool inicialized = false;
	Poco::Logger& log;
	IOTMessage msg;
	std::thread module_thread;
	Device sensor;
	TT_Table tt;

private:

};

#endif	/* MODULEADT_H */
