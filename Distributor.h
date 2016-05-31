/**
 * @file Distributor.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef DISTRIBUTOR_H
#define	DISTRIBUTOR_H

extern bool quit_global_flag;

#include <deque>

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Poco/Runnable.h>

#include "Aggregator.h"
#include "MosqClient.h"
#include "utils.h"
#include "XMLTool.h"


class Aggregator;
class MosqClient;
/**
 * Class for converting IOTMessages to different types of output formats.
 * It is designed even for distributing IOTMessages to third-party applications - running in thread.
 */
class Distributor: public Poco::Runnable {
	private:
		bool geek_mode_enabled;           // enable of geek mode
		std::string geek_mode_pipe;       // name of geek pipe (if enabled)
		int geek_pipe_fd;                 // file descriptor for geek pipe (if enabled)
		std::multimap<unsigned long int, IOTMessage> history; // vector of ALL stored messages
		std::deque<IOTMessage> pending_msgs;                  // vector of unsent messages
		std::unique_ptr<Poco::FastMutex> history_lock;          // Lock for inserting or reading values in history vector
		std::unique_ptr<Poco::FastMutex> pending_lock;          // Lock for inserting or reading values in pending messages vector
		Aggregator &agg;                  // pointer to aggregator for calling "need more history data" messages
		std::shared_ptr<MosqClient> mq;                   // pointer to Mosquitto client for MQTT communication
		Poco::Logger& log;

	public:
		Distributor(Aggregator &_agg, std::shared_ptr<MosqClient> _mq);
		virtual void run();
		~Distributor();

		// Convert functions
		std::string convertToCSV (IOTMessage msg, bool full_format);
		std::string convertToXML (IOTMessage msg);
		std::string convertToPlainText (IOTMessage);

		// transfering functions
		void initGeekMode();
		void sendToNamedPipe (IOTMessage msg);
		void sendToNamedPipe (std::string msg);
		void sendToHDMI ();

		// mosquitto functions
		void sendMessageToMQTT (IOTMessage msg);
		void sendMessageToMQTT (std::string msg);
		void sendMessageToMQTT (std::string msg, std::string topic, int qos=2);

		/**
		 * Send data (sensor) messages. Data are sent to the root topic for data (in full CSV format) and also to subtopics /xml (XML format) and /csv (short CSV format).
		 * @param msg Sensor message
		 * @param qos QoS for the message
		 * @return true if sending was successful (on all topics), false otherwise
		 */
		bool sendDataToMQTT(IOTMessage msg, int qos=2);

		/**
		 * Send error message.
		 * @param message Error message
		 * @param qos QoS for the message
		 * @return true in case of successful send, false otherwise
		 */
		bool sendErrorToMQTT(std::string message, int qos=2);

		/**
		 * Send status message.
		 * @param message Status message
		 * @param qos QoS for the message
		 * @return true in case of successful send, false otherwise
		 */
		bool sendInfoToMQTT(std::string message, int qos=2);

		// tools
		bool getGeekModeFlag() { return geek_mode_enabled; }
		void addNewMessage(IOTMessage msg);
		bool findIfExists(IOTMessage msg);

		// functions for load more history data - TODO - also for particular sensor etc
		void needMoreHistory();
		void needMoreHistory(unsigned long int from, unsigned long int to);
};

#endif	/* DISTRIBUTOR_H */
