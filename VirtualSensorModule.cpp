/**
 * @file VirtualSensorModule.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include "VirtualSensorModule.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Runnable;
using Poco::Thread;
using Poco::Util::IniFileConfiguration;

VirtualSensorModule::VirtualSensorModule(IOTMessage _msg, shared_ptr<Aggregator> _agg) :
	msg(_msg),
	agg(_agg),
	log(Poco::Logger::get("Adaapp-VSm"))
{
	log.setLevel("trace"); // set default lowest level

	loadSensors(_agg);
	int sum = 0;
	for (auto s:sensors) {
		sum += s.vsc.size();
	}

	// fixed bug with drained thread pool with more than 16 threads.
	if (sum > 16)
		Poco::ThreadPool::defaultPool().addCapacity(sum-16+1);		// +1 just in case

	// Number of unpaired sensors is total amount of sensors (unless there were some sensors paired in previous run)
	unpaired_sensors = sensors.size();
}

void VirtualSensorModule::run() {
	if (!sensors.size())
		return;

	// Load number of paired VS from last run
	unsigned int paired_sensors = 0;
	Poco::File x(vs_paired_path);
	if (x.exists()) {
		ifstream in;
		std::string tmp;
		in.open(vs_paired_path);
		if (in.good()) {
			std::getline(in, tmp);
			paired_sensors = min(toNumFromString(tmp), (long long int)sensors.size());
			log.information("In last relation was paired " + std::to_string(paired_sensors) + " VS! Activating them. Notice that if there was more VS than in this run, num of VS is decreased.");
		}
		try{
			x.remove();
		}
		catch (Poco::Exception& ex){
			log.error("Error at removing pre-paired sensor count file");
		}
	}

	for (unsigned int i = 0; i < paired_sensors; i++)
		pairNewVS();

	while(!quit_global_flag)
		sleep(1);
}

bool VirtualSensorModule::unpairedSensorsLeft() {
	return (unpaired_sensors);
}

void VirtualSensorModule::fromServerCmd(Command cmd) {
	if (cmd.state == "update") {
		for (VirtualSensor& vs : sensors) {
			if (vs.sensor.euid == cmd.euid) {
				vs.setWakeUpTime(cmd.time);
			}
		}
	}
	else if (cmd.state == "listen") {
		pairNewVS();
	}
	else if (cmd.state == "set") {
		for (VirtualSensor& vs : sensors) {
			if (vs.sensor.euid == cmd.euid) {
				; // It shouldn't happen in case of VS, maybe just for virtual actuator
			vs.parseCmdFromServer(cmd);
			}
		}
	}
}

void VirtualSensorModule::pairNewVS() {
	if (unpaired_sensors) {
		Poco::Thread* t = new Poco::Thread("New VS start");
		log.information("Waking up new sensor in time " + toStringFromLongInt(time(0)));
		t->start(sensors.at(sensors.size() - (unpaired_sensors--)));
		sensors_threads.push_back(t);

		// Save number of paired VS for next run
		ofstream out;
		out.open(vs_paired_path);

		if (out.good())
			out << sensors_threads.size();

		out.close();
		sleep(1);
	}
	else {
		log.information("No sensor left for wake up, sleeping for 2 sec.");
		sleep(2); // Simulation of expired timeout
	}
}

bool VirtualSensorModule::isVirtualDevice(euid_t sensor_id) {
	for (VirtualSensor vs : sensors) {
		if (vs.sensor.euid == sensor_id)
			return true;
	}
	return false;
}

void VirtualSensorModule::loadSensors(shared_ptr<Aggregator> _agg) {
	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_VIRTUAL_SENSOR)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
		exit (EXIT_FAILURE);
	}
	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/

	vs_paired_path = cfg->getString("virtual_sensor.pairing_file", "/tmp/vs_paired.cnt");

	unsigned int i = 1;
	while (!quit_global_flag) {
		try {
			sensors.push_back(VirtualSensor(cfg, msg, i, _agg));
		} catch (Poco::Exception& ex) {
			break;
		}
		i++;
	}
	log.information("VSM loaded " + toStringFromInt(sensors.size()) + " sensors, loading complete.");

}

VirtualSensorModule::~VirtualSensorModule() {
	for (unsigned int i = 0; i < sensors_threads.size(); i++) {
		sensors_threads.at(i)->join();
		delete sensors_threads.at(i);
	}
}
