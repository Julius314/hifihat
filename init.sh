#!/bin/bash 

sudo apt-get -y install wiringpi i2c-tools libi2c-dev

#   enable i2c via sudo raspi-config
sudo sh -c "echo 'dtparam=i2c_arm=on' >> /boot/config.txt"
sudo sh -c "echo 'dtparam=i2c1=on' >> /boot/config.txt"

#   add "sudo modprobe i2c-bcm2708" to etc/rc.local
##needs to be appended as second last line!
#sudo sh -c "echo 'sudo modprobe i2c-bcm2708' >> /etc/rc.local"


#add i2c-bcm2708 && i2c-dev to /etc/modules
sudo sh -c "echo 'i2c-bcm2708' >> /etc/modules"
sudo sh -c "echo 'i2c-dev' >> /etc/modules"

#make cc for changing coefficients executable in any directory
sudo cp hifihat/socket/cc /usr/local/bin/cc

#create service
sudo cp hifihat/hifihat.service /etc/systemd/system/hifihat.service
sudo systemctl enable hifihat

sudo reboot now