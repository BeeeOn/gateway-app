/**
 * @file main.cpp
 * @Author BeeeOn team
 * @date
 * @brief
 */

#include <Poco/Net/NetSSL.h>
#include <Poco/Util/LoggingConfigurator.h>

#include "main.h"

using namespace std;
using namespace Poco::Net;
using Poco::AutoPtr;
using Poco::Logger;
using Poco::Runnable;
using Poco::Util::IniFileConfiguration;

bool quit_global_flag;

void mosq_thread (shared_ptr<MosqClient> mq) {
	while (!quit_global_flag) {
		int rc = mq->loop();
		if (rc != MOSQ_ERR_SUCCESS) {
			//cout << "MQTT: Trying to reconnect (" << rc << ")" << endl;
			//TODO this whole code must be moved to mosquittoclient.cpp
			mq->reconnect();
			usleep(500000);
		}
	}
}

void killMe(int) {
	printf("User pressed Ctrl+C\n");
	quit_global_flag = true;
}

int main (int, char**) {
	quit_global_flag = false;
	signal(SIGINT, killMe);
	signal(SIGTERM, killMe);

#ifdef LEDS_ENABLED
	LEDControl::initLED(LED_LIME);
	LEDControl::initLED(LED_PAN);
	LEDControl::setLED(LED_LIME, false);
	LEDControl::setLED(LED_PAN, false);
	sleep(1);
	LEDControl::blinkLED(LED_LIME, 3);
 #endif

	long long int adapter_id = 0x0;
	bool mod_pan = false;
	bool mod_virtual_sensor = false;
	bool mod_pressure_sensor = false;
	bool mod_mqtt = false;
	bool mod_vpt = false;
	bool mod_openhab = false;

	AutoPtr<IniFileConfiguration> cfg;
	AutoPtr<IniFileConfiguration> cfg_logging;
	AutoPtr<IniFileConfiguration> cfg_pan;
	AutoPtr<IniFileConfiguration> cfg_virtual_sensor;
	AutoPtr<IniFileConfiguration> cfg_pressure_sensor;
	AutoPtr<IniFileConfiguration> cfg_vpt;
	AutoPtr<IniFileConfiguration> cfg_mqtt;
	AutoPtr<IniFileConfiguration> cfg_hab;

	Poco::Thread agg_thread("Aggregator");
	Poco::Thread vsm_thread("VSM");
	Poco::Thread srv_thread("TCP");
	Poco::Thread vpt_thread("VPT");
	Poco::Thread hab_thread("HAB");

	Logger& log = Poco::Logger::get("Adaapp-MAIN"); // get logging utility

	cerr << "Loading AdaApp.ini from " << CONFIG_FILE << endl;

	/* Load main config file */
	try {
		cfg = new IniFileConfiguration(CONFIG_FILE);
	}
	catch (Poco::Exception& ex) {
		cerr << "Error: Exception with config file reading:" << (string)ex.displayText() << endl;
		return EXIT_FAILURE;
	}

	/* Configure loggers */
	try {
		cfg_logging = new IniFileConfiguration(LOGGERS_CONFIG_FILE);
		Poco::Util::LoggingConfigurator logging_configurator;
		logging_configurator.configure(cfg_logging);
	}
	catch (Poco::Exception& ex) {
		cerr << "Failed to configure logging: "
			<< (string)ex.displayText() << endl;
		Logger::root().setLevel("trace");
	}

	cerr << "Loading modules configurations from " << MODULES_DIR << endl;

	try {
		cfg_pan = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_PAN)+".ini");
		mod_pan = cfg_pan->getBool(string(MOD_PAN)+".enabled", false);
	}
	catch (...) { }

	try {
		cfg_virtual_sensor = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_VIRTUAL_SENSOR)+".ini");
		mod_virtual_sensor = cfg_virtual_sensor->getBool(string(MOD_VIRTUAL_SENSOR)+".enabled", false);
	}
	catch (...) { }

	try {
		cfg_pressure_sensor = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_PRESSURE_SENSOR)+".ini");
		mod_pressure_sensor = cfg_pressure_sensor->getBool(string(MOD_PRESSURE_SENSOR)+".enabled", false);
	}
	catch (...) { }

	try {
		cfg_vpt = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_VPT_SENSOR)+".ini");
		mod_vpt = cfg_vpt->getBool(string(MOD_VPT_SENSOR)+".enabled", false);
	}
	catch (...) { }

	try { // OpenHAB
		cfg_hab = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_OPENHAB)+".ini");
		mod_openhab = cfg_hab->getBool(string(MOD_OPENHAB)+".enabled", false);
	}
	catch (...) { }

	try {
		cfg_mqtt = new IniFileConfiguration(string(MODULES_DIR)+string(MOD_MQTT)+".ini");
		mod_mqtt = cfg_mqtt->getBool(string(MOD_MQTT)+".enabled", false);
	}
	catch (...) { }

