/**
 * @file IOcontrol.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef IOCONTROL_H
#define	IOCONTROL_H

#include <thread>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include "utils.h"

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

#ifndef EV_SYN
#define EV_SYN 0
#endif

extern bool quit_global_flag;

int waitForEvent(void (*)(int));

#ifndef PC_PLATFORM
/**
 * Class for LED control
 */
class LEDControl {
public:
	static void initLED(std::string led);
	static void blinkLED(std::string led, unsigned int blinks_count=5);
	static void setLED(std::string led, bool val, long int timeout=0);
	static void setLEDAfterTimeout(std::string led, bool val, long int timeout);
};
#endif

#endif	/* IOCONTROL_H */
