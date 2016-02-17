#!/usr/bin/python2
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
import SocketServer

devices_file = ".bluedevices"	# path to file with list of MAC bluetooth devices
openhab_path = "/opt/openhab"	# path to OpenHAB (where is ./start.sh)
openhab_port = 8080				# port http
mqtt_channel = "BeeeOn/openhab"	# name mosquitto channel
sleep_scan = 9					# X seconds for bluetooth scan
HOST, PORT = "localhost", 9871	# http handler and port
log = False						# continuous list-ing to stdout

#=======================================================================
thread_bool = True
know = None
#=======================================================================
class Knowledge(object):	# work with and memory MAC addresses
	def __init__(self):
		self.listMAC = []	# a list of MAC addresses
		self.item = []		# mark in OH config

	def addMAC(self, mac):
		if mac in self.listMAC:
			return
		self.listMAC.append(mac)
		self.item.append('')
		self.writeOH()		# edit in OH conf
		self.savetofile()	# save MAC

	def removeMAC(self, mac):
		if not mac in self.listMAC:
			return
		idx = self.listMAC.index(mac)
		del self.listMAC[idx]
		del self.item[idx]
		# not necessary edit in OH, just will be not used
		self.savetofile()	# remove MAC

	def addItem(self, mac, item):
		idx = self.listMAC.index(mac)
		self.item[idx] = item

	def getItem(self, mac):
		if mac in self.listMAC:
			idx = self.listMAC.index(mac)
			return self.item[idx]
		else: return ''

	def dbg(self):		# debug print
		for i in range (0, len(self.listMAC)):
			print self.listMAC[i], self.item[i]

	def isstored(self,mac):
		if mac in self.listMAC:
			return True
		else:
			return False

	def writeOH(self):	# write changes to config files OpenHab
		f_items = open(openhab_path+"/configurations/items/bluetooth.items", 'w')	# items
		f_rules = open(openhab_path+"/configurations/rules/bluetooth.rules", 'w')	# rules
		f_items.write('Group All\nString fromPy (All)\n')
		f_items.write('Switch toMqtt (All) {mqtt=">[broker:BeeeOn/openhab:command:on:${command}],>[broker:BeeeOn/openhab:command:*:${command}]"}\n')
		f_items.write('String fromMqtt (All) {mqtt="<[broker:BeeeOn/openhab/bt:state:default]"}\n')
		f_rules.write('rule "commands from Py to Mqtt"\nwhen\n\tItem fromPy received update\nthen\n\ttoMqtt.sendCommand(fromPy.state)\nend\n\n')
		f_rules.write('rule "commands from Mqtt to Py"\nwhen\n\tItem fromMqtt received update\nthen\n\tsendHttpPutRequest("http://'+HOST+':'+str(PORT)+'", "text/plain", fromMqtt.state.toString)\nend\n')
		for idx, mac in enumerate(know.listMAC):
			index = str(idx)
			f_items.write('\nSwitch BT'+index+' (All) {bluetooth="'+mac+'"}\n')
			know.addItem(mac, ('BT'+ index) )
			f_items.write('Switch MQTT'+index+' (All) {mqtt=">[broker:'+mqtt_channel+':command:on:euid;'+ mac +';device_id;5;module_id;0x00;value;1.00],>[broker:'+mqtt_channel+':command:off:euid;'+ mac +';device_id;5;module_id;0x00;value;2.00]"}\n')

			f_rules.write('\nrule "rule '+index+' for BT'+index+'"\nwhen\n')
			f_rules.write('\tItem BT'+index+' received update\nthen\n')
			f_rules.write('\tMQTT'+index+'.sendCommand(BT'+index+'.state)\nend\n')
		f_items.close()
		f_rules.close()

	def savetofile(self):
		head = '# List of saved MAC address devices for OpenHAB\n# Read only at start. Please do not modify!\n'

		with open(devices_file, 'w') as fi:	# touch file
			fi.write(head)
			for mac in know.listMAC:
				fi.write(mac + '\n')

