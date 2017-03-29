#ifndef BELKIN_WEMO_H
#define BELKIN_WEMO_H

#define BELKIN_PREFIX ((uint64_t)0xa7 << 48)

#include "ModuleADT.h"

class Aggregator;

class Belkin_WeMo : public ModuleADT {
public:
	Belkin_WeMo(IOTMessage _msg, std::shared_ptr<Aggregator> _agg);
	void parseCmdFromServer(const Command& cmd);
	bool isBelkinDevice(euid_t device_id);

	void turnOn(const std::string &url);
	void turnOff(const std::string &url);
	bool getState(const std::string &url);
	uint64_t getMacAddr(const std::string &url);
	void threadFunction() override;

private:
	unsigned int m_wakeUpTime;
	std::string m_url;
};
#endif
