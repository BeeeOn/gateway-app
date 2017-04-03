/**
 * @file WebSocketServerConnection.cpp
 * @Author BeeeOn team - Richard Wolfert
 * @date Q3/2016
 * @brief Implementation of communication with BeeeOn ada_server over WebSockets
 */

#include <Poco/Exception.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/NetException.h>
#include <Poco/ScopedLock.h>
#include <Poco/Timespan.h>

#include "WebSocketServerConnection.h"
#include "XMLTool.h"

#define DEFAULT_SOCKET_TIMEOUT 5

#define MAX_WAIT_FOR_RESPONSE_TIME 10
#define RETRY_WAIT_TIME 1

#define RECEIVE_BUFFER_SIZE 4096

#define TIME_TO_ANSWER 1

using namespace std;
using namespace Poco::Net;
using Poco::Mutex;

WebSocketServerConnection::WebSocketServerConnection(shared_ptr<Aggregator> agg, Poco::Util::IniFileConfiguration *cfg, IOTMessage msg) :
	m_msg(ServerMessage(msg)),
	log(Poco::Logger::get("WebSocketServerConnection")),
	m_agg(agg),
	m_current_request_id(0)
{
	m_socketTimeout = cfg->getInt("server.timeout", DEFAULT_SOCKET_TIMEOUT);

	try {
		m_host = cfg->getString("server.ip");
		m_port = cfg->getInt("server.port");
		m_uri = cfg->getString("server.uri");
	}
	catch (Poco::Exception &ex) {
		log.log(ex, __FILE__, __LINE__);
		m_initialized = false;
		return;
	}
	m_initialized = true;
}

void WebSocketServerConnection::initConnection()
{
	Mutex::ScopedLock lock(m_init_mutex);
	if (initSocket()) {
		log.debug("connection established");
		sendRegisterMessage();
	}
	else {
		log.debug("failed to connect");
	}
}

bool WebSocketServerConnection::initSocket()
{
	HTTPSClientSession cs(m_host, m_port);
	HTTPRequest request(HTTPRequest::HTTP_GET, m_uri, HTTPMessage::HTTP_1_1);
	HTTPResponse response;

	log.information("Initializing connection to server:");
	log.debug("Hostname:" + m_host);
	log.debug("Port:" + to_string(m_port));
	log.debug("Uri:" + m_uri);

	m_socket.reset();
	try {
		m_socket.reset(new WebSocket(cs, request, response));
	}
	catch(Poco::Exception &ex) {
		log.log(ex, __FILE__, __LINE__);
		return false;
	}

	m_socket->setReceiveTimeout(Poco::Timespan(m_socketTimeout, 0));
	m_socket->setBlocking(true);
	return true;
}

void WebSocketServerConnection::run()
{
	if (m_initialized) {
		log.information("Starting server connection thread");
	}
	else {
		log.critical("Connection settings are not initialized, exiting");
		return;
	}

	while (!quit_global_flag) {
		if (!connectWithRetry(RETRY_WAIT_TIME))
			continue;
		receiveMessages();
	}
}

bool WebSocketServerConnection::connectWithRetry(int timeout)
{
	bool exit_value = true;
	initConnection();
	while (!isConnected()) {
		if (quit_global_flag) {
			exit_value = false;
			break;
		}

		log.warning("failed to connect to server, retry in 1 s",
			__FILE__, __LINE__);

		sleep(timeout);
		initConnection();
	}

	return exit_value;
}

void WebSocketServerConnection::receiveMessages()
{
	while (!quit_global_flag) {
		try {
			vector<string> received_messages;
			received_messages = receiveMessagesFromServer();

			for (string const& message : received_messages) {
				acceptMessage(message);
			}
		}
		catch(Poco::TimeoutException &ex) {
			continue;
		}
		catch(Poco::Net::NetException &ex) {
			log.log(ex, __FILE__, __LINE__);
			break;
		}
	}
}

bool WebSocketServerConnection::isConnected()
{
	return (m_socket.get() != NULL);
}

void WebSocketServerConnection::sendRegisterMessage()
{
	log.information("sending register message to server");
	m_msg.iotmessage.state = "register";
	m_msg.iotmessage.priority = MSG_PRIO_REG;
	XMLTool xml(m_msg);
	string message = xml.createXML(INIT);
	sendStringToServer(message);
}

bool WebSocketServerConnection::sendStringToServer(string message)
{
	Mutex::ScopedLock lock(m_socket_write_mutex);
	log.information("Sending message to server: " + message);
	if (isConnected()) {
		try {
			m_socket->sendBytes(message.c_str(), message.length());
		}
		catch (Poco::Exception &ex) {
			log.log(ex, __FILE__, __LINE__);
			return false;
		}
		log.debug("Send successful");
		return true;
	}
	else {
		log.error("Send failed: socket is not initialized");
		return false;
	}
}

