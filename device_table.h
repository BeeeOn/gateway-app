#ifndef DEVICE_TABLE_H
#define DEVICE_TABLE_H

#include "utils.h"

using TT_Table = std::map<int, TT_Device>;

/**
 * @brief Generovany soubor s funkci pro vraceni nove tabulky typu, ktera je slozena z devices (multisenzory) a modules (fyzicke senzory na device).
 * @return Vraci mapu devices, ktere obsahuji jejich moduly.
 */
inline TT_Table fillDeviceTable() {

	// list of defined TT_Devices and theirs modules
	std::map<int, TT_Device> devices;
	std::map<int, TT_Module> modules;

	modules.insert( {0, TT_Module(0, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {1, TT_Module(1, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {2, TT_Module(2, 0x03, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	// FIXME Battery conversion is edited by hand here. Don't forget to fix source files on next update.
	modules.insert( {3, TT_Module(3, 0x08, 2, false, "/1000", "*1000", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {4, TT_Module(4, 0x09, 1, false, "", "", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {5, TT_Module(5, 0x0A, 2, true, "", "", {true, 5.0}, {true, 3600.0}, {}) } );
	devices.insert( {0, TT_Device(0, modules)} ); // insert device to map of devices
	modules.clear();

	modules.insert( {0, TT_Module(0, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {1, TT_Module(1, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {2, TT_Module(2, 0x02, 4, true, "/100", "*100", {true, 0.0}, {true, 160.0}, {}) } );
	modules.insert( {3, TT_Module(3, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {4, TT_Module(4, 0x02, 4, true, "/100", "*100", {true, 20.0}, {true, 90.0}, {}) } );
	modules.insert( {5, TT_Module(5, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {6, TT_Module(6, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {7, TT_Module(7, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {8, TT_Module(8, 0x02, 4, true, "/100", "*100", {true, 0.0}, {true, 160.0}, {}) } );
	modules.insert( {9, TT_Module(9, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {10, TT_Module(10, 0x02, 4, true, "/100", "*100", {true, 20.0}, {true, 90.0}, {}) } );
	modules.insert( {11, TT_Module(11, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {12, TT_Module(12, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {13, TT_Module(13, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {14, TT_Module(14, 0x02, 4, true, "/100", "*100", {true, 0.0}, {true, 160.0}, {}) } );
	modules.insert( {15, TT_Module(15, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {16, TT_Module(16, 0x02, 4, true, "/100", "*100", {true, 20.0}, {true, 90.0}, {}) } );
	modules.insert( {17, TT_Module(17, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {18, TT_Module(18, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {19, TT_Module(19, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {20, TT_Module(20, 0x02, 4, true, "/100", "*100", {true, 0.0}, {true, 160.0}, {}) } );
	modules.insert( {21, TT_Module(21, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {22, TT_Module(22, 0x02, 4, true, "/100", "*100", {true, 20.0}, {true, 90.0}, {}) } );
	modules.insert( {23, TT_Module(23, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {24, TT_Module(24, 0x01, 1, false, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {25, TT_Module(25, 0x09, 1, false, "", "", {false, 0.0}, {false, 0.0}, {}) } );
	devices.insert( {1, TT_Device(1, modules)} ); // insert device to map of devices
	modules.clear();

	modules.insert( {0, TT_Module(0, 0x04, 2, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {1, TT_Module(1, 0x0A, 2, true, "", "", {true, 5.0}, {true, 3600.0}, {}) } );
	devices.insert( {2, TT_Device(2, modules)} ); // insert device to map of devices
	modules.clear();

	modules.insert( {0, TT_Module(0, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {1, TT_Module(1, 0x03, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {2, TT_Module(2, 0x06, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	devices.insert( {3, TT_Device(3, modules)} ); // insert device to map of devices
	modules.clear();

	modules.insert( {0, TT_Module(0, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {1, TT_Module(1, 0x03, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {2, TT_Module(2, 0x06, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {3, TT_Module(3, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {4, TT_Module(4, 0x03, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {5, TT_Module(5, 0x06, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {6, TT_Module(6, 0x02, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {7, TT_Module(7, 0x03, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	modules.insert( {8, TT_Module(8, 0x06, 4, false, "/100", "*100", {false, 0.0}, {false, 0.0}, {}) } );
	devices.insert( {4, TT_Device(4, modules)} ); // insert device to map of devices
	modules.clear();

	modules.insert( {0, TT_Module(0, 0x01, 1, false, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	devices.insert( {5, TT_Device(5, modules)} ); // insert device to map of devices
	modules.clear();

	modules.insert( {0, TT_Module(0, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {1, TT_Module(1, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {2, TT_Module(2, 0x02, 4, false, "/100", "*100", {true, 0.0}, {true, 160.0}, {}) } );
	modules.insert( {3, TT_Module(3, 0x02, 4, false, "/100", "*100", {true, -20.0}, {true, 40.0}, {}) } );
	modules.insert( {4, TT_Module(4, 0x02, 4, true, "/100", "*100", {true, 50.0}, {true, 90.0}, {}) } );
	modules.insert( {65, TT_Module(65, 0x02, 4, false, "/100", "*100", {true, 10.0}, {true, 110.0}, {}) } );
	modules.insert( {5, TT_Module(5, 0x02, 4, false, "/100", "*100", {true, 20.0}, {true, 120.0}, {}) } );
	modules.insert( {11, TT_Module(11, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1}) } );
	modules.insert( {12, TT_Module(12, 0x02, 4, true, "/100", "*100", {true, 25.0}, {true, 50.0}, {}) } );
	modules.insert( {13, TT_Module(13, 0x02, 4, true, "/100", "*100", {true, 0.0}, {true, 15.0}, {}) } );
	modules.insert( {14, TT_Module(14, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {15, TT_Module(15, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {16, TT_Module(16, 0x02, 4, false, "/100", "*100", {true, 0.0}, {true, 160.0}, {}) } );
	modules.insert( {17, TT_Module(17, 0x02, 4, false, "/100", "*100", {true, -20.0}, {true, 40.0}, {}) } );
	modules.insert( {18, TT_Module(18, 0x02, 4, true, "/100", "*100", {true, 50.0}, {true, 90.0}, {}) } );
	modules.insert( {66, TT_Module(66, 0x02, 4, false, "/100", "*100", {true, 10.0}, {true, 110.0}, {}) } );
	modules.insert( {19, TT_Module(19, 0x02, 4, false, "/100", "*100", {true, 20.0}, {true, 120.0}, {}) } );
	modules.insert( {25, TT_Module(25, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1}) } );
	modules.insert( {26, TT_Module(26, 0x02, 4, true, "/100", "*100", {true, 25.0}, {true, 50.0}, {}) } );
	modules.insert( {27, TT_Module(27, 0x02, 4, true, "/100", "*100", {true, 0.0}, {true, 15.0}, {}) } );
	modules.insert( {28, TT_Module(28, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {29, TT_Module(29, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {30, TT_Module(30, 0x02, 4, false, "/100", "*100", {true, 0.0}, {true, 160.0}, {}) } );
	modules.insert( {31, TT_Module(31, 0x02, 4, false, "/100", "*100", {true, -20.0}, {true, 40.0}, {}) } );
	modules.insert( {32, TT_Module(32, 0x02, 4, true, "/100", "*100", {true, 50.0}, {true, 90.0}, {}) } );
	modules.insert( {67, TT_Module(67, 0x02, 4, false, "/100", "*100", {true, 10.0}, {true, 110.0}, {}) } );
	modules.insert( {33, TT_Module(33, 0x02, 4, false, "/100", "*100", {true, 20.0}, {true, 120.0}, {}) } );
	modules.insert( {39, TT_Module(39, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1}) } );
	modules.insert( {40, TT_Module(40, 0x02, 4, true, "/100", "*100", {true, 25.0}, {true, 50.0}, {}) } );
	modules.insert( {41, TT_Module(41, 0x02, 4, true, "/100", "*100", {true, 0.0}, {true, 15.0}, {}) } );
	modules.insert( {42, TT_Module(42, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {43, TT_Module(43, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {44, TT_Module(44, 0x02, 4, false, "/100", "*100", {true, 0.0}, {true, 160.0}, {}) } );
	modules.insert( {45, TT_Module(45, 0x02, 4, false, "/100", "*100", {true, -20.0}, {true, 40.0}, {}) } );
	modules.insert( {46, TT_Module(46, 0x02, 4, true, "/100", "*100", {true, 50.0}, {true, 90.0}, {}) } );
	modules.insert( {68, TT_Module(68, 0x02, 4, false, "/100", "*100", {true, 10.0}, {true, 110.0}, {}) } );
	modules.insert( {47, TT_Module(47, 0x02, 4, false, "/100", "*100", {true, 20.0}, {true, 120.0}, {}) } );
	modules.insert( {53, TT_Module(53, 0x01, 1, true, "", "", {false, 0.0}, {false, 0.0}, {0, 1}) } );
	modules.insert( {54, TT_Module(54, 0x02, 4, true, "/100", "*100", {true, 25.0}, {true, 50.0}, {}) } );
	modules.insert( {55, TT_Module(55, 0x02, 4, true, "/100", "*100", {true, 0.0}, {true, 15.0}, {}) } );
	modules.insert( {56, TT_Module(56, 0x01, 1, false, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2, 3, 4}) } );
	modules.insert( {57, TT_Module(57, 0x01, 1, false, "", "", {false, 0.0}, {false, 0.0}, {0, 1, 2}) } );
	modules.insert( {58, TT_Module(58, 0x02, 4, false, "/100", "*100", {true, 20.0}, {true, 120.0}, {}) } );
	modules.insert( {59, TT_Module(59, 0x02, 4, false, "/100", "*100", {true, -50.0}, {true, 60.0}, {}) } );
	modules.insert( {60, TT_Module(60, 0x02, 4, false, "/100", "*100", {true, -40.0}, {true, 40.0}, {}) } );
	modules.insert( {61, TT_Module(61, 0x03, 4, false, "/100", "*100", {true, 0.0}, {true, 100.0}, {}) } );
	modules.insert( {62, TT_Module(62, 0x04, 2, false, "/100", "*100", {true, 0.0}, {true, 10.0}, {}) } );
	modules.insert( {63, TT_Module(63, 0x0B, 4, false, "", "", {false, 0.0}, {false, 0.0}, {}) } );
	devices.insert( {6, TT_Device(6, modules)} ); // insert device to map of devices
	modules.clear();

	return devices;
}

#endif /* DEVICE_TABLE_H */
