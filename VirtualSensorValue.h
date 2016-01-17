/**
 * @file VirtualSensorValue.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef VIRTUALSENSORVALUE_H
#define	VIRTUALSENSORVALUE_H

#include <iostream>

#include <Poco/AutoPtr.h>
#include <Poco/Logger.h>
#include <Poco/Thread.h>
#include <Poco/Timer.h>

#include "utils.h"

/**
 * Class for particular sensor value. If there is more "complex" value (other than constant), thread is started.
 */
class VirtualSensorValue {
public:
	float from;
	float to;
	float step;
	long int change_time;
	std::string generator_type; // XXX enum might be better
	float actual_value;
	int module_id;
	Poco::Timer* timer;
	Poco::Logger& log;

	VirtualSensorValue();
	virtual ~VirtualSensorValue();
	void print();
	void nextVal(Poco::Timer&);
	void startTimer();
};

#endif	/* VIRTUALSENSORVALUE_H */
