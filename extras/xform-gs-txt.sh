#!/bin/bash
#
# Shell script to transform 'VDL2_Ground_Stations.txt' file to a compatible format
# but with more informative GS data, for dumpvdl2 ( https://github.com/szpajder/dumpvdl2 )
# This file is maintained (a really hard job) by Sergio, Thanks.
# File format is mostly applicable to MultiPSK program.
#
# Requires 'ap4dumpvdl2.sqb' SQLite3 database, built by running 'ap4dumpvdl2.sh' script in 'extras' directory.
# Requires sqlite3 command line interface, install 'sqlite3' cli ( $ sudo apt install sqlite3 )
# Optional, recover some GSs that could not be auto-transformed correctly, "more-gss.txt" file
#
# Created by Dimitris Georgiou <olyair11@gmail.com>
#
apx() {
local IFS='|'
read -a strarr <<< "$3"
name="${strarr[0]}"
city="${strarr[1]}"
country="${strarr[2]}"
printf '%s [%s %s, %s] [%s]\r\n' "$1" "$2" "$name" "$country" "$city" >> VDL2_Ground_Stations_new.txt
}
more_gss=0
min_gs_txt_size=100000
if [ ! -f ap4dumpvdl2.sqb ]; then
    echo "ap4dumpvdl2.sqb database file not found. Build it by running 'ap4dumpvdl2.sh' script."
    exit
fi
if [ ! -f more-gss.txt ]; then
    echo "Optional 'more-gss.txt' file not found."
    read -p "Hit Enter key to continue, Ctrl+c to stop ..."
else
    ((more_gss++))
fi
if ! command sqlite3 -version >/dev/null; then
    echo "sqlite3 command not found. Install 'sqlite3' cli."
    exit
fi
if [ ! -f VDL2_Ground_Stations.txt ]; then
    echo "VDL2_Ground_Stations.txt file Not found."
    exit
fi
gs_txt_size=$(stat -c %s VDL2_Ground_Stations.txt)
if [ $gs_txt_size -lt $min_gs_txt_size ]; then
	echo "VDL2_Ground_Stations.txt file size is smaller than $min_gs_txt_zize, corrupted file ?"
    exit
fi
echo "Transforming VDL2_Ground_Stations.txt to a more informative format ..."
cp VDL2_Ground_Stations.txt VDL2_Ground_Stations.orig
grep -oP "[A-Z0-9]{6} \[[A-Z0-9]{4}" VDL2_Ground_Stations.txt | sed 's/\[//' > tmp1.txt
if [ $more_gss -ne 0 ]; then
    cat tmp1.txt more-gss.txt | sort -k 1 > good1s.txt
else
    cat tmp1.txt | sort -k 1 > good1s.txt
fi
grep -vP "[A-Z0-9]{6} \[[A-Z0-9]{4}" VDL2_Ground_Stations.txt > bad1s.txt
rm -f VDL2_Ground_Stations_new.txt
IFS=' '
while IFS= read -r line; do
  read -r gsid apid <<< "$line"
  apinfo=$(sqlite3 ap4dumpvdl2.sqb "select name, city, country from airports where icao='$apid' or iata='$apid';")
  apx "$gsid" "$apid" "$apinfo"
done < good1s.txt
grep '\[, \]' VDL2_Ground_Stations_new.txt | sort -u -k 2 > tmp2.txt
cat VDL2_Ground_Stations_new.txt | sed 's| \/ |\/|g' | sed 's/\"/`/g' > VDL2_Ground_Stations.txt
rm -f good1s.txt bad1s.txt tmp1.txt tmp2.txt VDL2_Ground_Stations_new.txt
all_gss=$(wc -l VDL2_Ground_Stations.txt | awk '{ print $1 }')
echo "Successful transformation. $all_gss records."
