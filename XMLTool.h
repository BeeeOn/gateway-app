/**
 * @file XMLTool.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef XMLTOOL_H
#define	XMLTOOL_H

#include <fstream>
#include <tuple>

#include <Poco/AutoPtr.h>
#include <Poco/DOM/AutoPtr.h>
#include <Poco/DOM/Document.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/NamedNodeMap.h>
#include <Poco/DOM/NodeFilter.h>
#include <Poco/DOM/NodeIterator.h>
#include <Poco/Exception.h>
#include <Poco/Logger.h>
#include <Poco/SAX/AttributesImpl.h>
#include <Poco/SAX/InputSource.h>
#include <Poco/UTF8Encoding.h>
#include <Poco/XML/XMLWriter.h>

#include "utils.h"

/**
 * Parse XML data. It can generate XML messages according to defined protocol and parse them.
 */
class XMLTool {
public:
	XMLTool();
	XMLTool(ServerMessage);
	std::string createXML(int);
	ServerCommand parseXML(std::string);
	virtual ~XMLTool();

private:
	ServerMessage msg;
	void createDevice(Poco::XML::XMLWriter*, Device, bool debug=false, std::string proto="", std::string fw="");
	void createParam(Poco::XML::XMLWriter* w, CmdParam dev, std::string state);
	Poco::Logger& log;
};

#endif	/* XMLTOOL_H */
