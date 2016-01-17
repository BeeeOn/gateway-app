#!/bin/bash
# works on RaspberryPi (Raspian)
# Date: 2015-09-24

if [ "$1" = "install" ]; then
	#echo "Install utilits..."
	#apt-get install hostapd udhcpd iw -y
	
	echo "Editing files for wifi AP."
	
	# ------------------------------------------------------------------
	if [ -f /etc/default/hostapd.old ]; then	# if FILE exists and is a regular file.
		cp /etc/default/hostapd.old /etc/default/hostapd	# recovery
	else
		cp /etc/default/hostapd /etc/default/hostapd.old	# backup
	fi
	echo 'DAEMON_CONF="/etc/hostapd/hostapd.conf"' >> /etc/default/hostapd
	
	
	if [ -f /etc/hostapd/hostapd.conf.old ]; then
		cp /etc/hostapd/hostapd.conf.old /etc/hostapd/hostapd.conf	# recovery
	else
		cp /etc/hostapd/hostapd.conf /etc/hostapd/hostapd.conf.old	# backup
	fi
	echo -n '
interface=wlan0_ap
ssid=BeeeOnTest
hw_mode=g
channel=1
auth_algs=1
wmm_enabled=0
' > /etc/hostapd/hostapd.conf				# rewrite file

	# ------------------------------------------------------------------
	if [ -f /etc/network/interfaces.old ]; then		# if FILE exists
		cp /etc/network/interfaces.old /etc/network/interfaces	# recovery
	else
		cp /etc/network/interfaces /etc/network/interfaces.old	# backup
	fi
	echo -n '
iface wlan0_ap inet static
hostapd /etc/hostapd/hostapd.conf
address 192.168.50.1
netmask 255.255.255.0
' >> /etc/network/interfaces

	# ------------------------------------------------------------------
	if [ -f /etc/default/udhcpd.old ]; then
		cp /etc/default/udhcpd.old /etc/default/udhcpd	# recovery
	else 
		cp /etc/default/udhcpd /etc/default/udhcpd.old	# backup
	fi
	echo 'DHCPD_OPTS="-S"' > /etc/default/udhcpd	# rewrite file
	
	
	if [ -f /etc/udhcpd.conf.old ]; then
		cp /etc/udhcpd.conf.old /etc/udhcpd.conf	# recovery
	else
		cp /etc/udhcpd.conf /etc/udhcpd.conf.old	# backup
	fi
	echo -n '
start 192.168.50.100
end 192.168.50.150
interface wlan0_ap
remaining yes
opt subnet 255.255.255.0
opt router 192.168.50.1
opt lease 7800
' > /etc/udhcpd.conf

	# ------------------------------------------------------------------

else
	echo "Use: install"

fi
