#!/usr/bin/python
# Author BeeeOn team
# Date: 2016-02-06
# Description: Script scans for available Bluetooth devices
#		and from the desired sending their status to http daemon on OpenHAB.
# Run:	bluetooth.py &

import thread
import time
from time import gmtime, strftime
import os,sys
import subprocess
from subprocess import Popen, PIPE
import re

devices_file = "bluedevices"	# path to file with list of MAC bluetooth devices
openhab_path = "/opt/openhab"	# path to OpenHAB (where is ./start.sh)
openhab_port = 8080				# port http
mqtt_canal = "BeeeOn/openhab"	# name mosquitto chanel
sleep_file = 2					# X seconds for check changes "devices_file"
sleep_scan = 9					# X seconds for bluetooth scan
log = False						# continuous list-ing to stdout

#=======================================================================
thread_bool = True
know = None
#=======================================================================
class Knowledge(object):	# work with and memory MAC addresses
	def __init__(self):
		self.iii = 0
		self.listMAC = []
		self.name = []
		self.item = []
		self.change = False

	def addMAC(self, mac):
		if mac in self.listMAC:
			return
		self.iii = self.iii + 1
		self.listMAC.append(mac)
		self.name.append('')
		self.item.append('')
		self.change = True

	def removeMAC(self, mac):
		idx = self.listMAC.index(mac)
		del self.listMAC[idx]
		del self.name[idx]
		del self.item[idx]
		self.iii = self.iii - 1
		self.change = True

	def addItem(self, mac, item):
		idx = self.listMAC.index(mac)
		self.item[idx] = item

	def getItem(self, mac):
		if mac in self.listMAC:
			idx = self.listMAC.index(mac)
			return self.item[idx]
		else: return ''

	def addName(self, mac, name):
		if mac in self.listMAC:
			idx = self.listMAC.index(mac)
			self.name[idx] = name

	def getName(self, mac):
		if mac in self.listMAC:
			idx = self.listMAC.index(mac)
			return self.name[idx]
		else: return ''

	def checkChange(self, newlist):
		different = False
		if len(newlist) != len(self.listMAC):
			different = True
		elif len(set(newlist) & set(self.listMAC)) != len(newlist):
			different = True

		if different:
			for mac in self.listMAC:
				if not mac in newlist:
					self.removeMAC(mac)
			for mac in newlist:
				if not mac in self.listMAC:
					self.addMAC(mac)

	def ischange(self):
		pom = self.change
		self.change = False
		return pom

	def dbg(self):		# debug print
		for i in range (0, self.iii):
			print self.listMAC[i], self.name[i], self.item[i]

#=======================================================================
def failBT():	# print error and The Bad End
	print(str(sys.argv[0]) + ': BT scan failed! Maybe BT dongle is not connected.')
	thread_bool = False
	sys.exit(1)

#=======================================================================
def converse(mac):	# addition ":"
	MAC = ''
	for i in range(0, len(mac)):
		MAC += mac[i]
		if ((i+1)%2) == 0 and i < len(mac)-1:
			MAC += ':'
	return MAC

#=======================================================================
def check_file():	# control file with MAC
	global openhab_path, know
	timestamp = 0
	templist = []

	if not os.path.isfile(devices_file):	# if file does not exist, create
		with open(devices_file, 'w') as fi:	# touch file
			fi.write('# List of MAC address devices for OpenHAB\n# Example:\n# 00:FF:AB:CD:EF:GH\n# 00FFABCDEFGH\n')
			fi.close()

	while thread_bool:	# over and over
		if timestamp == os.stat(devices_file).st_mtime:	# file modification time
			time.sleep(sleep_file)	# sleep
			continue

		f = open(devices_file, 'r')		# read list od MAC
		templist = []		# temporary list
		for line in f:
			line = line.strip()
			if len(line) < 13:	# less than 12 isn't MAC (+ '\n')
				continue
			line = line.replace('\t', '')
			line = line.replace(' ', '')
			line = line.replace(':', '')
			line = line.replace('\n', '')
			if line.find('#') > -1:
				line = line[0:line.find('#')]	# trimming
			if len(line) > 0:
				line = line.upper()
				line = line.strip()
				templist.append(line)
		f.close()
		timestamp = os.stat(devices_file).st_mtime	# set time last change

		know.checkChange(templist)	# monitoring changes in the list

		if log: print "listMAC: ",templist

		if not know.ischange():	continue	# to begin if lists are same

		# write to config files OpenHab
		if openhab_path[-1] == '/': openhab_path = openhab_path[0:-1]	# delete slash
		f_items = open(openhab_path+"/configurations/items/adaapp.items", 'w')
		f_rules = open(openhab_path+"/configurations/rules/adaapp.rules", 'w')
		f_items.write('Group All\n')
		for idx, mac in enumerate(know.listMAC):
			f_items.write('\nSwitch BT'+ str(idx) +' (All) {bluetooth="'+ mac +'"}\n')
			know.addItem(mac, ('BT'+ str(idx)) )
			f_items.write('Switch MQTT'+ str(idx) +' (All) {mqtt=">[broker:'+mqtt_canal+':command:on:euid;'+ mac +';device_id;5;module_id;0x00;value;1.00],>[broker:'+mqtt_canal+':command:off:euid;'+ mac +';device_id;5;module_id;0x00;value;2.00]"}\n')

			for boo in ['ON','OFF']:
				f_rules.write('\nrule "rule '+ str(idx) +' for mqtt '+ boo +'"\nwhen\n')
				f_rules.write('\tItem BT'+ str(idx) +' received update '+ boo +'\nthen\n')
				f_rules.write('\tMQTT'+ str(idx) +'.sendCommand('+ boo +')\nend\n')
		f_items.close()
		f_rules.close()
	#while

#=======================================================================
def scanBT():		# scanning of individual devices
	global know
	test = 0
	while thread_bool:
		time.sleep(sleep_scan)
		for mac in know.listMAC:
			MAC = converse(mac)
			(out, err) = Popen(["hcitool", "name", MAC], stdout=PIPE).communicate()
			if len(out) <= 0:
				if log: print (MAC+" NO name")
				state = 'OFF'
			elif len(out) > 0:
				if log: print (MAC+" HAS name: " + out)
				state = 'ON'
			try: os.system('curl --max-time 2 --connect-timeout 2 --header "Content-Type: text/plain" --request PUT --data "'+state+'" http://localhost:'+str(openhab_port)+'/rest/items/'+know.getItem(mac)+'/state')
			except: print(str(sys.argv[0]) + ": Failed sending to OpenHAB")
			time.sleep(1)
	# while

#=======================================================================
def main():
	global log, know
	out = 0
	try: out = subprocess.call(["hciconfig", "hci0", "up"])	# turn on bluetooth dongle
	except: failBT()		# fail of turn on
	if out != 0: failBT()	# bad return code

	know = Knowledge()	# object for parse and memory MAC address

	try:
		thread.start_new_thread( check_file, () )	# check file with list of MAC
		print(str(sys.argv[0]) + ": OK, run.")
	except:
		sys.stderr.write(str(sys.argv[0]) + ": Thread cannot start\n")
		sys.exit(0)

	if len(sys.argv) == 2:
		if sys.argv[1] == '--log' or sys.argv[1] == '-l':
			log = True

	scanBT()

#=======================================================================
if __name__ == '__main__':
	try:
		main()
	except KeyboardInterrupt:
		thread_bool = False
		print('\n' + str(sys.argv[0]) + ': Shutdown')
		sys.exit(0)
