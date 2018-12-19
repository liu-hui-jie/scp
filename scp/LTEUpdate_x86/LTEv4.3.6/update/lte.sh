#! /bin/sh
auto=1
nuc=1
date -s "2015-07-15"
cd /root/LTELocation/driver/
./load.sh
cd /root
rmmod rtl8192cu
insmod 8192cu.ko
sleep 3
if [ $nuc -eq 1 ];then
    ethdevice=enp0s25
else
    ethdevice=enp3s0
fi
for((i=0; i<8; ++i));do
    cpupower -c $i frequency-set -g performance
done
UseSpaceBound=4500000000
while [ $(du --block-size=1 /root/LTELocation | awk '{print $1}' | tail -n 1) -gt $UseSpaceBound ];do
    fileName=$(du /root/LTELocation/run*.log | sort -rn | head -n 1 | awk '{print $2}')
    rm -f $fileName
done
cd /root/LTELocation/
while read LINE;do
    newIndex="$LINE"
done < logIndex
newIndex=`expr $newIndex + 1`
if [ $newIndex = "31" ];then
    newIndex="1"
fi
if [ $auto -eq 1 ];then
    echo $newIndex > logIndex
    ./FFTWCheck &
    ./LTE_Location_r &> run$newIndex.log &
fi
wlandevice=$(iwconfig 2>/dev/null | head -1 | cut -d ' ' -f 1)
ifconfig $ethdevice up
ifconfig $ethdevice 192.168.100.71
route=20181218
wpa=0
if [ ${route:0:4} -gt 0062 ];then
    wpa=1
fi
if [ -z $wlandevice ];then
    wlandevice=wlan0
else
    ifconfig $wlandevice up
    iwconfig $wlandevice power off
    ifconfig $wlandevice 192.168.43.115
    sleep 3
    if [ $wpa -eq 1 ];then
        wpa_passphrase $route ltedw123 > wpa_inv.conf
        wpa_supplicant -B -i $wlandevice -Dwext -cwpa_inv.conf
    else
        iwconfig $wlandevice essid $route
    fi
fi
cd /root/LTELocation/
./LTEAPConnect $wlandevice $route $wpa > apconnect.log &
