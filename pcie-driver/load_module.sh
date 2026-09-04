#!/bin/sh
module="mymodule"
device="FPGA"
mode="777"
group=0



#sudo find /var/log -type f -name "*.log" -delete

#sudo rm -rf ~/.cache/*
function load(){
    insmod ./$module.ko $* || exit 1

    rm -f /dev/${device}

    major=$(awk -v device="$device" '$2==device {print $1}' /proc/devices)
    mknod /dev/${device} c $major 0

    chgrp $group /dev/$device
    chmod $mode /dev/$device
}

function unload() {
    echo "WARNING: rmmod $module after RX/TX can Oops and D3cold the FPGA." >&2
    echo "Prefer a reboot. Continuing unload anyway." >&2
    rm -f /dev/${device}
    rmmod $module || exit 1
}

arg=${1:-"load"}
case $arg in
    load)
        load ;;
    unload)
        unload ;;
    reload)
        ( unload )
        load
        ;;
    *)
        echo "Usage: $0 {load | unload | reload}"
        echo "Default is load"
        exit 1
        ;;
esac
