#!/bin/bash
#
# Capture frames from ESPCAM
#
#

setup() {
    
    export myname=$(basename ${0})
    export camidx=${1:-"00"}
    export interval=${2:-15}s
    export dest=${3:-"/mnt/USB64/capture"}
    export CAM=ESPCAM${camidx}

    [ -f ${dest}/.personal_settings ] && . ${dest}/.personal_settings 
}

usage() {
cat<<EOF

    usage: ${0} [camidx] [interval]  [destdir] 
    
    Defaults:
        camidx   = "00"
        interval =  15
        destdir  =  ${dest}
          
    ${myname}     will record from ESPCAM00 (Default for camidx is 00)
    ${myname} 01  will record from ESPCAM01
    ${myname} 02 10 on /tmp  will record from ESPCAM02 one picture every 10 seconds to /tmp 
EOF
exit 0
}

 
setcampar() {
    curl "http://${CAM}/control?var=framesize&val=10"
    curl "http://${CAM}/control?var=quality&val=10"
    curl "http://${CAM}/control?var=awb&val=1"
    curl "http://${CAM}/control?var=wb_mode&val=0"
    
    #
    # Auto exposure Sensor + Auto exposure DSP on 
    #
    curl "http://${CAM}/control?var=aec&val=1"
    curl "http://${CAM}/control?var=aec2&val=1"
    curl "http://${CAM}/control?var=ae_level&val=0"
    
    #
    # Gain-Ceilling  Level 2  
    #
    #  from 0=2 to 6=128
    #
    curl "http://${CAM}/control?var=agc&val=1"
    curl "http://${CAM}/control?var=gainceiling&val=2"
    
    #
    # white- and blackpoint correction ON
    #
    curl "http://${CAM}/control?var=bpc&val=1"
    curl "http://${CAM}/control?var=wpc&val=1"

    curl "http://${CAM}/control?var=raw_gma&val=1"
    curl "http://${CAM}/control?var=lenc&val=12"
    curl "http://${CAM}/control?var=dcw&val=0"
    
    # curl "http://${CAM}/control?var=hmirror&val=0
    # curl "http://${CAM}/control?var=vflip&val=0"
}

#
#  Main
#
[ ${1}"x" = "--usagex" ] && usage

setup

#  No time restriction (capture always)
#
# export always=on

# 
# Capture only from 06:00 to 20:30
#
export from=060000
export to=203000
#
# capture frames.

while true
do
    #
    # Reinitalize the parameter evrey 12 captures
    #  (in case s.o. changed them via Webinterface)
    #
    setcampar
    for (( i=1; i<=12; i++ ))
    do  
        echo -e "${myname}: waiting ${interval} before next capture.\n"
        sleep ${interval}
    
        datim=$(date +%H%M%S_+%Y%m%d)
        hms={datim/%_*/}
        ymd={datim/#_*/}

        [[ $always != "" || (${hms} -gt ${from}  &&  ${hms} -lt ${to}) ]] && {
            [ -d ${dest}/${ymd} ] || mkdir -p ${dest}/${ymd}
            curl "http://${CAM}/capture" --output ${dest}/${ymd}/${CAM}-${datim}.jpg
            echo -e "\n${myname}: ${dest}/${ymd}/${CAM}-${datim}.jpg has been captured."
        }
    done 
done

# That's all folks