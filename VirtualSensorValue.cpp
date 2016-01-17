/**
 * @file VirtualSensorValue.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include "VirtualSensorValue.h"

using namespace std;
using namespace Poco;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Timer;

VirtualSensorValue::VirtualSensorValue() :
	from(0),
	to(0),
	step(0),
	change_time(0),
	generator_type(""),
	actual_value(0),
	module_id(0),
	timer(NULL),
	log(Poco::Logger::get("Adaapp-VSv"))
{
}

void VirtualSensorValue::print() {
	log.information("g. type  :" + generator_type);
	log.information("module_id:" + toStringFromInt(module_id));
	log.information("from     :" + toStringFromFloat(from));
	log.information("to       :" + toStringFromFloat(to));
	log.information("step     :" + toStringFromFloat(step));
	log.information("actual   :" + toStringFromFloat(actual_value));
}

void VirtualSensorValue::nextVal(Poco::Timer&) {
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

	actual_value = next_val;
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
