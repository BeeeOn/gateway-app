/**
 * @file Aggregator.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include "Aggregator.h"
#include "IOcontrol.h"
#include "MosqClient.h"

using namespace std;
using namespace Poco::Net;
using Poco::AutoPtr;
using Poco::FastMutex;
using Poco::File;
using Poco::Logger;
using Poco::StringTokenizer;
using Poco::Util::IniFileConfiguration;

#define LIMIT_TIMESTAMP 1420070400

void Aggregator::buttonCallback(int event_type) {
	std::cout << "Callback::event_type: " << event_type << std::endl;
	switch (event_type) {
		case BUTTON_EVENT_LONG:
			break;

		case BUTTON_EVENT_SHORT:
			cout << "BUTTON_EVENT_SHORT - Not implemented yet!" << endl;
			break;

		default:
			break;
	}
}

// Helper for to call method which detects press and length of press of button
void buttonControl(void) {
	waitForEvent(Aggregator::buttonCallback);
}

bool isTimeValid(long long int ts = time(NULL)) {
	return (ts > LIMIT_TIMESTAMP);
}

TimeWatchdog::TimeWatchdog(std::chrono::steady_clock::time_point _start) :
	system_start_point(_start),
	active(false),
	agg(NULL)
{
}

void TimeWatchdog::run() {
	active = true;
	while (!quit_global_flag) {
		if (isTimeValid()) {
			if (agg)
				agg->validateAllMessages(time(NULL), getOffset());
			break;
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	active = false;
}

Aggregator::Aggregator(IOTMessage _msg, shared_ptr<MosqClient> _mq) :
	log(Poco::Logger::get("Adaapp-AGG")),
	cache_last_storing_time(time(NULL)),
	msg_default(_msg),
	watchdog(std::chrono::steady_clock::now()),
	mq(_mq)
{
	cache_lock.reset(new FastMutex);
	cache.clear();

	log.setLevel("trace"); // set default lowest level
	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(CONFIG_FILE);
		cache_minimal_items = cfg->getInt("cache.minimal_items_cached", 10);
		cache_minimal_time = cfg->getInt("cache.minimal_saving_time", 10);        // in minutes
		permanent_cache_path = cfg->getString("cache.permanent_cache_path", "/tmp/permanent.cache");

		// Create distributor
		if (cfg->getBool("distributor.enabled", false))
			dist.reset(new Distributor(*this, mq));
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
	}
	setLoggingLevel(log, cfg); // Set logging level from config file
	setLoggingChannel(log, cfg); // Set where to log (console, file, etc.)

#ifdef LEDS_ENABLED
	LEDControl::blinkLED(LED_PAN, 3);
#endif

	param = shared_ptr<Parameters> (new Parameters(*this, msg_default, log));
}

void Aggregator::sendToPANviaMQTT(std::vector<uint8_t> msg) {
	std::string message = "";
	for (auto byte : msg) {
		message += std::to_string(byte) + ",";
	}
	log.information("SENDING THIS BYTES IN STRING: " + message);

	if (dist) {
		log.information("Giving message with MQTT data to distributor.");
		dist->sendMessageToMQTT(message, "BeeeOn/data_to_PAN");
	}
	else {
		log.error("Distributor is NULL, I cannot send this message to MQTT!");
	}
}

void Aggregator::sendFromPAN(uint8_t type, std::vector<uint8_t> msg) {
	if (pan.get() != nullptr)
		pan->msgFromPAN(type, msg);
}

void Aggregator::run() {
	log.information("Starting Aggregator thread...");

	Poco::Thread distThread("Distributor thread");
	if (dist)
		distThread.start(*dist);

	// Button's handler thread
	button_t = std::thread (buttonControl);

	Poco::Thread watchdog_t;
	watchdog.setAgg(this);

	// Initialize timer at least as possible
	cache_last_storing_time = time(NULL);

	// try to load stored cache to SD card and add items (which may not has been sent to server) to cache and takes it like another IoT messages ready to send to server.
	cache_lock->lock();
	loadCache();

	printCache(false);
	cache_lock->unlock();

	while (!quit_global_flag) {
		// If there is not valid time, blackout is signalled. Start process for measuring the blackou length to create new timestamps.
		// Cache is flushed to not to lose current items (in case of application restart during solving of blackout)
		if (!isTimeValid() && (!watchdog.isActive())){
			log.warning("Blackout of time service!");
			watchdog.setBlackouStartPoint(std::chrono::steady_clock::now());
			watchdog_t.start(watchdog);
			cache_lock->lock();
			if (!cache.empty())
				storeCache();
			cache_lock->unlock();
		}
		if (cache_lock->tryLock(1000)) {
			if (!cache.empty()) {
				log.information("Items in queue (" + toStringFromInt(cache.size()) + ")");

				multimap<Cache_Key, IOTMessage>::iterator it = cache.begin();
				// Search item with valid timestamp and highest priority. Validity is not part of the key because it is changed on
				// several places in the code (changing would hurt performance)
				while ((it != cache.end()) && !(it->second.valid)) {
					++it;
				}
				// Check if there is any message which can be send.
				bool no_sendable_msg = (it != cache.end()) ? false : true;

				long long int now = std::time(NULL);
				// Save to cache if this condition is satisfied.
				if ((now - cache_last_storing_time > (cache_minimal_time*60)) && (cache.size() >= cache_minimal_items)) {
					log.information("Items (" + std::to_string(cache.size()) + ") are accumulated in cache -> saved to file \"" + permanent_cache_path + "\".");
					log.information("Last cache-storing time: \"" + toStringFromLongInt(cache_last_storing_time) + "\", new time (now): \"" + toStringFromLongInt(now) + "\"");
					storeCache();
				}

				pair<Cache_Key, IOTMessage> item;
				if (!no_sendable_msg) {
					item = (*it);
					cache.erase(it);

					log.information("Removed from the Aggregator's cache queue, now contains " + toStringFromInt(cache.size()) + " items.");
				}
				cache_lock->unlock();

				// Primary function to send data to server
				if (!no_sendable_msg)
					sendData(item.second);
			}
			else {    // Queue is empty
				// Update time of last cache save. Otherwise it can be saved in wrong time.
				cache_last_storing_time = time(NULL);

				// remove file if the cache is empty = this means that all messages were successfully sent to server
				File x(permanent_cache_path);
				if (x.exists()) {
					log.information("Cache is now empty, so file \"" + permanent_cache_path + "\" with backup of cache is no longer needed - removing it.");
					try{
						x.remove();
					}
					catch (Poco::Exception& ex){
						log.error("Error in removing permanent cache file");
					}
				}
				cache_lock->unlock();
			}
		}
		else {
			log.warning("Cannot lock Aggregator's cache!.");
		}
		sleep(2);
	}

	distThread.join();
}

pair<bool, Command> Aggregator::sendData(IOTMessage _msg) {
	IOTMessage msg(_msg);
	Command c;
	pair<bool, Command> retval = std::pair<bool, Command>(false, c);

	// Send valid message
	if (isTimeValid(msg.time)){
		retval = tcp->sendToServer(msg);
	} else {
		// Message with invalid timestamp came in valid time
		msg.valid = false;
		msg.offset = watchdog.getOffset();
	}

	if (!retval.first && msg.state == "data") {
		if (isTimeValid(msg.time))
			log.warning("Failed to send message (ts=" + to_string(msg.time) + ") to server - save to cache!");
		else
			log.warning("Can't send message (ts=" + to_string(msg.time) + ") to server - its time is not valid - save to cache!");
		cache_lock->lock();
		cache.insert(make_pair(Cache_Key(msg.priority, msg.time), msg));
		cache_lock->unlock();
	}
	// Distribute to other listeners
	// TODO - Send even if it is not valid?
	if (dist)
		dist->addNewMessage(msg); // Secondary function to add message to the history

	return retval;
}

void Aggregator::setPAN(shared_ptr<PanInterface> _pan) {
	pan = _pan;
}

void Aggregator::setVPT(shared_ptr<VPTSensor> _vpt) {
	vpt = _vpt;
}

void Aggregator::setPSM(shared_ptr<PressureSensor> _psm) {
	psm = _psm;
}

void Aggregator::setVSM(shared_ptr<VirtualSensorModule> _vsm) {
	vsm = _vsm;
}

void Aggregator::setTCP(shared_ptr<IOTReceiver> _tcp) {
	tcp = _tcp;
}

void Aggregator::setHAB(shared_ptr<OpenHAB> _hab) {
	hab = _hab;
}

void Aggregator::parseCmd(Command cmd) {
	log.information("Agg: parseCmd, Command = " + cmd.state);

	if (cmd.state == "listen") {
		if (vsm && vsm->unpairedSensorsLeft()) {
			log.information("Sending incoming PAIRING command to VSM");
			vsm->fromServerCmd(cmd);
		}
		else if (pan.get()!=nullptr) {
			log.information("Sending incoming PAIRING command to PAN");
			pan->sendCommandToPAN(cmd);
		}
		if (vpt) {
			vpt->parseCmdFromServer(cmd);
		}
		if (hab) {
			hab->cmdFromServer(cmd);
		}
	}
	else if (cmd.state == "getparameters" || cmd.state == "parameters"){
		log.information("Incoming GET-PARAMETER command");
		param->cmdFromServer(cmd);
	}
	else {
		if (psm && psm->belongTo(cmd.euid)) {
			log.information("Sending incoming command to PSM");
			psm->parseCmdFromServer(cmd);
		}
		else if (vsm && vsm->isVirtualDevice(cmd.euid)) {
			log.information("Sending incoming command to VSM");
			vsm->fromServerCmd(cmd);
		}
		else if (vpt && vpt->isVPTSensor(cmd.euid)) {
			log.information("Sending incoming command to VPT");
			vpt->parseCmdFromServer(cmd);
		}
		else if (hab) {
			log.information("Sending incoming command to HAB");
			hab->cmdFromServer(cmd);
		}
		else if (pan.get() != nullptr) {
			log.information("Sending incoming command to PAN");
			pan->sendCommandToPAN(cmd);
		}
	}
}

/*
 * Flush whole cache to a memory
 * Important: It is crucial call cache_lock before and after the function call
 */
