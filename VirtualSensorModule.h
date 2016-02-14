/**
 * @file VirtualSensorModule.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef VIRTUALSENSORMODULE_H
#define	VIRTUALSENSORMODULE_H

extern bool quit_global_flag;

#include <Poco/AutoPtr.h>
#include <Poco/File.h>
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/Thread.h>

#include "Aggregator.h"
#include "utils.h"
#include "VirtualSensor.h"

class Aggregator;
class VirtualSensor;
class VirtualSensorValue;

/**
 * Class for VS. It precesses their configuration, create them and run their threads.
 */
class VirtualSensorModule : public Poco::Runnable {
public:
	VirtualSensorModule(IOTMessage _msg, std::shared_ptr<Aggregator> _agg);
	void run();
	bool isVirtualDevice(euid_t sensor_id);
	void fromServerCmd(Command _cmd);
	virtual ~VirtualSensorModule();
	bool unpairedSensorsLeft();

private:
	IOTMessage msg;
	std::shared_ptr<Aggregator> agg;
	std::vector<VirtualSensor> sensors;
	std::vector<Poco::Thread*> sensors_threads;
	unsigned int unpaired_sensors;
	Poco::Logger& log;
	std::string vs_paired_path;

	void loadSensors(std::shared_ptr<Aggregator> _agg);
	void pairNewVS(void);
};

#endif	/* VIRTUALSENSORMODULE_H */