vector<string> WebSocketServerConnection::receiveMessagesFromServer()
{
	char buffer[RECEIVE_BUFFER_SIZE + 1];
	int bytes_received;
	vector<string> result_vector;
	int last_string = 0;

	bytes_received = m_socket->receiveBytes(buffer, RECEIVE_BUFFER_SIZE);
	buffer[bytes_received] = '\0';

	for (int i = 0; i < bytes_received; i++) {
		if (buffer[i] == '\0') {
			result_vector.push_back(m_data_leftover +
					string(buffer + last_string));
			m_data_leftover = "";
			last_string = i + 1;
		}
	}
	m_data_leftover = string(buffer + last_string);

	if (bytes_received < 1)
		throw Poco::Net::NoMessageException("");

	log.debug("received packet of " + to_string(bytes_received) + " Bytes, packet content: " + string(buffer));

	return result_vector;
}

std::pair<bool, Command> WebSocketServerConnection::sendToServer(IOTMessage msg)
{
	std::pair<bool, Command> answer(false, Command());
	ServerMessage sMessage = ServerMessage(msg);
	request_id_t request_id = generateRequestId();

	sMessage.request_id = request_id;
	XMLTool xml(sMessage);
	string message_to_server;

	if(sMessage.iotmessage.state == "getparameters" || sMessage.iotmessage.state == "parameters")
		message_to_server = xml.createXML(PARAM);
	else
		message_to_server = xml.createXML(A_TO_S);

	prepareForResponse(request_id);

	if (!safeSendToServer(message_to_server)) {
		requestDone(request_id);
		return answer; //answer is not valid, answer.first means validity, which is set to false
	}

	int try_count = 0;
	while (try_count < MAX_WAIT_FOR_RESPONSE_TIME) {
		checkForResponse(request_id, answer);

		// We either have answer or adaapp is terminating
		if (answer.first || quit_global_flag ) {
			requestDone(request_id);
			break;
		}

		// Waiting last time in next iteration,
		// we remove request_id from m_pending_requests,
		// so the message can be received only if receiving 
		// thread already received this message, but its 
		// context was changed right before inserting answer 
		// to responses map, otherwise this could result
		// in memory leak, because socket reading thread
		// would insert entry into m_responses map, but none
		// would take it away.
		if (try_count == (MAX_WAIT_FOR_RESPONSE_TIME-1)) {
			requestDone(request_id);
		}

		sleep(TIME_TO_ANSWER); // wait one more second for an answer
		try_count++;
	}

	if (!answer.first) {
		log.error("failed to send message to server");
		log.debug("message content: " + message_to_server);
	}

	return answer;
}

void WebSocketServerConnection::requestDone(request_id_t request_id)
{
	Mutex::ScopedLock lock(m_requests_mutex);
	m_pending_requests.erase(request_id);
}

request_id_t WebSocketServerConnection::generateRequestId()
{
	Mutex::ScopedLock lock(m_current_request_id_mutex);

	if (m_current_request_id == 0) // skip zero, invalid ID
		return ++m_current_request_id;

	return m_current_request_id++;
}

void WebSocketServerConnection::prepareForResponse(request_id_t request_id)
{
	Mutex::ScopedLock lock(m_requests_mutex);

	m_pending_requests.insert(request_id);
	log.debug("inserting pending request number" + to_string(request_id)
			+ ", total pending requests: " + to_string(m_pending_requests.size()));
}

bool WebSocketServerConnection::safeSendToServer(std::string message)
{
	Mutex::ScopedLock lock(m_init_mutex);

	return sendStringToServer(message);
}

void WebSocketServerConnection::checkForResponse(request_id_t request_id, std::pair<bool, Command> &answer)
{
	Mutex::ScopedLock lock(m_responses_mutex);
	map<request_id_t, ServerCommand>::iterator it;

	it = m_responses.find(request_id);
	if (it != m_responses.end()) { //response found
		answer.second = m_responses[request_id].command;
		answer.first = true;
		m_responses.erase(it);
	}
}

void WebSocketServerConnection::acceptMessage(std::string message)
{
	XMLTool xml;
	ServerCommand cmd = xml.parseXML(message);

	log.trace("acceptMessage, parsed message= response_id:" + to_string(cmd.response_id)+" Request id: " + to_string(cmd.request_id));
	if (cmd.response_id != 0) { //This is an answer
		Mutex::ScopedLock lock(m_requests_mutex);

		if (m_pending_requests.find(cmd.response_id) != m_pending_requests.end()) {
			Mutex::ScopedLock lock(m_responses_mutex);

			m_responses.insert(pair<request_id_t, ServerCommand>(cmd.response_id, cmd));
			log.debug("Inserted response to requests number " + to_string(cmd.response_id)
					+ ",total responses = " + to_string(m_responses.size()));
		}
	}
	else {
		if (cmd.request_id != 0) {
			sendAckToServer(cmd.request_id);
			m_agg->parseCmd(cmd.command);
		}
		else {
			log.warning("received message without response_id neither request_id");
		}
	}
}

void WebSocketServerConnection::sendAckToServer(request_id_t response)
{
	log.debug("sending ack message to server");
	m_msg.iotmessage.state = "ack";
	m_msg.response_id = response;
	XMLTool xml(m_msg);
	string message = xml.createXML(A_TO_S);
	safeSendToServer(message);
}
