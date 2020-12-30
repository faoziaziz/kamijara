#!/bin/bash

#script untuk restart aplikasi

date

var=$(ps axu | grep -v grep | grep "parser 34.87.193.140 trumon 3306 faoziaziz Azizkeren1234 3")
        if [ ! -z "$var" ];
                then echo "found";
        else
          while true
          do
		/home/almar/Trumon/Parser/parser 34.87.193.140 trumon 3306 faoziaziz Azizkeren1234 3|more
          done
fi

