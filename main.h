/**
 * @file main.h
 * @Author BeeeOn team
 * @date
 * @brief
 */

#ifndef MAIN_H
#define	MAIN_H

#include <thread>

#include <linux/input.h>
#include <signal.h>

#include <Poco/AutoPtr.h>
#include <Poco/FileChannel.h>
#include <Poco/Logger.h>
#include <Poco/Net/ConsoleCertificateHandler.h>
#include <Poco/Net/KeyConsoleHandler.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Util/IniFileConfiguration.h>

#include "Aggregator.h"
#include "IOcontrol.h"
#include "MosqClient.h"
#include "Openhab.h"
#include "PanInterface.h"
#include "TCP.h"
#include "VirtualSensor.h"
#include "VPT.h"

#endif	/* MAIN_H */