#ifndef PC_PLATFORM
	EEPROM_ITEMS eeprom = parseEEPROM();

	for (auto item : eeprom.items) {
		if (item.first == 0x01) {
			adapter_id = item.second;
			break;
		}
	}
#endif

	// Try to find adapter_id in configuration file
	// TODO this is temporal solution for PC platform
	if (adapter_id <= 0)
		adapter_id = toNumFromString(cfg->getString("adapter.id", "0x0"));

	// Fail if there is no valid adapter_id
	if (adapter_id <= 0) {
		log.fatal("Error: Adapter ID is not a valid number - quit! (ID = " + toStringFromLongHex(adapter_id) + ")");
		return EXIT_FAILURE;
	}

	/* Print loaded modules */
	log.information("Starting Adapter (ID: " + toStringFromLongHex(adapter_id) + ", FW_VERSION: " + FW_VERSION + ") with these modules:");
	log.information(MOD_PAN             + ":             " + ((mod_pan) ? " ON" : "OFF"));
	log.information(MOD_PRESSURE_SENSOR + ": " + ((mod_pressure_sensor) ? " ON" : "OFF"));
	log.information(MOD_VIRTUAL_SENSOR  + ":  " + ((mod_virtual_sensor) ? " ON" : "OFF"));
	log.information(MOD_MQTT            + ":            " + ((mod_mqtt) ? " ON" : "OFF"));
	log.information(MOD_VPT_SENSOR      + ":      " +        ((mod_vpt) ? " ON" : "OFF"));
	log.information(MOD_OPENHAB         + ":         " + ((mod_openhab) ? " ON" : "OFF"));

	srand(time(0));

	/* Create default "header" for messages which are sent to server - these parameters are seldomly changed during runtime */
	IOTMessage msg;
	msg.adapter_id = toStringFromLongHex(adapter_id);
	msg.fw_version = FW_VERSION;
	msg.debug = false;
	msg.priority = MSG_PRIO_SENSOR;
	msg.valid = true;
	msg.offset = 0;
	msg.tt_version = 0;

	string IP_addr_out = cfg->getString("server.ip");
	int port_out = cfg->getInt("server.port");

	/* SSL stuff - Lukas Koszegy */
	Poco::SharedPtr<PrivateKeyPassphraseHandler> pConsoleHandler = new KeyConsoleHandler(true);
	Poco::SharedPtr<InvalidCertificateHandler> pInvalidCertHandler = new ConsoleCertificateHandler(true);
	Context::Ptr pContext = NULL;
	try {
		pContext = new Context(Context::CLIENT_USE, cfg->getString("ssl.key", ""), cfg->getString("ssl.certificate", ""), cfg->getString("ssl.calocation", "./"), (Context::VerificationMode) cfg->getInt("ssl.verify_level", Context::VERIFY_RELAXED), 9, false, "ALL:ADH:!LOW:!EXP:!MD5:@STRENGTH");
	} catch (Poco::Exception& ex) {
		log.fatal("Creating SSL failed! Please check cerficate file and configuration! (" + (string)ex.displayText() + ")");
		return (EXIT_FAILURE);
	}
	SSLManager::instance().initializeClient(0, pInvalidCertHandler, pContext);
	initializeSSL();
	/* Module initialization, start threads, wait in loop and test for termination of the app */
	try {
		shared_ptr<MosqClient> mosq;
		shared_ptr<PanInterface> pan;
		shared_ptr<VPTSensor> vptsensor;
		shared_ptr<OpenHAB> hab;
		shared_ptr<PressureSensor> psm;
		shared_ptr<VirtualSensorModule> vsm;

		std::thread mq_t;

		if (mod_mqtt) {
			log.information("Starting MQTT module.");
			std::string mq_client_id =  cfg_mqtt->getString("mqtt.client_id", "AdaApp");
			std::string mq_main_topic = cfg_mqtt->getString("mqtt.main_topic", "BeeeOn/#");
			std::string mq_host_addr =  cfg_mqtt->getString("mqtt.host_addr", "localhost");
			int mq_port =               cfg_mqtt->getInt("mqtt.port", 1883);

			log.information("Starting Mosquitto Client.");
			mosq.reset(new MosqClient(mq_client_id, mq_main_topic, "AdaApp", mq_host_addr, mq_port));
			mq_t = std::thread(mosq_thread, mosq);
		}

		/* Mandatory module for sending and receiving data */
		shared_ptr<Aggregator> agg (new Aggregator(msg, mosq));

		msg.state = "register";
		msg.time = time(NULL);
		/* Mandatory component for receiving asynchronous messages from server */
		shared_ptr<IOTReceiver> receiver (new IOTReceiver(agg, IP_addr_out, port_out, msg, adapter_id));
		receiver->keepaliveInit(cfg);
		agg->setTCP(receiver);

		// BeeeOn PAN coordinator module
		if (mod_pan) {
			log.information("Creating PAN module.");
			pan.reset(new PanInterface(msg, agg));
			pan->set_pan(pan); // XXX Function name should be same
			agg->setPAN(pan);
		}

		if (mod_vpt) {
			log.information("Creating VPT module.");
			vptsensor.reset(new VPTSensor(msg, agg, adapter_id));
			agg->setVPT(vptsensor);
		}

		if (mod_openhab && mod_mqtt) {
			log.information("Creating OpenHAB module.");
			hab.reset(new OpenHAB(msg, agg));
			agg->setHAB(hab);
		}

		if (mod_pressure_sensor) {
			log.information("Creating PressureSensors module.");
			psm.reset(new PressureSensor(msg, agg));
			agg->setPSM(psm);
		}

		if (mod_virtual_sensor) {
			log.information("Creating VirtualSensors module.");
			vsm.reset(new VirtualSensorModule(msg, agg));
			agg->setVSM(vsm);
		}

		agg_thread.start(*agg.get());
		srv_thread.start(*receiver.get());

		if (mod_vpt) {
			log.information("Starting VPT module.");
			vpt_thread.start(*vptsensor.get());
		}

		if (mod_openhab && mod_mqtt) {
			log.information("Starting OpenHAB module.");
			hab_thread.start(*hab.get());
		}

		if (mod_pressure_sensor) {
			log.information("Starting PressureSensors module.");
			psm.get()->start();
		}

		if (mod_virtual_sensor) {
			log.information("Starting VirtualSensors module.");
			vsm_thread.start(*vsm.get());
		}

		/* "Endless" loop waiting for SIGINT/SIGTERM */
		while (!quit_global_flag) {
			sleep(1);
		}

		log.information("Stopping modules...");

		if (mod_virtual_sensor) {
			log.information("Stopping Virtual Sensor module...");
			vsm_thread.join();
		}

		if (mod_pressure_sensor) {
			log.information("Stopping Pressure Sensor module...");
			psm.get()->stop();
		}

		if (mod_vpt) {
			log.information("Stopping VPT Sensor module...");
			vpt_thread.join();
		}

		if (mod_openhab && mod_mqtt) {
			log.information("Stopping OpenHAB module...");
			hab_thread.join();
		}

		if (mod_mqtt) {
			log.information("Stopping MQTT...");
			mq_t.join();
		}

		log.information("Stopping server...");
		srv_thread.join();

		log.information("Stopping aggregator...");
		agg_thread.join();

		uninitializeSSL();

	}
	catch (Poco::Exception& ex) {
		log.fatal("Error: Poco exception!" + (string)ex.displayText());
		return (EXIT_FAILURE);
	}
	catch (std::exception& ex) {
		log.fatal("Error: Standard expcetion!\n" + (string)ex.what());
		return (EXIT_FAILURE);
	}
	log.information("Adapter finished");
	return 0;
}
