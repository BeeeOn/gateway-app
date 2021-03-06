/**
 * @file HTTP.cpp
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Definition functions for HTTPClient
 */


#include <tuple>

#include <Poco/URI.h>

#include "compat.h"
#include "HTTP.h"

#define FOUR_BYTE 3

#define IP_ADDR 0
#define MASK_ADDR 1

#define CONNECT_TIMEOUT 10000 //Receive timeout 10ms (in us)
#define RECEIVE_TIMEOUT 3 //Receive timeout 3s


using namespace std;
using namespace Poco::Net;
using Poco::Logger;

const uint32_t UNKNOWN_ADAPTER_IP = 0;

HTTPClient::HTTPClient(uint16_t _port) : log(Poco::Logger::get("Adaapp-VPT")) {
	receiveTime = Poco::Timespan(RECEIVE_TIMEOUT, 0);
	port = _port;
}

vector<string> HTTPClient::discoverDevices(void) {
	return detectNetworkDevices(detectNetworkInterfaces());
}

uint32_t HTTPClient::findAdapterIP(std::string target_ip) {
	Poco::Net::NetworkInterface::NetworkInterfaceList list = Poco::Net::NetworkInterface::list(); ///< List of interfaces

	struct in_addr *addr;

	if (!list.empty()) {
		for(auto itr=list.begin(); itr!=list.end(); ++itr) {
			if (itr->isPointToPoint())
				continue;

			NetworkInterface::AddressList iplist = itr->addressList();
			IPAddress mask;
			for(auto ip_itr=iplist.begin(); ip_itr != iplist.end(); ++ip_itr) {
				if (ip_itr->get<IP_ADDR>().family() == Poco::Net::IPAddress::Family::IPv6)
					continue;

				mask = ip_itr->get<MASK_ADDR>();
				IPAddress network_ip = ip_itr->get<IP_ADDR>() & mask;
				//get<IP_BROADCAST>() does not work as expected
				if (isFromSubnet(network_ip, mask, IPAddress(target_ip))) {
					addr = (struct in_addr *) ip_itr->get<IP_ADDR>().addr();
					return (uint32_t) addr->s_addr;
				}
			}
		}
	}
	return UNKNOWN_ADAPTER_IP; //not found IP
}

string HTTPClient::sendRequest(string ip, string url) {
	http.reset();
	response.clear();
	request.clear();
	string encoded_url;

	Poco::URI::encode(url, " +", encoded_url);
	http.setHost(ip);
	http.setPort(port);
	http.setTimeout(receiveTime);
	request.setMethod(Poco::Net::HTTPRequest::HTTP_GET);
	request.setURI(encoded_url);
	log.information("HTTP: " + ip + "(" + to_string(port) + ")" + ": " + url);
	http.sendRequest(request);
	istream & input = http.receiveResponse(response);

	const int status = response.getStatus();

	if (status >= 400)
		log.warning("HTTP: Response: " + to_string(status));
	else
		log.information("HTTP: Response: " + to_string(status));

	return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}

string HTTPClient::sendRequestPost(const string &uri, const string &headerAction, const string &msg)
{
	http.reset();
	response.clear();
	request.clear();

	Poco::URI uri2(uri);
	http.setHost(uri2.getHost());
	http.setPort(uri2.getPort());
	http.setTimeout(receiveTime);

	request.setMethod(Poco::Net::HTTPRequest::HTTP_POST);
	request.setURI(uri);
	request.setContentType("text/xml; charset=\"utf-8\"");
	request.set("SOAPACTION", "\"urn:Belkin:service:basicevent:1#" + headerAction + "\"");
	request.set("Accept", " ");

	log.information("HTTP: " + uri2.getHost() + "(" + to_string(uri2.getPort()) + ")" + ": " + uri);

	request.setContentLength(msg.length());

	std::ostream& os = http.sendRequest(request);
	os << msg;
	istream & input = http.receiveResponse(response);

	const int status = response.getStatus();

	if (status >= 400)
		log.warning("HTTP: Response: " + to_string(status));
	else
		log.information("HTTP: Response: " + to_string(status));

	return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}


