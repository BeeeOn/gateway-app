#include <iostream>
#include <string>

#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/NodeIterator.h>
#include <Poco/DOM/NodeFilter.h>
#include <Poco/DOM/AutoPtr.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Timespan.h>

#include "Belkin_WeMo.h"
#include "Parameters.h"
#include "HTTP.h"

#define BELKIN_SWITCH_STATE_ON 1
#define BELKIN_SWITCH_STATE_OFF 0
#define BELKIN_MODULE_ID 0
#define BELKIN_SWITCH_INDEX 36

using namespace std;
using namespace Poco::XML;
using namespace Poco::Net;
using namespace Poco;

Belkin_WeMo::Belkin_WeMo(IOTMessage _msg, std::shared_ptr<Aggregator> _agg) :
  ModuleADT(_agg, "Adaapp-BELKIN_WEMO", MOD_BELKIN_WEMO, _msg), m_wakeUpTime(60)
{
	m_url = "http://192.168.101.8:49154/upnp/control/basicevent1";

	sensor.euid = BELKIN_PREFIX + getMacAddr(m_url);
	sensor.pairs = 1;
	sensor.device_id = BELKIN_SWITCH_INDEX;
}

void Belkin_WeMo::parseCmdFromServer(const Command& cmd)
{
	if (cmd.state == "set"){
		if(cmd.values[0].second == 0)
			turnOff(m_url);
		else if(cmd.values[0].second == 1)
			turnOn(m_url);

		bool state = getState(m_url);
		if(state)
			sensor.values.push_back({BELKIN_MODULE_ID, BELKIN_SWITCH_STATE_ON});
		else
			sensor.values.push_back({BELKIN_MODULE_ID, BELKIN_SWITCH_STATE_OFF});
		msg.device = sensor;
		msg.time = time(NULL);
		agg->sendData(msg);
	}
}

void Belkin_WeMo::turnOn(const string &url)
{
	HTTPClient client;
	string headerOn = "SetBinaryState";
	string bodyOn = R"(<?xml version="1.0" encoding="utf-8"?>)"
	                R"(<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">)"
	                R"(<s:Body>)"
	                R"(<u:SetBinaryState xmlns:u="urn:Belkin:service:basicevent:1">)"
	                R"(<BinaryState>1</BinaryState>)"
	                R"(</u:SetBinaryState>)"
	                R"(</s:Body>)"
	                R"(</s:Envelope>)";
	client.sendRequestPost(url, headerOn, bodyOn);
}

void Belkin_WeMo::turnOff(const string &url)
{
	HTTPClient client;
	string headerOff = "SetBinaryState";
	string bodyOff = R"(<?xml version="1.0" encoding="utf-8"?>)"
	                 R"(<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">)"
	                 R"(<s:Body>)"
	                 R"(<u:SetBinaryState xmlns:u="urn:Belkin:service:basicevent:1">)"
	                 R"(<BinaryState>0</BinaryState>)"
	                 R"(</u:SetBinaryState>)"
	                 R"(</s:Body>)"
	                 R"(</s:Envelope>)";
	client.sendRequestPost(url, headerOff, bodyOff);
}

bool Belkin_WeMo::getState(const string &url)
{
	HTTPClient client;
	string headerGetState = "GetBinaryState";
	string bodyGetState = R"(<?xml version="1.0" encoding="utf-8"?>)"
	                      R"(<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">)"
	                      R"(<s:Body>)"
	                      R"(<u:GetBinaryState xmlns:u="urn:Belkin:service:basicevent:1">)"
	                      R"(<BinaryState></BinaryState>)"
	                      R"(</u:GetBinaryState>")
	                      R"(</s:Body>)"
	                      R"(</s:Envelope>)";
	string response = client.sendRequestPost(url, headerGetState, bodyGetState);

	DOMParser parser;
	AutoPtr<Document> xmlDoc = parser.parseString(response);
	NodeIterator iterator(xmlDoc, NodeFilter::SHOW_ALL);
	Node* xmlNode = iterator.nextNode();

	while(xmlNode) {
		if(xmlNode->nodeName().compare("BinaryState") == 0) {
			xmlNode = iterator.nextNode();
			if(xmlNode->nodeValue().compare("1") == 0)
				return true;
			else if(xmlNode->nodeValue().compare("0") == 0)
				return false;
		}

		xmlNode = iterator.nextNode();
	}
	throw exception();
}

uint64_t Belkin_WeMo::getMacAddr(const string &url)
{
	HTTPClient client;
	string headerGetMacAddr = "GetMacAddr";
	string bodyGetMacAddr = R"(<?xml version="1.0" encoding="utf-8"?>)"
	                        R"(<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">)"
	                        R"(<s:Body>)"
	                        R"(<u:GetMacAddr xmlns:u="urn:Belkin:service:basicevent:1">)"
	                        R"(</u:GetMacAddr>)"
	                        R"(</s:Body>)"
	                        R"(</s:Envelope>)";

	string response = client.sendRequestPost(url, headerGetMacAddr, bodyGetMacAddr);

	DOMParser parser;
	AutoPtr<Document> xmlDoc = parser.parseString(response);
	NodeIterator iterator(xmlDoc, NodeFilter::SHOW_ALL);
	Node* xmlNode = iterator.nextNode();

	while(xmlNode) {
		if(xmlNode->nodeName().compare("MacAddr") == 0) {
			xmlNode = iterator.nextNode();
			return strtoull(xmlNode->nodeValue().c_str(), NULL, 16);
		}

		xmlNode = iterator.nextNode();
	}
	throw exception();
}

bool Belkin_WeMo::isBelkinDevice(euid_t device_id)
{
	return (device_id == sensor.euid);
}

void Belkin_WeMo::threadFunction()
{
	log.information("Starting Belkin_WeMo thread...");

	bool state;

	while(!quit_global_flag) {
		state = getState(m_url);
		if(state)
			sensor.values.push_back({BELKIN_MODULE_ID, BELKIN_SWITCH_STATE_ON});
		else
			sensor.values.push_back({BELKIN_MODULE_ID, BELKIN_SWITCH_STATE_OFF});

		msg.device = sensor;
		msg.time = time(NULL);
		agg->sendData(msg);

		for(unsigned int i = 0; i < m_wakeUpTime; i++) {
			if(quit_global_flag)
				break;
			sleep(1);
		}
	}
}
