/**
 * @file WebSocketServerConnection.h
 * @Author BeeeOn team - Richard Wolfert
 * @date Q3/2016
 * @brief Implementation of ServerConnectionInterface using WebSockets
 */

#pragma once

extern bool quit_global_flag;

#include <map>
#include <set>
#include <string>
#include <vector>

#include <Poco/Logger.h>
#include <Poco/Mutex.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "Aggregator.h"
#include "ServerConnector.h"

class WebSocketServerConnection : public ServerConnector {
public:
	WebSocketServerConnection(std::shared_ptr<Aggregator> _agg,
			Poco::Util::IniFileConfiguration *cfg, IOTMessage _msg);
	void run();
	std::pair<bool, Command> sendToServer(IOTMessage _msg);

private:
	void requestDone(request_id_t request_id);

	bool initSocket();
	void initConnection();
	bool sendStringToServer(std::string message);
	std::vector<std::string> receiveMessagesFromServer();
	void sendRegisterMessage();
	unsigned int generateRequestId();

	//sendToServer subroutines
	void prepareForResponse(request_id_t request_id);
	bool safeSendToServer(std::string message);
	void checkForResponse(request_id_t request_id, std::pair<bool, Command> &answer);
	//init and receiveFromServer subroutines
	bool connectWithRetry(int timeout);
	void receiveMessages();
	void acceptMessage(std::string message);
	void sendAckToServer(request_id_t response);
	bool isConnected();
	std::unique_ptr<Poco::Net::WebSocket> m_socket;
	std::string m_data_leftover;

	ServerMessage m_msg;

	//Connection settings
	std::string m_uri;
	std::string m_host;
	int m_port;
	int m_socketTimeout;

	bool m_initialized;

	Poco::Logger& log;
	std::shared_ptr<Aggregator> m_agg;

	//runtime variables
	request_id_t m_current_request_id;
	Poco::Mutex m_current_request_id_mutex;
	std::set<request_id_t> m_pending_requests;
	Poco::Mutex m_requests_mutex;
	std::map<request_id_t, ServerCommand> m_responses;
	Poco::Mutex m_responses_mutex;
	Poco::Mutex m_socket_write_mutex;
	Poco::Mutex m_init_mutex;
};
