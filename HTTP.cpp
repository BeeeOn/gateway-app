/**
 * @file HTTP.cpp
 * @Author Lukas Koszegy (l.koszegy@gmail.com)
 * @date November, 2015
 * @brief Definition functions for HTTPClient
 */


#include <tuple>

#include "HTTP.h"


#define FOUR_BYTE 3

#define IP_ADDR 0
#define MASK_ADDR 1

#define CONNECT_TIMEOUT 10000 //Receive timeout 10ms (in us)
#define RECEIVE_TIMEOUT 3 //Receive timeout 3s


using namespace std;
using namespace Poco::Net;
using Poco::Logger;


HTTPClient::HTTPClient(void) : log(Poco::Logger::get("Adaapp-VPT")) {
	receiveTime = Poco::Timespan(RECEIVE_TIMEOUT, 0);
}

vector<string> HTTPClient::discoverDevices(void) {
	return detectNetworkDevices(detectNetworkInterfaces());
}

string HTTPClient::sendRequest(string ip, string url) {
	http.reset();
	response.clear();
	request.clear();

	http.setHost(ip);
	http.setTimeout(receiveTime);
	request.setMethod(Poco::Net::HTTPRequest::HTTP_GET);
	request.setURI(url);
	log.information("HTTP: " + ip + ": " + url);
	http.sendRequest(request);
	istream & input = http.receiveResponse(response);
	log.information("HTTP: Response status code: " + to_string(response.getStatus()));
	return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}


vector<string> HTTPClient::detectNetworkDevices(vector<pair<uint32_t, IPAddress>> ip_ranges) { //,  HTTPDeviceDetector *detector) {
	vector<string> devices;
	IPAddress broadcastIP;
	StreamSocket stream;
	uint32_t help_mask;
	uint8_t * ptr_ipv4bytes = (uint8_t *) &help_mask;
	Poco::Timespan connectTime(CONNECT_TIMEOUT);
	for (vector<pair<uint32_t, IPAddress>>::iterator it = ip_ranges.begin(); it != ip_ranges.end(); it++ ) {
		help_mask = it->first;
		broadcastIP = it->second;
		IPAddress testIP((struct addr_in *) &help_mask, sizeof(help_mask));
		//Generate IP for one range
		for ( int p = 0; testIP <= broadcastIP && p < 256; p++, testIP = IPAddress((struct addr_in *) &help_mask, sizeof(help_mask)), ptr_ipv4bytes[FOUR_BYTE]++) {
			try {
				//Try connect on IP
				stream.connect(SocketAddress(testIP, Poco::UInt16(80)), connectTime);
				devices.push_back(testIP.toString());
			}
			catch ( ...) { }
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

void HTTPClient::checkIPAddresses(NetworkInterface::AddressList &iplist,
	vector<pair<uint32_t, IPAddress>> &networks)
{
	uint32_t help_mask; // Help variables for save IP address as 32-bit value
	uint8_t * ptr_ipv4bytes = (uint8_t *) &help_mask; //Pointer on bytes IP Address

	for(NetworkInterface::AddressList::const_iterator ip_itr=iplist.begin(); ip_itr != iplist.end(); ++ip_itr) {
		//Element have to be IPv4 and contains IP Address, Mask Address, BroadCast Address
		if ((ip_itr->get<IP_ADDR>()).family() == Poco::Net::IPAddress::Family::IPv4 &&  ip_itr->length == 3 ) {
			//Get Network IP address
			uint32_t mask = ipv4_to_mask_u32(*ip_itr);
			help_mask = mask;
			//Get Broadcast IP address, Prefix less than 24 will be rounding to 24
			for ( uint8_t i = 32, prefix = (uint8_t) (ip_itr->get<MASK_ADDR>()).prefixLength(), k = 1; prefix < i; i--, k = k<<1 ) {
				ptr_ipv4bytes[FOUR_BYTE] = ptr_ipv4bytes[FOUR_BYTE] | k;
			}
			networks.push_back({mask, IPAddress((struct in_addr *) &help_mask, sizeof(help_mask))});
		}
	}
}

vector<pair<uint32_t, IPAddress>> HTTPClient::detectNetworkInterfaces(void) {
	log.information("HTTP: Detect network interfaces");
	Poco::Net::NetworkInterface::NetworkInterfaceList list = Poco::Net::NetworkInterface::list(); ///< List of interfaces
	vector<pair<uint32_t, IPAddress>> networks;

	if ( !list.empty() ) {
		for(NetworkInterface::NetworkInterfaceList::const_iterator itr=list.begin(); itr!=list.end(); ++itr)
		{
			if (itr->isLoopback() || itr->isPointToPoint())
				continue;

			NetworkInterface::AddressList iplist = itr->addressList();
			log.information("HTTP: Check interface: " + itr->adapterName());
			checkIPAddresses(iplist, networks);
		}
	}
	else {
		throw Poco::Exception("HTTP: Interfaces not found");
	}
	return networks;
}
