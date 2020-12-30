#!/bin/bash

#script untuk restart aplikasi

date

var=$(ps axu | grep -v grep | grep "TcpServerTS1 3321")
        if [ ! -z "$var" ];
                then echo "found";
        else
                /home/almar/Trumon/Receiver/TcpServerTS1 3321 128 127.0.0.1 trumon 3306 pos_almar @Alm4r2020
fi


