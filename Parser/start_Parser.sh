#!/bin/bash

#script untuk restart aplikasi

date

var=$(ps axu | grep -v grep | grep "parser 127.0.0.1 trumon 3306 pos_almar @Alm4r2020 3")
        if [ ! -z "$var" ];
                then echo "found";
        else
          while true
          do
		/home/almar/Trumon/Parser/parser 127.0.0.1 trumon 3306 pos_almar @Alm4r2020 3|more
          done
fi

