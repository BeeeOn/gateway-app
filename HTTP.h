/**
 * @file HTTP.h
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Class for support communucation via HTTP
 */

#ifndef HTTP_H
#define HTTP_H


#include <iostream>
#include <string>
#include <vector>
#include <utility>

#include <Poco/AutoPtr.h>
#include <Poco/Exception.h>
#include <Poco/Logger.h>
#include <Poco/SharedPtr.h>

#include <Poco/Net/IPAddress.h>
#include <Poco/Net/NetworkInterface.h>

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>


/**
 * Class represent devices, which have output in json format
 */
class HTTPClient {
public:
	HTTPClient(uint16_t _port = 80);

	std::vector<std::string> discoverDevices();

	std::string sendRequest(std::string ip, std::string url = "/values.json");

private:
	Poco::Net::HTTPClientSession http;
	Poco::Net::HTTPResponse response;
	Poco::Net::HTTPRequest request;

	Poco::Logger& log;
	Poco::Timespan receiveTime;
	uint16_t port;


	void checkIPAddresses(Poco::Net::NetworkInterface::AddressList &iplist,
		std::vector<std::pair<uint32_t, Poco::Net::IPAddress>> &networks);
	std::vector<std::string> detectNetworkDevices(std::vector<std::pair<uint32_t, Poco::Net::IPAddress>>);
	std::vector<std::pair<uint32_t, Poco::Net::IPAddress>> detectNetworkInterfaces();

};
#endif
