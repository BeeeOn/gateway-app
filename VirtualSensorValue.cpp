/**
 * @file VirtualSensorValue.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include <memory>
#include <unistd.h>

#include "Aggregator.h"
#include "VirtualSensorValue.h"

using namespace std;
using namespace Poco;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Timer;
using namespace Poco::Util;

VirtualSensorValue::VirtualSensorValue() :
	is_actuator(false),
	from(0),
	to(0),
	step(0),
	change_time(0),
	switch_time(0),
	generator_type(""),
	actual_value(0),
	module_id(0),
	timer(NULL),
	log(Poco::Logger::get("Adaapp-VS"))
{
	value_change_mutex.reset(new Poco::FastMutex);
}

void VirtualSensorValue::print() {
	log.information("actuator :" + std::string(is_actuator ? "TRUE" : "FALSE"));
	log.information("g. type  :" + generator_type);
	log.information("module_id:" + toStringFromInt(module_id));
	log.information("from     :" + toStringFromFloat(from));
	log.information("to       :" + toStringFromFloat(to));
	log.information("step     :" + toStringFromFloat(step));
	log.information("actual   :" + toStringFromFloat(actual_value));
}

void VirtualSensorValue::setNewVal(float val) {
	if(!is_actuator)
		return;

	value_change_mutex->lock();     // anyone can change value untill process is complete
		log.information("VirtualSensorValue request: set value to "+std::to_string(val));
		log.information("    ---> wait for switch time : "+std::to_string(switch_time)+" s");
		for (int i = 0; i < switch_time; i++) {    // sleep for switch time, then set new value
			if (quit_global_flag)
				break;
			sleep(1);
		}
		if (!quit_global_flag)
		{
			actual_value = val;
			log.information("VirtualSensorValue request done: new value is "+std::to_string(val));
		}
	value_change_mutex->unlock();
}

void VirtualSensorValue::nextVal(Poco::Timer&) {
	value_change_mutex->lock();
	float next_val = actual_value;
	float f = from;
	float t = to;

	if (f > t)
		swap(f,t);

	if (generator_type == "linear") {
		next_val += step;
		if (next_val > t) {
			next_val = t;
			this->timer->stop(); // Final value hasbeen reached, no need to generate more events
			return;
		}
		else if (next_val < f) {
			next_val = f;
			this->timer->stop();
			return;
		}
	}

	if (generator_type == "triangle") {
		next_val += step;
		if (next_val > t) {
			next_val = t;
			step *= -1;                 // Reverse to create the saw
		}
		else if (next_val < f) {
			next_val = f;
			step *= -1;
		}
	}
	else if (generator_type == "random") {
		next_val = (rand() % (int) (t-f)) + f ;
	}

	for (int i = 0; i < switch_time; i++) {    // sleep for switch time, then set new value
		if (quit_global_flag)
			break;
		sleep(1);
	}
	if (quit_global_flag)
		return;
	actual_value = next_val;
	value_change_mutex->unlock();
}

void VirtualSensorValue::startTimer() {
	timer = new Poco::Timer(change_time*1000, change_time*1000);  // *1000 because of conversion to ms
	timer->start(TimerCallback<VirtualSensorValue>(*this, &VirtualSensorValue::nextVal));
}

VirtualSensorValue::~VirtualSensorValue() {
	if (timer){
		timer->stop();
		delete timer;
	}
}
