/**
 * @file TCP.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include <utility>

#include <Poco/AutoPtr.h>
#include <Poco/Event.h>
#include <Poco/Net/SocketAddress.h>

#include "IOcontrol.h"
#include "TCP.h"
#include "XMLTool.h"

using namespace std;
using namespace Poco::Net;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Mutex;
using Poco::Runnable;
using Poco::Semaphore;
using Poco::Util::IniFileConfiguration;


#define RECONNECT_TIME 5  // seconds
#define RECEIVE_TIMEOUT 3 // seconds

/**
 * Thread function for sending data to server (to not to block receiving thread)
 */
void referMessageFromServer(shared_ptr<std::queue<std::string>>send_queue,
			std::pair<shared_ptr<Aggregator>, Poco::Logger&> params,
			shared_ptr<Semaphore> send_semaphore,
			shared_ptr<Mutex>queue_mutex) {

	while(!quit_global_flag){
		shared_ptr<Aggregator> agg = params.first;
		Logger& log = params.second;
		std::string msg;
		Command cmd;

		send_semaphore->wait();
		if (quit_global_flag)
			break;
		queue_mutex->lock();

		if (send_queue->empty()) /*Should not happen*/
			continue;
		msg = send_queue->front();
		send_queue->pop();
		queue_mutex->unlock();

		try {
			unique_ptr<XMLTool> xml(new XMLTool());
			cmd = xml->parseXML(msg);
		}
		catch (Poco::Exception& ex) {
				log.error("Exception: " + ex.displayText());
				return;
		}
		agg->parseCmd(cmd);
	}

}

IOTReceiver::IOTReceiver(shared_ptr<Aggregator> _agg, string _address, int _port, IOTMessage _msg, long long int _adapter_id) :
	agg(_agg),
	address(_address),
	port(_port),
	msg(_msg),
	adapter_id(_adapter_id),
	log(Poco::Logger::get("Adaapp-TCP"))
{
	keepalive = {0,0,0};
	input_socket = nullptr;
	send_semaphore = nullptr;
	send_queue = nullptr;
	queue_mutex = nullptr;

	log.setLevel("trace"); // set default lowest level
	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(std::string(MODULES_DIR)+std::string(MOD_TCP)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
		exit (EXIT_FAILURE);
	}
	setLoggingLevel(log, cfg); /* Set logging level from configuration file*/
	setLoggingChannel(log, cfg); /* Set where to log (console, file, etc.)*/
}

void IOTReceiver::keepaliveInit(IniFileConfiguration * cfg) {

	try{
		if ( cfg->getBool("keepalive.enable") == false ) {
				keepalive_enable = false;
				return;
		}
		keepalive.time = cfg->getInt("keepalive.time", KEEPALIVE_TIME);
		keepalive.interval = cfg->getInt("keepalive.interval", KEEPALIVE_INTERVAL);
		keepalive.probes = cfg->getInt("keepalive.probes", KEEPALIVE_PROBES);
		keepalive_enable = true;
	}
	catch (Poco::Exception& exc) {
		keepalive_enable = false;
	}
}

void IOTReceiver::init() {
	SocketAddress srv_address(address, port);
	input_socket.reset(new SecureStreamSocket(srv_address));
	unique_ptr<XMLTool> xml(new XMLTool(msg));
	string message = xml->createXML(INIT);
	char buffer[2000] = {0};
	input_socket->sendBytes(message.c_str(), message.length());
	input_socket->receiveBytes(buffer, 1999, 0);
	input_socket->setBlocking(true);

	if ( keepalive_enable ) {
		 input_socket->setKeepAlive(true);
		 input_socket->setOption(SOL_TCP, TCP_KEEPIDLE, keepalive.time);
		 input_socket->setOption(SOL_TCP, TCP_KEEPINTVL, keepalive.interval);
		 input_socket->setOption(SOL_TCP, TCP_KEEPCNT, keepalive.probes);
	}

}

void IOTReceiver::run() {
	log.information("Starting SSL connection thread...");

	msg.state = "register";
	msg.priority = MSG_PRIO_REG;

	send_queue.reset(new std::queue<std::string>());
	send_semaphore.reset(new Semaphore(0,10));
	queue_mutex.reset(new Mutex());
	send_thread.reset (new std::thread(referMessageFromServer, send_queue, std::pair<shared_ptr<Aggregator>, Poco::Logger&>(agg, log), send_semaphore, queue_mutex));
	while ( !quit_global_flag ) {
		if ( input_socket.get() == nullptr ) {
			try {
				init();
#ifdef LEDS_ENABLED
				LEDControl::blinkLED(LED_LIME);
#endif
				if (input_socket.get() != nullptr)
					input_socket->setReceiveTimeout(Poco::Timespan(RECEIVE_TIMEOUT,0));
			}
			catch (Poco::Exception& exc) {
				log.error("Exception: " + exc.displayText());
#ifdef LEDS_ENABLED
				LEDControl::setLED(LED_LIME, false);
#endif
				for (unsigned int i = 0; i < RECONNECT_TIME; i++) {
					if (quit_global_flag)
						return;
					sleep(1);
				}
				continue;
			}
		}

		try {
			MSG_TYPE ret;		// variable for the message residue
			try {
				int n = 0;
				// XXX This while should have some maximum message size counter,
				// otherwise it will cycle until \0 will be found
				while ((n >= 0) && (!quit_global_flag)) {
				  MSG_TYPE buffer (BUF_SIZE);
					try {
						n = input_socket->receiveBytes(buffer.data(), buffer.size());
					}
					catch (Poco::TimeoutException& t_ex) {
						n = 0;
					}
					// Check for timeout expiration for receive bytes
					if (n > 0) {
						buffer.resize(n);
						ret.insert(ret.end(), buffer.begin(), buffer.end()); // Concatenate vector
						ret = parseTempMessage(ret);
					}

				}
				if (quit_global_flag)
					return;

				if ( n <= 0 ) {
					input_socket.reset(nullptr);
					continue;
				}

				// It is possible that receiveBytes returned 0 - try to parse message again if there is something
				if (ret.size())
					parseTempMessage(ret);

			} catch (Poco::Exception& exc) {
				log.error("Exception: " + exc.displayText());
				input_socket.reset(nullptr);
			}
			if ( input_socket.get() == nullptr )
				 continue;

		} catch (Poco::Exception& ex) {
			log.error("Exception: " + ex.displayText());
		}
	}
}

/**
 * Parse temporary buffer from server. Delimiter is found, message is parsed and sent to other parts.
 * @param tmp_msg Message which will be parsed
 * @param delimiter Delimiter character
 * @return residue after parsing
 */
MSG_TYPE IOTReceiver::parseTempMessage(MSG_TYPE tmp_msg, char delimiter) {
	MSG_TYPE::iterator it = tmp_msg.end();

	while ((it = std::find(tmp_msg.begin(), tmp_msg.end(), delimiter) ) != tmp_msg.end()) {		// Try to find delimiter
		std::string partial_msg(tmp_msg.begin(), it++);
		if (!partial_msg.empty()) { // Partial message might be empty
			log.information("Incomming message:\n" + partial_msg);

			queue_mutex->lock();
			send_queue->push(partial_msg);
			queue_mutex->unlock();
			send_semaphore->set();
		}
		tmp_msg.erase(tmp_msg.begin(), it);
	}
	return tmp_msg;
}

IOTReceiver::~IOTReceiver() {
	send_semaphore->set();
	send_thread->join();
}
