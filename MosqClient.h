/**
 * @file MosqClient.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef MYMOSQ_H
#define	MYMOSQ_H

#include <iostream>
#include <thread>

#include <Poco/Runnable.h>

#include <mosquittopp.h>

#include "Aggregator.h"
#include "utils.h"

class Aggregator;

/**
 * Class for communication over MQTT protocol.
 */
class MosqClient : public mosqpp::mosquittopp, public Poco::Runnable {
private:
	int keepalive;
	std::string id;
	std::string topic;
	std::string msg_prefix;
	std::string host;
	int port;
	Poco::Logger& log;
	Aggregator* agg;

public:
	bool connected;
	void run() override;

private:
	void on_connect(int rc);
	void on_disconnect(int rc);
	//void on_publish(int mid);
	//void on_log(int level, const char* str);
	void on_message(const struct mosquitto_message* message);
	void on_subscribe(int mid, int qos_count, const int* granted_qos);
	void on_error();
	void on_unsubscribe(int mid);

public:
	MosqClient(std::string client_id_, std::string main_topic_, std::string msg_prefix, std::string host_ = "localhost", int port_=1883);
	~MosqClient();

	void setAgg(Aggregator* agg_);
	void newMessageFromPAN(std::string msg);
	void newMessageToMQTTDataModule(std::string msg);

	/**
	 * Send message to default topic (specified in constructor).
	 * @param message Message to send
	 * @param qos QoS for the message
	 * @return true for successful send, false otherwise
	 */
	bool send_message(std::string message, int qos=0);

	/**
	 * Send message to speficied topic
	 * @param message Message to send
	 * @param topic Topic to send
	 * @param qos QoS for the message
	 * @return true for successful send, false otherwise
	 */
	bool send_message(std::string message, std::string topic, int qos=0);

	/**
	 * Subscribe to new topic
	 * @param topic Topic name
	 * @return true on successful subscribe, false otherwise
	 */
	bool add_topic_to_subscribe(std::string topic);

	void askTheServer(std::string msg_text);
};

#endif	/* MYMOSQ_H */