#=======================================================================
class TCPHandler(SocketServer.BaseRequestHandler):
	def handle(self):
		global know
		self.data = self.request.recv(1024).strip()
		dat = str(self.data).split()
		mesg = dat[-1]
		if mesg == '': return
		numOfTry = 0
		output = ''
		self.request.sendall("200 OK\r\n")
		if mesg == 'test':
			if log: print('TCPHandler, just test')

		if mesg == 'iwannalistofbluetooth':
			if log: print('OH wants list of bluetooth - I do scanning...')
			while numOfTry < 8:		# number of experiments, when the scan fails (BT is busy)
				try:
					numOfTry += 1
					output = subprocess.check_output(["hcitool", "scan"]) # do full scan
				except:
					time.sleep(1)
					continue

			if output == '':
				#self.request.sendall("400 FAIL\r\n")
				return
			output = output.split('\n')

			for line in output:
				if line.find(':') > 0:
					lili = line.split()
					if log: print(lili)
					if len(lili) == 2 and not know.isstored(converse(lili[0])):
						strforsend = 'euid;'+ converse(lili[0]) +';device_id;5;module_id;0x00;value;0.00;name;' + lili[1]
						if log: print(strforsend)
						try: os.system('curl --max-time 2 --connect-timeout 2 --header "Content-Type: text/plain" --request PUT --data "'+strforsend+'" http://localhost:'+str(openhab_port)+'/rest/items/fromPy/state')
						except: print(str(sys.argv[0]) + ": Failed sending2 to OpenHAB")

		mesg = mesg.split(';')	# have to parse first !

		if mesg[0] == 'adddevicetolist':		# example: adddevicetolist;00ABCDEF0123
			if log: print('ADD DEVICE: ' + mesg[1])
			know.addMAC(mesg[1])		# mesg[1] # there is a mac to add
		if mesg[0] == 'removedevicefromlist':
			if log: print('REMOVE DEVICE: ' + mesg[1])
			know.removeMAC(mesg[1])

#=======================================================================
def failBT():	# print error and The Bad End
	print(str(sys.argv[0]) + ': BT scan failed! Maybe BT donge is not connected.')
	thread_bool = False
	sys.exit(1)

#=======================================================================
def converse(mac):	# addition or remove ":"
	MAC = ''
	if mac.find(':') > 0:	# found ':' so remove it
		for i in range(0, len(mac)):
			if mac[i] != ':':
				MAC += mac[i]
	else:	# not found ':' so add it
		for i in range(0, len(mac)):
			MAC += mac[i]
			if ((i+1)%2) == 0 and i < len(mac)-1:
				MAC += ':'
	return MAC

#=======================================================================
def load_file():
	global know
	if not os.path.isfile(devices_file):	# if file does not exist, nothing to do
		return

	f = open(devices_file, 'r')		# read list od MAC
	for line in f:
		line = line.strip()		# remove spaces (l+r side)
		if len(line) < 12:		# less than 12 isn't MAC (+ '\n')
			continue
		line = line.replace('\t', '')	# remove white spaces
		line = line.replace(' ', '')
		line = line.replace(':', '')
		line = line.replace('\n', '')
		if line.find('#') > -1:			# if is finds a comment
			line = line[0:line.find('#')]	# trimming
		if len(line) > 0:
			line = line.upper()
			line = line.strip()
			know.addMAC(line)
	f.close()
	if log:
		text = 'Loaded:'
		for mac in know.listMAC:
			text += ' | ' + mac
		print(text)

#=======================================================================
def http_handler():
	server = SocketServer.TCPServer((HOST, PORT), TCPHandler)
	server.serve_forever()

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
	global openhab_path, log, know

	if len(sys.argv) == 2:
		if sys.argv[1] == '--log' or sys.argv[1] == '-l':
			log = True

	out = 0
	try: out = subprocess.call(["hciconfig", "hci0", "up"])	# turn on bluetooth dongle
	except: failBT()		# fail of turn on
	if out != 0: failBT()	# bad return code

	know = Knowledge()	# object for parse and memory MAC address

	if openhab_path[-1] == '/': openhab_path = openhab_path[0:-1]	# delete slash

	load_file()		# load stored MAC

	try:
		#thread.start_new_thread( checking_file, () )	# check file with list of MAC
		thread.start_new_thread( http_handler, () )	# run http handler
		print(str(sys.argv[0]) + ": OK, run")
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
