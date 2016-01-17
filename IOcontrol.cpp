/**
 * @file IOcontrol.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include <iostream>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "IOcontrol.h"


#define BLINK_PERIOD 50000      // us
#define EVENT_TIME_THRESHOLD 4  // s
#define READ_TIMEOUT_SECONDS 1

#ifdef LEDS_ENABLED
void blink(std::string led, unsigned int blinks_count) {
	bool val = false;
	for (unsigned int i = 0; i < 4*blinks_count; i++) {
		LEDControl::setLED(led, val);
		usleep(BLINK_PERIOD);
		val = !val;
	}
}

// Set LED trigger
void LEDControl::initLED(std::string led) {
	std::string cmd = "/bin/echo \"default-on\" > " + led + "trigger";
	system(cmd.c_str());
}

// Set LED either to ON or OFF
void LEDControl::setLED(std::string led, bool val, long int timeout) {
	usleep(timeout);
	std::string cmd = std::string("/bin/echo ") + (val ? "1" : "0") + std::string(" > ") + led + std::string("brightness");
	system(cmd.c_str());
}

void LEDControl::setLEDAfterTimeout(std::string led, bool val, long int timeout) {
		std::thread t(LEDControl::setLED, led, val, timeout);
		t.detach();
}

// Non-blocking flashing of LED
void LEDControl::blinkLED(std::string led, unsigned int blinks_count) {
	std::thread t(blink, led, blinks_count);
	t.detach();
}
#endif

int waitForEvent(void (*callback)(int)) {
	int fd;
	int rd;
	unsigned int i;

	fd_set set;
	struct timeval timeout;
	int rv;

	struct input_event ev[64];
#ifdef PC_PLATFORM
	if ((fd = open("/dev/input/event3", O_RDONLY)) < 0) {
#else
	if ((fd = open("/dev/input/event0", O_RDONLY)) < 0) {
#endif
		return 1;
	}

	std::cout << "Try to push button (F1 key)... (interrupt to exit)" << std::endl;

	__time_t start_time = 0;
	while (!quit_global_flag) {
		FD_ZERO(&set);
		FD_SET(fd, &set);

		timeout.tv_sec = READ_TIMEOUT_SECONDS;
		timeout.tv_usec = 0;
		rv = select(fd + 1, &set, NULL, NULL, &timeout);
		if (rv == -1)
			break; /*IO error*/
		else if (rv == 0)
			continue; /*timeout*/
		else
			rd = read(fd, ev, sizeof(struct input_event) * 64);

		if (rd < (int) sizeof(struct input_event))
			return 1;

		for (i = 0; i < rd / sizeof(struct input_event); i++) {
			if ((ev[i].type == EV_KEY) && (ev[i].code == KEY_F1)) {
				/*printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
					ev[i].time.tv_sec, ev[i].time.tv_usec, ev[i].type, ("EVENT"),
					ev[i].code,
					"KEY",
					ev[i].value);*/
				if (ev[i].value) {
					start_time = ev[i].time.tv_sec;
				}
				else {
					if ((ev[i].time.tv_sec - start_time) > EVENT_TIME_THRESHOLD) {
						std::cout << "EVENT: long push by KEY_F1" << std::endl;
						//agg->event("F1", "long");
						callback(BUTTON_EVENT_LONG);
					}
					else {
						std::cout << "EVENT: short push by KEY_F1" << std::endl;
						//agg->event("F1", "short");
						callback(BUTTON_EVENT_SHORT);
					}
				}
			}
		}
	}
	return 0;
}
