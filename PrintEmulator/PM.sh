#!/bin/bash

#script untuk restart aplikasi

date

var=$(ps axu | grep -v grep | grep "PrintEmulator 127.0.0.1")
        if [ ! -z "$var" ];
                then echo "found";
        else
          while true
          do
		/home/almar/Trumon/PrintEmulator/PrintEmulator 127.0.0.1 trumon 3306 pos_almar '@Alm4r2020'|more
          done
fi

