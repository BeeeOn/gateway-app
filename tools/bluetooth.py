#!/usr/bin/python
# Date: 2015-11-19
# Description: Script scans for available Bluetooth devices
#		and from the desired sending their status to http daemon on OpenHAB.
# Run:	bluetooth.py &

import thread
import time
from time import gmtime, strftime
import os,sys
import subprocess
import re

devices_file = "BTdevices"		# path to file with list of MAC bluetooth devices
openhab_path = "/opt/openhab"	# path to OpenHAB (where is ./start.sh)
openhab_port = 8080				# port http
mqtt_canal = "BeeeOn/openhab"	# name mosquitto chanel
bt_list_file = "/tmp/.openhab.list"	# list available BT
sleep_file = 2					# X seconds for check changes "devices_file"
sleep_scan = 10					# X seconds for bluetooth scan
log = False						# continuous listing to stdout

#=======================================================================
thread_bool = True
know = None
#=======================================================================
class Knowledge(object):
	def __init__(self):
		self.iii = 0
		self.listMAC = []
		self.name = []
		self.item = []
		self.change = False
		self.all = []

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

	def allDevices(self,stringe):
		if stringe in self.all:
			return
		self.all.append(stringe)

	def writeAllDevices(self):
		with open(bt_list_file, 'w') as f:
			for st in self.all:
				f.write(st + '\n')
			f.close()

	def dbg(self):		# debug print
		for i in range (0, self.iii):
			print self.listMAC[i], self.name[i], self.item[i]

#=======================================================================
def failBT():	# vypis chyby a konec
	print(str(sys.argv[0]) + ': Fail BT scan! Maybe BT donge is not connected.')
	thread_bool = False
	sys.exit(1)

#=======================================================================
def check_file():	# kontrola souboru s MAC
	global openhab_path, know
	timestamp = 0
	templist = []

	if not os.path.isfile(devices_file):
		with open(devices_file, 'w') as fi:	# touch file
			fi.write('# List of MAC address devices for OpenHAB\n# Example:\n# 00:FF:AB:CD:EF:GH\n# 00FFABCDEFGH\n')
			fi.close()

	while thread_bool:
		if timestamp == os.stat(devices_file).st_mtime:
			time.sleep(sleep_file)	# waits for file change
			continue

		f = open(devices_file, 'r')		# read list od MAC
		templist = []
		for line in f:
			line = line.strip()
			if len(line) < 13:	# less than 12 isn't MAC (+ '\n')
				continue
			line = line.replace('\t', '')
			line = line.replace(' ', '')
			line = line.replace(':', '')
			line = line.replace('\n', '')
			if line.find('#') > -1:
				line = line[0:line.find('#')]
			if len(line) > 0:
				line = line.upper()
				line = line.strip()
				templist.append(line)
		f.close()
		timestamp = os.stat(devices_file).st_mtime	# set time last change

		know.checkChange(templist)	# check list change

		if log: print "listMAC: ",templist #=#=#=#=#

		if not know.ischange():	continue	# only if there is a change

		# write to config files
		if openhab_path[-1] == '/': openhab_path = openhab_path[0:-1]
		f_items = open(openhab_path+"/configurations/items/adaapp.items", 'w')
		f_rules = open(openhab_path+"/configurations/rules/adaapp.rules", 'w')
		f_items.write('Group All\n')
		for idx, mac in enumerate(know.listMAC):
			#print idx, mac
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
def scanBT():
	global know
	test = 0
	while thread_bool:
		time.sleep(sleep_scan)
		try:
			#output = subprocess.check_output(["hcitool", "scan"], stderr='/dev/null')
			output = subprocess.check_output(["hcitool", "scan"])
			test = 0
		except:
			if test > 0:
				failBT()
			else:
				test = test + 1
				continue
		output = output.split('\n')

		BTnet = []
		for line in output:
			if line.find(':') > 0:
				line = " ".join(line.split())	# remove whitespaces
				li = line.split()
				if li[0] == '' or li[1] == '': continue
				li[0] = li[0].upper()	# make upper case
				li[0] = li[0].replace(':', '')	# remove ':'
				BTnet.append(li[0])
				know.addName(li[0],li[1])	# name
				if log: print ("BT "+strftime("%H:%M:%S",gmtime())+": "), BTnet
				know.allDevices(li[0] + ';' + li[1])
		know.writeAllDevices()
		#print BTnet
		state = 'OFF'
		for mac in know.listMAC:
			if mac in BTnet:
				state = 'ON'
			else:
				state = 'OFF'
			#print 'sending'
			os.system('curl --max-time 2 --connect-timeout 2 --header "Content-Type: text/plain" --request PUT --data "'+state+'" http://localhost:'+str(openhab_port)+'/rest/items/'+know.getItem(mac)+'/state 2>/dev/null')
			#else: print(str(sys.argv[0]) + ": Failed sending to OpenHAB")
	# while

#=======================================================================
def main():
	global know
	out = 0
	try: out = subprocess.call(["hciconfig", "hci0", "up"])	# enable of bluetooth dongle
	except: failBT()
	if out != 0: failBT()

	know = Knowledge()

	try:
		thread.start_new_thread( check_file, () )
		print(str(sys.argv[0]) + ": OK, run.")
	except:
		sys.stderr.write(str(sys.argv[0]) + ": Thread cannot start\n")
		sys.exit(0)

	scanBT()

#=======================================================================
if __name__ == '__main__':
	try:
		main()
	except KeyboardInterrupt:
		thread_bool = False
		print('\n' + str(sys.argv[0]) + ': Shutdown')
		sys.exit(0)
