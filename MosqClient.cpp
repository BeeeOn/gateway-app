/**
 * @file MosqClient.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include <cstdbool>

#include "MQTTDataModule.h"
#include "MosqClient.h"

using namespace std;
using Poco::AutoPtr;
using Poco::Util::IniFileConfiguration;

MosqClient::MosqClient(std::string client_id_, std::string main_topic_, std::string msg_prefix_, std::string host_, int port_) :
	mosquittopp(client_id_.c_str()),
	keepalive(60),
	id(client_id_),
	topic(main_topic_),
	msg_prefix("["+msg_prefix_+"]"),
	host(host_),
	port(port_),
	log(Poco::Logger::get("Adaapp-MQTT")),
	agg(nullptr),
	connected(false)
{
	AutoPtr<IniFileConfiguration> cfg;
	try {
		cfg = new IniFileConfiguration(std::string(MODULES_DIR)+std::string(MOD_MQTT)+".ini");
	}
	catch (Poco::Exception& ex) {
		log.error("Exception with config file reading:\n" + ex.displayText());
		exit (EXIT_FAILURE);
	}

	mosqpp::lib_init();        // Mandatory initialization for mosquitto library
	log.information("mosqClient \"" + id + "\" connecting to main topic \"" + topic + "\" to broker with interface \"" + host + ":" + std::to_string(port) + "\".");
	connect_async(host.c_str(), port, keepalive);   // non blocking connection to broker request
}

bool MosqClient::send_message(std::string message, int qos) {
	return send_message(message, topic, qos);
}

bool MosqClient::send_message(std::string message, std::string topic_, int qos) {
	log.information("Sending this message to topic \"" + topic_ + "\" with QoS " + std::to_string(qos) + ":\n" + message);
	return ((publish(NULL, topic_.c_str(), message.length(), message.c_str(), qos)) == MOSQ_ERR_SUCCESS );
}

void MosqClient::setAgg(Aggregator* agg_) {
	agg = agg_;
}

void MosqClient::newMessageFromPAN(std::string msg) {
	uint8_t type = 0;
	std::vector<uint8_t> vec;
	std::string s(msg);
	std::string delimiter = ",";
	size_t pos = 0;
	std::string token;
	bool type_flag = false;
	//split
	while ((pos = s.find(delimiter)) != std::string::npos) {
		token = s.substr(0, pos);
		if (type_flag == false) {
			type = toIntFromString(token);
			type_flag = true;
		}
		else
			vec.push_back(toIntFromString(token));
		s.erase(0, pos + delimiter.length());
	}
	//uint8_t last = toIntFromString(s.substr(pos + delimiter.length()));
	//vec.push_back(last);

	if (agg)
		agg->sendFromPAN(type, vec);
}

void MosqClient::newMessageToMQTTDataModule(std::string msg)
{
	if (agg)
		agg->sendToMQTTDataModule(msg);
	else
		log.error("missing aggregator ", __FILE__, __LINE__);
}

MosqClient::~MosqClient() {
	disconnect();
	mosqpp::lib_cleanup();    // Mosquitto library cleanup
}

void MosqClient::on_disconnect(int rc) {
	log.information("Client disconnection(" + std::to_string(rc) + ")");
	connected = false;
}

void MosqClient::on_connect(int rc) {
	if ( rc == 0 ) {
		log.information("Successfully connected with server!");
		connected = true;
		add_topic_to_subscribe(topic);
		send_message("[AdaApp]Welcome");
		send_message("[AdaApp]Welcome", "BeeeOn/service");
	}
	else {
		log.warning("Impossible to connect with server(" + std::to_string(rc) + ")!");
	}
}

/*void MosqClient::on_publish(int mid) {
	log.information(">> MosqClient - Message (" << mid << ") succeed to be published ");
}*/

/*void MosqClient::on_log(int, const char *str) {
	log.information("Log: " + std::string(str));
}*/

void MosqClient::on_error() {
	log.error("Error function()");
}

void MosqClient::on_unsubscribe(int mid) {
	log.information("Unsubscribe (" + std::to_string(mid) + ") succeed to be published ");
}

void MosqClient::on_message (const struct mosquitto_message *message) {
	std::string msg_topic(message->topic);
	std::string msg_text((const char*)message->payload);
	if ((msg_topic.compare("BeeeOn/sensors") != 0) && (msg_topic.compare("BeeeOn/sensors/csv") != 0) && (msg_topic.compare("BeeeOn/sensors/xml") != 0))
		log.information(/*"[" + std::to_string(time(NULL)) + "]:*/ "New message on topic: " + msg_topic + " -> " + msg_text);

	if (msg_topic.compare("BeeeOn/data_from_PAN") == 0) {
		newMessageFromPAN(msg_text);
	}

	if (msg_topic.compare("BeeeOn/kodivis_cmd") == 0) {
		if (msg_text.compare("IsThatYou") == 0){
			send_message("YesIAm","BeeeOn/kodivis", 0);
		}
	}
	if (msg_topic.compare("BeeeOn/param") == 0) {
		askTheServer(msg_text);
	}

	if (msg_topic.compare(MQTT_DATA_MODULE_TO_ADAAPP) == 0) {
		newMessageToMQTTDataModule(msg_text);
	}
}

void MosqClient::on_subscribe (int mid, int qos_count, const int *granted_qos) {
	log.information("On subscribe(" + std::to_string(mid) + ") - qos=" + std::to_string(qos_count) + ", granted qos=" + std::to_string(*granted_qos));
}

bool MosqClient::add_topic_to_subscribe (std::string topic_) {
	log.information("Adding new topic to subscribe:\"" + topic_ + "\"");
	int ret = subscribe(NULL, topic_.c_str());
	return ( ret == MOSQ_ERR_SUCCESS );
}

void MosqClient::askTheServer(string msg_text){
	log.information("Mqtt ask the server for parameter " + msg_text);
	if(agg){
		CmdParam param;
		param.param_id = toIntFromString(msg_text);
		param = agg->sendParam(param);
		if (! param.status)
			return;
		if (param.value.size()){
			for(auto item: param.value){
				log.information("Server answer: " + item.first);
			}
			log.information("Server answers done.");
		} else log.information("No values from server");
	} else {
		log.error("askTheServer | Missing agg");
	}
}

void MosqClient::run()
{
	while (!quit_global_flag) {
		int rc = loop();
		if (rc != MOSQ_ERR_SUCCESS) {
			log.debug("Trying to reconnect");
			reconnect();
			usleep(500000);
		}
	}
}