vector<string> HTTPClient::detectNetworkDevices(vector<pair<uint32_t, IPAddress>> ip_ranges) { //,  HTTPDeviceDetector *detector) {
	vector<string> devices;
	IPAddress broadcastIP;
	StreamSocket stream;
	uint32_t help_mask;
	uint8_t * ptr_ipv4bytes = (uint8_t *) &help_mask;
	Poco::Timespan connectTime(CONNECT_TIMEOUT);
	for (vector<pair<uint32_t, IPAddress>>::iterator it = ip_ranges.begin(); it != ip_ranges.end(); it++) {
		help_mask = it->first;
		broadcastIP = it->second;
		IPAddress testIP((struct addr_in *) &help_mask, sizeof(help_mask));
		//Generate IP for one range
		for (int p = 0; testIP <= broadcastIP && p < 256; p++, testIP = IPAddress((struct addr_in *) &help_mask, sizeof(help_mask)), ptr_ipv4bytes[FOUR_BYTE]++) {
			try {
				//Try connect on IP
				stream.connect(SocketAddress(testIP, Poco::UInt16(80)), connectTime);
				devices.push_back(testIP.toString());
			}
			catch (...) { }
			stream.close();
		}
	}
	return devices;
}

static inline uint32_t ipv4_to_mask_u32(const NetworkInterface::AddressTuple &a)
{
	IPAddress address = a.get<IP_ADDR>();
	IPAddress mask = a.get<MASK_ADDR>();
	struct in_addr *addr = (struct in_addr *) (address & mask).addr();

	return (uint32_t) addr->s_addr;
}

//Get Broadcast IP address, Prefix less than 24 will be rounding to 24
static inline uint32_t ipv4_to_broadcast_u32(const NetworkInterface::AddressTuple &a)
{
	uint32_t addr = ipv4_to_mask_u32(a);
	int prefix = a.get<MASK_ADDR>().prefixLength();
	uint8_t *bytes = (uint8_t *) &addr;
	int i;
	int k;

	for (i = 32, k = 1; prefix < i; i--, k <<= 1)
		bytes[FOUR_BYTE] |= k;

	return addr;
}

void HTTPClient::checkIPAddresses(NetworkInterface::AddressList &iplist,
	vector<pair<uint32_t, IPAddress>> &networks)
{
	for(NetworkInterface::AddressList::const_iterator ip_itr=iplist.begin(); ip_itr != iplist.end(); ++ip_itr) {
		//Element have to be IPv4 and contains IP Address, Mask Address, BroadCast Address
		if ((ip_itr->get<IP_ADDR>()).family() == Poco::Net::IPAddress::Family::IPv4 &&  ip_itr->length == 3) {
			uint32_t mask = ipv4_to_mask_u32(*ip_itr);
			uint32_t broadcast = ipv4_to_broadcast_u32(*ip_itr);
			networks.push_back({mask, IPAddress((struct in_addr *) &broadcast, sizeof(broadcast))});
		}
	}
}

vector<pair<uint32_t, IPAddress>> HTTPClient::detectNetworkInterfaces(void) {
	log.debug("HTTP: Detect network interfaces");
	Poco::Net::NetworkInterface::NetworkInterfaceList list = Poco::Net::NetworkInterface::list(); ///< List of interfaces
	vector<pair<uint32_t, IPAddress>> networks;

	if (!list.empty()) {
		for(NetworkInterface::NetworkInterfaceList::const_iterator itr=list.begin(); itr!=list.end(); ++itr)
		{
			if (itr->isLoopback() || itr->isPointToPoint())
				continue;

			NetworkInterface::AddressList iplist = itr->addressList();
			log.debug("HTTP: Check interface: " + itr->adapterName());
			checkIPAddresses(iplist, networks);
		}
	}
	else {
		throw Poco::Exception("HTTP: Interfaces not found");
	}
	return networks;
}

bool HTTPClient::isFromSubnet(IPAddress &network_ip, IPAddress &mask, IPAddress target_ip) {
	IPAddress broadcast_ip = network_ip | ~mask;
	log.debug("HTTP: Network range: " + network_ip.toString() + " - " + broadcast_ip.toString());

	if (target_ip.family() == network_ip.family() &&
			target_ip > network_ip &&
			target_ip < broadcast_ip)
		return true;

	return false;
}
