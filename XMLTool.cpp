/**
 * @file XMLTool.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include "device_table.h"
#include "XMLTool.h"

using namespace std;
using namespace Poco::XML;
using Poco::AutoPtr;
using Poco::Logger;

/**
 * Constructor of XML for cases when we need to parse string and create Command structure from it.
 */
XMLTool::XMLTool() :
	msg(),
	log(Poco::Logger::get("Adaapp-XML"))
{
}

/**
 * Constructor of XML for cases when we need to create message from incomming data.
 */
XMLTool::XMLTool(IOTMessage _msg) :
	msg(_msg),
	log(Poco::Logger::get("Adaapp-XML"))
{
}

/**
 * Create message which can be sent to server.
 * @param type Message type (0 - A_TO_S, 1 - INIT, 2 - PARAM)
 * @return Created message in string
 */
string XMLTool::createXML(int type) {
		stringstream stream;
		Poco::UTF8Encoding utf8;

	try {
		XMLWriter writer(stream, XMLWriter::WRITE_XML_DECLARATION | XMLWriter::PRETTY_PRINT, "UTF-8", &utf8);  // Print XML header and add newline for each tag
		writer.setNewLine("\n");
		AttributesImpl attrs;
		writer.startDocument();

		attrs.addAttribute("", "", "adapter_id", "", msg.adapter_id); // TODO!! remove
		attrs.addAttribute("", "", "state", "", msg.state);
		attrs.addAttribute("", "", "protocol_version", "", msg.protocol_version);
		attrs.addAttribute("", "", "fw_version", "", msg.fw_version);
		attrs.addAttribute("", "", "time", "", toStringFromLongInt(msg.time));
		if (type == INIT) {
			// TODO attrs.addAttribute("", "", "adapter_id", "", msg.adapter_id);
			// TODO send even if I don't have it yet? attrs.addAttribute("", "", "tt_version", "", toStringFromLongInt(msg.tt_version));
		}
		writer.startElement("", "adapter_server", "", attrs);

		if (type == A_TO_S) {
			createDevice(&writer, msg.device);  // TODO add fw_version etc?
		}
		else if (type == INIT) {
			writer.characters(" ");
		}
		else if (type == PARAM) {
			createParam(&writer, msg.params, msg.state);
		}

		writer.endElement("", "adapter_server", "");
		writer.endDocument();
	}
	catch (Poco::Exception& ex) {
		log.error("*** Exception: \n" + ex.displayText());
	}
	 return stream.str();
}

/**
 * Internal function for creating the <device> element filled with values
 * @param w Pointer to XMLWriter
 * @param dev Device data
 * @param debug Flag - add debug mark
 * @param proto Communication protocol version
 * @param fw Firmware version of device
 */
void XMLTool::createDevice(XMLWriter* w, Device dev, bool debug, string proto, string fw) {
	AttributesImpl att;
	att.addAttribute("", "", "euid", "", toStringFromLongHex(dev.euid));
	att.addAttribute("", "", "device_id", "", toStringFromLongHex(dev.device_id, 2));
	if(dev.name != ""){
		att.addAttribute("", "", "name", "", dev.name);
	}

	w->startElement("", "device", "", att);

	if (debug)
		w->dataElement("", "", "debug", "", "protocol_version", proto, "fw_version", fw);

	TT_Table tt = fillDeviceTable();
	auto tt_device = tt.find(dev.device_id);
	if (tt_device == tt.end())
		throw Poco::Exception("Missing device in types table"); // FIXME temporary "fix", do it properly
	auto tt_dev = tt_device->second;

	if (dev.values.size()) { // If there are some values
		AttributesImpl attVal;
		attVal.addAttribute("", "", "count", "", toStringFromInt(dev.values.size()));
		w->startElement("", "values", "", attVal);
		for (auto item : dev.values){
			if(item.status == true){
				w->dataElement("", "", "value", toStringFromFloat(item.value), "module_id", toStringFromHex(item.mid));   // convert to hex format (0x00)
			} else {
				w->dataElement("", "", "value", toStringFromFloat(item.value), "module_id", toStringFromHex(item.mid), "status", "unavailable");
			}
		}
		w->endElement("", "values", "");
	}
	w->endElement("", "device", "");
}

