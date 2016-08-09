/*
 * @file IODaemonMsg.h
 * @Author BeeeOn team - Richard Wolfert (wolfert.richard@gmail.com)
 * @date July 2016
 * @brief Class representing messages from BeeeOn ioDaemon
 */

#pragma once

#include <iostream>
#include <string>

#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Object.h>
#include <Poco/Logger.h>

class ledConfiguration {
public:
	bool m_red;
	bool m_green;
	unsigned char m_blue;
};

class IODaemonMsg {
public:
	IODaemonMsg();

	void debug_print(Poco::Logger& log);
	void parse(const std::string &data, Poco::Logger& log);
	std::string createDaemonMsg() const;

	bool isUpdate() const
	{
		return (m_eventName == "updateLeds");
	}

	bool isValid() const
	{
		return m_validity;
	}

	void setValidity(const bool &validity)
	{
		m_validity = validity;
	}

	std::string getEventName() const
	{
		return m_eventName;
	}

	void setEventName(const std::string &eventName)
	{
		m_eventName = eventName;
	}

	std::string getAction() const
	{
		return m_action;
	}

	void setAction(const std::string &action)
	{
		m_action = action;
	}

	std::string getSenderName() const
	{
		return m_senderName;
	}

	void setSenderName(const std::string &senderName)
	{
		m_senderName = senderName;
	}

	unsigned char getPriority() const
	{
		return m_priority;
	}

	void setPriority(const unsigned char &priority)
	{
		m_priority = priority;
	}

	ledConfiguration getLedConf() const
	{
		return m_ledConf;
	}

	void setLedConf(const ledConfiguration &ledConf)
	{
		m_ledConf = ledConf;
	}

private:
	std::string m_eventName;
	std::string m_action;
	std::string m_senderName;
	unsigned char m_priority;
	ledConfiguration m_ledConf;
	bool m_validity;

	Poco::JSON::Object::Ptr parseJsonObject(const std::string &data);
	std::string extractString(const Poco::JSON::Object::Ptr &jsonObject, const std::string &key);
	unsigned char extractChar(const Poco::JSON::Object::Ptr &jsonObject, const std::string &key);
	ledConfiguration parseLedConfiguration(const Poco::JSON::Object::Ptr &jsonObject, const std::string &key);
};
