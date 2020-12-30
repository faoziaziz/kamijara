#!/bin/bash

#script untuk restart aplikasi

date

var=$(ps axu | grep -v grep | grep "PrintEmulator 34.87.193.140")
        if [ ! -z "$var" ];
                then echo "found";
        else
          while true
          do
		/home/almar/Trumon/PrintEmulator/PrintEmulator 34.87.193.140 trumon 3306 faoziaziz Azizkeren1234 |more
          done
fi

