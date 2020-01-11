#!/bin/bash 

sudo apt-get -y install wiringpi i2c-tools libi2c-dev

#manual requirements:
#   enable i2c via sudo raspi-config

#   add "sudo modprobe i2c-bcm2708" to etc/rc.local
echo "sudo modprobe i2c-bcm2708" >> /etc/rc.local


#add i2c-bcm2708 && i2c-dev to /etc/modules

#make cc for changing coefficients executable in any directory
sudo /home/pi/hifihat/socket/cp cc /usr/local/bin/cc



#create service
sudo cp hifihat.service /etc/systemd/system/hifihat.service
sudo systemctl enable hifihat