void Aggregator::storeCache() {

	// TODO Maybe better to do it even without distributor?
	if (!dist) {
		log.warning("Distributor does not exist - skipping storing to cache!");
		return;
	}

	log.information("Storing cached items to file \"" + permanent_cache_path + "\"");

	ofstream permanent_cache;
	permanent_cache.open(permanent_cache_path.c_str());
	for (auto item : cache) {
		permanent_cache << dist->convertToCSV(item.second, true);
	}
	permanent_cache.close();

	cache_last_storing_time = time(NULL);
}

void Aggregator::loadCache() {
	File x(permanent_cache_path);
	if (!x.exists()) {
		log.information("There is no such file \"" + permanent_cache_path + "\" for restoring cache - nothing to do!");
		return;
	}

	ifstream permanent_cache;
	permanent_cache.open(permanent_cache_path.c_str());

	while (permanent_cache.good()) {
		IOTMessage msg;

		std::string line;
		std::getline(permanent_cache, line);
		if (((line == "") && !permanent_cache.good())) {
			break;
		}
		else if (line == "\n")
			continue;

		StringTokenizer token(line, (std::string)";", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
		for (unsigned int i = 0; i < (unsigned int)token.count();) {
			if (token[i].compare("time") == 0)
				msg.time = toIntFromString(token[i+1]);

			else if (token[i].compare("euid") == 0)
				msg.device.euid = stoull(token[i+1], nullptr, 0);

			else if (token[i].compare("device_id") == 0)
				msg.device.device_id = toIntFromString(token[i+1]);

			else if (token[i].compare("dev_version") == 0)
				msg.device.version = toIntFromString(token[i+1]);

			else if (token[i].compare("valid") == 0)
				msg.valid = (token[i+1].compare("yes") == 0) ? true : false;

			else if (token[i].compare("state") == 0)
				msg.state = token[i+1];

			else if (token[i].compare("fw_version") == 0)
				msg.fw_version = token[i+1];

			else if (token[i].compare("protocol_version") == 0)
				msg.protocol_version = token[i+1];

			else if (token[i].compare("pairs") == 0)
				msg.device.pairs = atoi(token[i+1].c_str());

			else if (token[i].compare("tt_version") == 0)
				msg.tt_version = toIntFromString(token[i+1]);

			else {
				log.information("Unknown type of token name: " + token[i]);
			}
			i+=2;
		}
		for (int i = 0; i < msg.device.pairs; i++) {
			std::getline(permanent_cache, line);
			StringTokenizer token2(line, (std::string)";", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
			int module_id = 0;
			float value = 0;

			for (unsigned int i = 0; i < (unsigned int)token2.count(); ) {
				if (token2[i].compare("module_id") == 0)
					module_id = toNumFromString(token2[i+1]);
				else if (token2[i].compare("value") == 0)
					value = toFloatFromString(token2[i+1]);
				else {
					log.information("Unknown type of token name: " + token2[i]);
				}
				i = i+2;
			}
			msg.device.values.push_back({module_id, value});
		}
		// Four options
		// 1) Valid msg timestamp   + valid system time     - everything is ok
		// 2) Valid msg timestamp   + invalid system time   - ok, similar to above
		// 3) Invalid msg timestamp + valid system time     - offset cannot be computed, message will be most probably dropped
		// 4) Invalid msg timestamp + invalid system time   - offset can be computed (offset is difference between msg timestamp and current timestamp
		if (isTimeValid(msg.time)) {     // Option 1 + 2
			msg.offset = 0;
			msg.valid = true;
		}
		else if (isTimeValid()) {                         // Option 3
			// TODO Most probably nothing to do
			log.information("This message has been stored with invalid TS, but now is time valid and I cannot find out origin TS.");
		}
		else {                                            // Option 4
			msg.offset = msg.time - time(NULL);       // Offset won't be negative (it is past, i.e. when that message came)
			msg.valid = false;
			log.information("This message has been stored with invalid TS, but now the time is not valid neither - so i can count offset from msg TS and actual TS.");
		}
		msg.adapter_id = msg_default.adapter_id;
		cache.insert(std::pair<Cache_Key, IOTMessage>(Cache_Key(msg.priority, msg.time), msg));
	}
	log.information("Loading of persistent cache is complete. Cache contains " + std::to_string(cache.size()) + " items.");
	permanent_cache.close();
	return;
}

void Aggregator::printCache(bool verbose) {
	if (!dist)
		return;

	int i = 0;
	for (auto item : cache) {
		i++;
		if (verbose) {
			log.information("Cache message (" + std::to_string(i) + "):\n" + dist->convertToXML(item.second) + "\n");
		}
		else {
			std::string val = "--MSG (" + std::to_string(i) + "): ts=" + std::to_string(item.second.time) + ", valid=" + (item.second.valid ? "Y" : "N") + ", offset=" + to_string(item.second.offset) + ", prio=";
			switch(item.second.priority) {
				case MSG_PRIO_ACTUATOR:
					val += "ACT";
					break;

				case MSG_PRIO_SENSOR:
					val += "SEN";
					break;

				case MSG_PRIO_HISTORY:
					val += "HIS";
					break;

				case MSG_PRIO_REG:
					val += "REG";
					break;

				default:
					val += "UNK";
					break;
			}
			log.information(val);
		}
	}
}

void Aggregator::validateAllMessages(long long int now, long long int duration) {
	cache_lock->lock();
	printCache(false);
	long long int orig_ts = now - duration;    // Timestamp of blackout
	log.information("Validating messages - now:" + std::to_string(now) + ", duration:" + std::to_string(duration) + ", orig_ts:" +   std::to_string(orig_ts));

	for (std::multimap<Cache_Key, IOTMessage>::iterator it = cache.begin(); it != cache.end(); ++it) {
		if(!it->second.valid) {
			it->second.time = orig_ts + it->second.offset;
			it->second.offset = 0;
			it->second.valid = true;
		}
	}
	// Store cache after update, computed timestamps are valuable.
	storeCache();
	cache_lock->unlock();
}

Aggregator::~Aggregator() {
	if (cache_lock->tryLock(5000)) {;
		storeCache();
		cache_lock->unlock();
	}

	if (cache_lock.get()!=nullptr) {
		cache_lock->unlock();
	}

	button_t.join();
}

/**
 * Convert value according ot its type
 * @param type Module from types table
 * @param old_val Original value
 * @param reverse If false, convert from, otherwise to.
 * @return Converted value
 */
float Aggregator::convertValue(TT_Module type, float old_val, bool reverse) {
	float new_val = old_val;
	std::string transform = (reverse ? type.transform_to : type.transform_from);

	if (transform != "") {
		float modifier = toFloatFromString(transform.substr(1));
		string op = transform.substr(0,1);

		if (op.compare("+") == 0)
			new_val += modifier;

		else if (op.compare("-") == 0)
			new_val -= modifier;

		else if (op.compare("/") == 0) {
			if (old_val != 0)
				new_val /= modifier;
		}
		else if (op.compare("*") == 0)
			new_val *= modifier;
		else {
			log.warning("Unknown modify operation: \"" + op + "\"!");
		}
	}
	return new_val;
}

void Aggregator::sendHABtoServer(std::string msg_text){
	if (hab) {
		IOTMessage msg = hab->msgFromMqtt(msg_text);
		if(msg.time != 0)
			sendData(msg);
	} else {
		log.information("New message on MQTT from OpenHAB, but module in NOT enabled.");
	}
}

/**
 * Forwarding message from modul Openhab to MosqClient
 * @param msq_text Message content
 * @param topic Name of topic on MQTT
 */
void Aggregator::sendToMQTT(std::string msg_text, std::string topic){
	log.information("Agg: OH: msg from server to MQTT");
	mq->send_message(msg_text,topic,0);
}

CmdParam Aggregator::sendParam(CmdParam par){
	return param->askServer(par);
}
