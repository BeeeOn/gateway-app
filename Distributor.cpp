/**
 * @file Distributor.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include <unistd.h>

#include "Distributor.h"

using namespace std;
using Poco::AutoPtr;
using Poco::FastMutex;
using Poco::Logger;
using Poco::Util::IniFileConfiguration;

void Distributor::run() {
	log.information("Distributor Thread running.");

	if (mq.get() != nullptr){
		mq->setAgg(agg);
	}

	while(!quit_global_flag) {
		if (pending_lock->tryLock(1000)) {
			if (!pending_msgs.empty()) {
				log.information("Items in queue (" + toStringFromInt(pending_msgs.size()) + "):");

				if (geek_mode_enabled) {
					log.information("Thread function of Distributor is sending data to Geek named pipe.");
					sendToNamedPipe(pending_msgs.front());
				}

				// TODO Other ways of sending
				if (mq)
					sendDataToMQTT(pending_msgs.front());

				pending_msgs.pop_front();

				pending_lock->unlock();
			}
			else
				pending_lock->unlock();
		}
		else {
			log.error("Cannot lock pending queue in thread function.");
		}
		usleep(DIST_THREAD_SLEEP);
	}
}

Distributor::Distributor(shared_ptr<Aggregator> _agg, shared_ptr<MosqClient> _mq) :
	geek_mode_enabled(false),
	agg(_agg),
	mq(_mq),
	log(Poco::Logger::get("Adaapp-DIST"))
{
	log.setLevel("trace"); // set default lowest level

	history_lock.reset(new FastMutex);
	pending_lock.reset(new FastMutex);
	pending_msgs.clear();
	history.clear();

	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(CONFIG_FILE);
		geek_mode_enabled = cfg->getBool("distributor.geek_mode_enabled", false);
		if (geek_mode_enabled) {
			geek_mode_pipe = cfg->getString("distributor.geek_mode_path", "/tmp/geek_mode");
			log.information("Geek mode enabled -> destination: " + geek_mode_pipe);
			initGeekMode();
		}
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
	}

	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/
}

Distributor::~Distributor() {
}

void Distributor::addNewMessage(IOTMessage msg) {
	// Send only data messages (silently discard others)
	if (msg.state != "data") {
		log.information("This is not message with sensor's data - ignoring! (state = " + msg.state + ")");
		return;
	}

	if (findIfExists(msg)) {
		log.information("This message is already in history queue - won't add that.");
		return;
	}

	// Add message to the send queue
	if (pending_lock->tryLock(2000)) {
		pending_msgs.push_back(msg);
		pending_lock->unlock();
	}
	else {
		log.warning("Cannot add item to pending queue!");
	}

	// Add message to the history
	if (history_lock->tryLock(2000)) {
		history.insert(std::pair<unsigned long int, IOTMessage>(msg.time, msg));
		log.information("Inserted to the history, now contains " + toStringFromInt(history.size()) + " items.");
		history_lock->unlock();
	}
	else {
		log.warning("Cannot add item to history!");
	}
}

bool Distributor::findIfExists(IOTMessage m1) {
	for (auto it = history.find(m1.time); it != history.end(); it++) {
		IOTMessage m2 = it->second;
		if (m1.fw_version != m2.fw_version)             return false;
		if (m1.tt_version != m2.tt_version)             return false;
		if (m1.protocol_version != m2.protocol_version) return false;
		if (m1.state != m2.state)                       return false;
		if (m1.debug != m2.debug)                       return false;
		//if (m1.device.battery != m2.device.battery)     return false;
		if (m1.device.euid != m2.device.euid)           return false;
		if (m1.device.device_id != m2.device.device_id) return false;
		if (m1.device.pairs != m2.device.pairs)         return false;
		//if (m1.device.rssi != m2.device.rssi)           return false;
		if (m1.device.version != m2.device.version)     return false;
		if (m1.device.values != m2.device.values)       return false;
		return true;
	}
	return false;
}

std::string Distributor::convertToCSV(IOTMessage msg, bool full_format) {
	std::string ret;

	ret += "time;" + toStringFromLongInt(msg.time) + ";";
	ret += "euid;" + toStringFromLongInt(msg.device.euid) + ";";
	ret += "device_id;" + toStringFromLongInt(msg.device.device_id) + ";";
	if (full_format) {
		ret += "state;" + msg.state + ";";
		ret += "fw_version;" + msg.fw_version + ";";
		ret += "protocol_version;" + msg.protocol_version + ";";
		ret += "dev_version;" + toStringFromInt(msg.device.version) + ";";
		ret += "tt_version;" + toStringFromLongInt(msg.tt_version) + ";";
		ret += "valid;" + (msg.valid ? string("yes") : string("no")) + ";";
	}
	//ret += "battery;" + toStringFromInt(msg.device.battery) + ";";
	//ret += "rssi;" + toStringFromInt(msg.device.rssi) + ";";
	ret += "pairs;" + toStringFromInt(msg.device.pairs) + "\n";

	for (auto i : msg.device.values) {     // module_id, value
		ret += "module_id;" + toStringFromHex(i.first) + ";";
		ret += "value;" + toStringFromFloat(i.second) + "\n";
	}
	return ret;
}

std::string Distributor::convertToXML(IOTMessage msg) {
	std::string ret;

	unique_ptr<XMLTool> xml(new XMLTool(msg));
	if (msg.state == "register")
		ret = xml->createXML(INIT);
	else
		ret = xml->createXML(A_TO_S);

	return ret;
}

std::string Distributor::convertToPlainText(IOTMessage msg) {
	std::string ret;

	ret += "time:" + toStringFromLongInt(msg.time) + "\n";
	ret += "euid:" + toStringFromLongInt(msg.device.euid) + "\n";
	ret += "device_id:" + toStringFromLongInt(msg.device.device_id) + "\n";
	ret += "state:" + msg.state + "\n";
	ret += "fw_version:" + msg.fw_version + "\n";
	ret += "protocol_version:" + msg.protocol_version + "\n";
	ret += "dev_version:" + toStringFromInt(msg.device.version) + "\n";
	//ret += "battery:" + toStringFromInt(msg.device.battery) + "\n";
	//ret += "rssi:" + toStringFromInt(msg.device.rssi) + "\n";
	ret += "sensor_values:\n";
	for (auto i : msg.device.values) {     // type, offset, value
		ret += "\tmodule_id:" + toStringFromHex(i.first) + ", ";
		ret += "value:" + toStringFromFloat(i.second) + "\n";
	}
	return ret;
}

void Distributor::initGeekMode() {
	geek_pipe_fd = mkfifo(geek_mode_pipe.c_str(), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
}

void Distributor::sendToNamedPipe(IOTMessage msg) {
	sendToNamedPipe(convertToCSV(msg, false));
}

void Distributor::sendToNamedPipe (std::string msg) {
	if (geek_mode_enabled) {
		geek_pipe_fd = open(geek_mode_pipe.c_str(), O_WRONLY | O_NONBLOCK);
		if (geek_pipe_fd<1) {
			log.error("Cant open geek pipe file.");
			return;
		}
		write(geek_pipe_fd, msg.c_str(), msg.length());
		close(geek_pipe_fd);
	}
}

void Distributor::sendMessageToMQTT(IOTMessage msg) {
	sendMessageToMQTT(convertToCSV(msg, true));
}

void Distributor::sendMessageToMQTT(std::string msg) {
	if (mq)
		mq->send_message(msg);
}

void Distributor::sendMessageToMQTT(std::string msg, std::string topic, int qos) {
	log.information("Distributing message to topic \"" + topic + "\" with QoS " + std::to_string(qos));

	if (mq) {
		bool retval = mq->send_message(msg, topic, qos);
		log.information("Sending message to MQTT " + std::string(retval ? "succeed!" : "failed!"));
	}
}

bool Distributor::sendDataToMQTT(IOTMessage msg, int qos) {
	log.information("Sendind data to Mosquitto.");
	int ret = 0;
	if (mq) {
		ret += mq->send_message(convertToCSV(msg, true), MOSQ_TOPIC_DATA, qos);
		ret += mq->send_message(convertToCSV(msg, false), MOSQ_TOPIC_DATA+"/csv", qos);
		ret += mq->send_message(convertToXML(msg), MOSQ_TOPIC_DATA+"/xml", qos);
	}
	return (ret == 3);
}
