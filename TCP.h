/**
 * @file TCP.h
 * @Author BeeeOn team, Martin Čechmánek, Lukáš Köszegy
 * @date
 * @brief
 */

#ifndef TCP_H
#define	TCP_H

extern bool quit_global_flag;

#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <Poco/Logger.h>
#include <Poco/Mutex.h>
#include <Poco/Net/SecureStreamSocket.h>
#include <Poco/Runnable.h>
#include <Poco/Semaphore.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "Aggregator.h"
#include "utils.h"


using MSG_TYPE = std::vector<char>;

#define KEEPALIVE_TIME 900
#define KEEPALIVE_INTERVAL 60
#define KEEPALIVE_PROBES 8

struct KeepALive {
		int time;
		int interval;
		int probes;
};

/**
 * Class for receiving message from server over a SSL connection.
 */
class IOTReceiver : public Poco::Runnable {
private:
	std::unique_ptr<Poco::Net::SecureStreamSocket> input_socket;

	std::shared_ptr<Aggregator> agg;
	std::string address;
	int port;
	IOTMessage msg;
	long long int adapter_id;
	Poco::Logger& log;
	bool keepalive_enable;
	KeepALive keepalive;
	std::shared_ptr<Poco::Semaphore>send_semaphore;
	std::shared_ptr<std::queue<std::string>> send_queue;
	std::shared_ptr<Poco::Mutex> queue_mutex;
	std::shared_ptr<std::thread> send_thread;
public:
		IOTReceiver(std::shared_ptr<Aggregator> _agg, std::string _address, int _port, IOTMessage _msg, long long int _adapter_id);
		~IOTReceiver();
		std::pair<bool, Command> sendToServer(IOTMessage _msg);

		void keepaliveInit(Poco::Util::IniFileConfiguration * cfg);
		void init();
		void run();
private:
		std::string parseTempMessage_alternative(std::string tmp_msg, char delimiter='\0');
		MSG_TYPE parseTempMessage(MSG_TYPE tmp_msg, char delimiter='\0');

};

#endif  /* TCP_H */