void XMLTool::createParam(XMLWriter* w, CmdParam par, string state){
	AttributesImpl att;
	att.addAttribute("", "", "param_id", "", toStringFromInt(par.param_id));
	if (par.euid > 0)
		att.addAttribute("", "", "euid", "", toStringFromLongHex(par.euid));
	if (par.module_id >= 0)
		att.addAttribute("", "", "module_id", "", toStringFromLongHex(par.module_id));

	w->startElement("", "parameter", "", att);

	if (state == "parameters" && par.value.size()) {
		for (auto item: par.value){
			w->dataElement("", "", "value", item.first);
		}
	}
	w->endElement("", "parameter", "");
}

/**
 * Parse incomming message (from server).
 * @param str Incomming server message
 * @return Message in Command structure
 */
Command XMLTool::parseXML(string str) {
	Command cmd;

	try  {
		DOMParser parser = DOMParser(0);
		AutoPtr<Document> pDoc = parser.parseString(str);
		NodeIterator it(pDoc, NodeFilter::SHOW_ELEMENT);
		Node* pNode = it.nextNode();
		NamedNodeMap* attributes = NULL;
		Node* attribute = NULL;

		while (pNode)  {
			if (pNode->nodeName().compare("server_adapter") == 0) {
				if (pNode->hasAttributes()) {
					attributes = pNode->attributes();
					for(unsigned int i = 0; i < attributes->length(); i++) {
						attribute = attributes->item(i);
						if (attribute->nodeName().compare("protocol_version") == 0) {
							cmd.protocol_version = attribute->nodeValue();
						}

						else if (attribute->nodeName().compare("state") == 0) {
							cmd.state = attribute->nodeValue();
						}
						// FIXME - id attribute is here only for backward compatibility, it should be removed in Q1/2016
						else if (attribute->nodeName().compare("euid") == 0 || attribute->nodeName().compare("id") == 0) {
							cmd.euid = stoull(attribute->nodeValue(), nullptr, 0);
						}

						else if (attribute->nodeName().compare("device_id") == 0) {
							cmd.device_id = atoll(attribute->nodeValue().c_str());
						}

						else if (attribute->nodeName().compare("time") == 0) {
							cmd.time = atoll(attribute->nodeValue().c_str());
						}

						else {
							log.error("Unknow attribute for SERVER_ADAPTER : " + fromXMLString(attribute->nodeName()));
						}
					}
					attributes->release();
				}
			}

			else if (pNode->nodeName().compare("value") == 0) {
				if(cmd.state == "getparameters" || cmd.state == "parameters"){
					string inner = pNode->innerText();
					string device_id = "";

					if (pNode->hasAttributes()) {
						attributes = pNode->attributes();
						string device_id = "";
						for(unsigned int i = 0; i < attributes->length(); i++) {
							attribute = attributes->item(i);
							if (attribute->nodeName().compare("device_id") == 0) {
								device_id = toNumFromString(attribute->nodeValue());
							}
							if (attribute->nodeName().compare("module_id") == 0) {
								cmd.params.module_id = toNumFromString(attribute->nodeValue());
							}
						}
						attributes->release();
					}

					if (!inner.empty() || !device_id.empty())
						cmd.params.value.push_back({inner, device_id});

				}
				else {
					float val = atof(pNode->innerText().c_str());

					if (pNode->hasAttributes()) {
						int module_id = 0;
						attributes = pNode->attributes();
						for(unsigned int i = 0; i < attributes->length(); i++) {
							attribute = attributes->item(i);
							if (attribute->nodeName().compare("module_id") == 0) {
								module_id = toNumFromString(attribute->nodeValue());
							}
						}
						cmd.values.push_back({module_id, val});  //TODO Hex number is processed wrongly
						attributes->release();
					}
				}
			}
			else if (pNode->nodeName().compare("parameter") == 0) {
				if (pNode->hasAttributes()) {
					attributes = pNode->attributes();
					for(unsigned int i = 0; i < attributes->length(); i++) {
						attribute = attributes->item(i);
						if (attribute->nodeName().compare("param_id") == 0 || attribute->nodeName().compare("id") == 0) {
							cmd.params.param_id = toIntFromString(attribute->nodeValue());
						}
						else if (attribute->nodeName().compare("euid") == 0) {
							cmd.params.euid = toNumFromString(attribute->nodeValue());
						}
					}
					attributes->release();
				}
			}
			pNode = it.nextNode();
		}
	}
	catch (Poco::Exception& e) {
		log.error("Invalid format of incoming message!" + e.displayText());
		cmd.state = "error";
	}
	return cmd;
}

/**
 * Destructor
 */
XMLTool::~XMLTool() {
}
