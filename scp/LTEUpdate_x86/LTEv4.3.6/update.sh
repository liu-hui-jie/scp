#!/bin/sh
#rm -rf config/Local*
#rm -rf config/*.dat
#date -s "2017-05-09"
#tar -xf update/LocalData.tar.gz -C config/
#date -s "2015-07-15"
#cp update/xilinx_driver.ko driver/
#cp update/lte.sh /root/
#chmod +x /root/lte.sh
#cp update/system.conf config/
kill $(pidof LTE_Location_r)
kill $(pidof LTEAPConnect )
rm -rf LTE_Location_r
rm -rf LTEAPConnect
cp -r update/* .
chmod +x LTE_Location_r
chmod +x LTEAPConnect
chmod +x FFTWCheck
chmod +x lte.sh
mv lte.sh ../lte.sh
rm -rf update
rm -rf update.sh
sync
