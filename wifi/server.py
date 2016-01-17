#!/usr/bin/python
# works on RaspberryPi (Raspian)
# Date: 2015-10-13

import SimpleHTTPServer
import SocketServer
import cgi
import sys, os
from threading import Thread
import time
import signal
import subprocess

HOST = ''
PORT = 0 
ALIVE = True
scanning = None
httpd = None
IFC_NEW = '/etc/network/interfaces'
IFC_OLD = '/etc/network/interfaces.temp'
PINGHOST = 'www.beeeon.com'
WLANS = 'wlans.html'
Status = -1

#=======================================================================
class PingPong(Thread):
	def __init__(self):
		Thread.__init__(self)
	def run(self):
		global ALIVE, Status
		while ALIVE:
			try:
				Status = subprocess.call(["ping", "-c", "3", PINGHOST])
				if Status == 0: # Its OK
					exit_gracefully(None, None)
			except: pass
			time.sleep(3)

#=======================================================================
class ScanWLAN(Thread):
	def __init__(self):
		Thread.__init__(self)
		self.arraynames = []
		self.ssid = self.passw = ''
	def run(self):
		global ALIVE
		while ALIVE:
			try:
				resultscan = subprocess.check_output(["iw","wlan0_sta","scan"])
			except:
				print('thread except')
				time.sleep(5)
				continue
			for line in resultscan.splitlines():
				line = line.strip()
				if line[0:5] == 'SSID:':
					self.stackNames(line[5:].strip())
			self.write()
			time.sleep(5)
			
	def stackNames(self,title):
		if title in self.arraynames: pass
		else: self.arraynames.append(title)

	def write(self):
		print self.arraynames
		if self.arraynames == []: return
		with open(WLANS, 'w') as f:
			f.write('<html><head>\n\t<link rel="stylesheet" type="text/css" href="img/style.css" />\n</head><body>\n')
			for item in self.arraynames:
				f.write('<div onclick=\'parent.fill("' + item + '");\'><p>' + item + '</p></div>\n')
			f.write('</body></html>\n')

#=======================================================================
class ServerHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):

	def do_POST(self):
		#print("======= POST STARTED =======")
		#print(self.headers)
		form = cgi.FieldStorage(
			fp=self.rfile,
			headers=self.headers,
			environ={'REQUEST_METHOD':'POST',
					 'CONTENT_TYPE':self.headers['Content-Type'],
					 })
		print("======= POST VALUES =======")
		#for item in form.list: print(item)
		try:
			print("SSID: " + str(form["first"].value))
			self.ssid = str(form["first"].value)
		except: pass
		try:
			print("PASS: " + str(form["second"].value))
			self.passw = str(form["second"].value)
		except: pass
		print("===========================")
		
		SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)
		
		if self.ssid != '':
			shunt(self.ssid, self.passw)
			pass

#=======================================================================
def exit_gracefully(signum, frame):
	global scanning, ALIVE, httpd
	print(' Exit_gracefully')
	signal.signal(signal.SIGINT, original_sigint)
	ALIVE = False
	try: os.remove(WLANS)
	except: pass
	
	print('exit() - del virtual')
	subprocess.call(["iw","dev","wlan0_sta","del"])	# delete virtual
	subprocess.call(["iw","dev","wlan0_ap","del"])	# delete virtual
	
	try: scanning.join()
	except: pass
	#httpd.shutdown()
	try: httpd.server_close()
	except: pass
	exit(0)

#=======================================================================
def bashprocess():
	print('bashprocess()')
	#Status = subprocess.call("ls", shell=True)	# result
	#Status = subprocess.check_output("ls")		# text
	'''
	iw dev wlan0 interface add wlan0_sta type station
	iw dev wlan0 interface add wlan0_ap  type __ap
	service hostapd start
	ifconfig wlan0_ap 192.168.50.1
	service udhcpd start
	'''
	print('bashprocess() - create virtual')
	try:
		subprocess.call(["iw","dev","wlan0","interface","add","wlan0_sta","type","station"])
		subprocess.call(["iw","dev","wlan0","interface","add","wlan0_ap","type","__ap"])
	except: pass
	
	subprocess.call(["service","hostapd","restart"])		# hostapd
	subprocess.call(["ifconfig","wlan0_ap","192.168.50.1"])		# set ip
	subprocess.call(["service","udhcpd","restart"]) 	# dhcp server

#=======================================================================
def shunt(ssid, psk):		# connecting to new LAN
	global Status
	
	print('shunt(ssid, psk)')
	subprocess.call(["mv", IFC_NEW, IFC_OLD])
	
	f_new = open(IFC_NEW, 'w')
	f_old = open(IFC_OLD, 'r')
	for line in f_old:
		if line == 'iface wlan0_ap inet static\n': pass
		elif line == 'hostapd /etc/hostapd/hostapd.conf\n': pass
		elif line == 'address 192.168.50.1\n': pass
		elif line == 'netmask 255.255.255.0\n': pass
		else:
			f_new.write(line)
	#for
	f_new.write('auto wlan0\n')
	f_new.write('iface wlan0 inet dhcp\n')
	f_new.write('\t wpa-ssid "' + ssid + '"\n')
	f_new.write('\t wpa-psk "' + psk + '"\n')

	f_new.close()
	f_old.close()
	
	print('shunt() - subprocess')
	subprocess.call(["service","hostapd","stop"])	# shutdown own AP
	
	subprocess.call(["service","udhcpd","stop"]) 	# dhcp server	# dhcp stop
	print('shunt() - del')
	subprocess.call(["iw","dev","wlan0_sta","del"])	# remove virtual
	subprocess.call(["iw","dev","wlan0_ap","del"])	# remove virtual
	# iw dev wlan0_sta del
	# iw dev wlan0_ap del
	print('shunt() - down/up')
	subprocess.call(["ip","link","set", "wlan0", "down"])	# enable
	subprocess.call(["ip","link","set", "wlan0", "up"])	# enable
	print('shunt() - dhclient')
	subprocess.call(["service","networking","restart"])	# networking
	subprocess.call(["sleep","2"])	# delay
	subprocess.call(["dhclient","wlan0"])	# get IP
	print('shunt() - dhclient done')
	time.sleep(3)	# delay
	print('Status of PING = ' + str(Status))
	if Status == 0: # Its OK
		exit_gracefully(None, None)
	else:
		returnback()
	
#=======================================================================
def returnback():
	print('returnback()')
	subprocess.call(["mv", IFC_OLD, IFC_NEW])
	
	bashprocess()

#=======================================================================
#=======================================================================
def main():
	print('main()')
	global scanning, httpd
	
	bashprocess()

	Handler = ServerHandler
	try: httpd = SocketServer.TCPServer((HOST, PORT), Handler)
	except:
		print('Server cannont start on port: ' + str(PORT)) 
		exit_gracefully(None, None)
	scanning = ScanWLAN()
	scanning.start()
	
	subprocess.call(["cp", WLANS + '.clean', WLANS])	# If there is no network

	print("Server started.")
	httpd.serve_forever()

#=======================================================================
if __name__ == "__main__":
	original_sigint = signal.getsignal(signal.SIGINT)
	signal.signal(signal.SIGINT, exit_gracefully)

	try: PORT = int(sys.argv[1])
	except: 
		print('Missing port! (python server.py PORT)')
		exit(1)
	
	Status = subprocess.call(["ping", "-c", "2", PINGHOST])
	time.sleep(1)		# delay
	if Status == 0: # Its OK
		print('Adapter is connected yet.')
		exit(0)
	else:
		print('Without connection...')
		main()
	
	#main()
#EOF
