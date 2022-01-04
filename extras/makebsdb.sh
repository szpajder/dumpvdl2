#!/bin/bash
#
# Shell script that generates the SQLite3 BaseStation database for
# "dumpvdl2" ( https://github.com/szpajder/dumpvdl2 ),
# "dumphfdl" ( https://github.com/szpajder/dumphfdl ),
# "JAERO" ( https://github.com/jontio/JAERO )
#
# Data provided courtesy of Opensky Network aircraft database file ( https://opensky-network.org )
#
# Requires sqlite3 command line interface, install 'sqlite3' cli ( $ sudo apt install sqlite3 )
# Optional, to download in half the time and data size, install 'unzip' command ( $ sudo apt install unzip )
#
# Created by Dimitris Georgiou <olyair11@gmail.com>
#
un_zip=0
minzize=70000000
echo "..."
echo "Shell script that generates the SQLite3 BaseStation aircraft database for dumpvdl2, dumphfdl, JAERO."
echo "Data provided courtesy of Opensky Network aircraft database file ( https://opensky-network.org )"
echo "..."
if ! command sqlite3 -version >/dev/null; then
    echo "sqlite3 command not found. Install 'sqlite3' cli."
    exit
fi
if ! command unzip -v >/dev/null; then
    echo "Optional, unzip command not found. If installed downloading will take half the time and data size."
    read -p "Hit Enter key to continue, Ctrl+c to stop ..."
else
    ((un_zip++))
fi
rm -f aircraftDatabase.csv aircraftDatabase.zip
echo "Stand by while downloading latest Opensky Network database file ..."
if [ $un_zip -ne 0 ]; then
    wget --no-check-certificate -q https://opensky-network.org/datasets/metadata/aircraftDatabase.zip
    if [ ! -f aircraftDatabase.zip ]; then
	wget --no-check-certificate -q https://opensky-network.org/datasets/metadata/aircraftDatabase.csv
    else
	unzip -q -j aircraftDatabase.zip
    fi
else
    wget --no-check-certificate -q https://opensky-network.org/datasets/metadata/aircraftDatabase.csv
fi
if [ ! -f aircraftDatabase.csv ]; then
    echo "Could not download from Opensky Network ..."
    exit
fi
dbsize=$(stat -c %s aircraftDatabase.csv)
printf "Downloaded aircraftDatabase file size is %d bytes.\n" $dbsize
if [ $dbsize -lt $minzize ]; then
    echo "Downloaded file size is smaller than $minzize, corrupted file ?"
    exit
else
    sed '3,$ {s/[^,]*/\U&/1}' aircraftDatabase.csv | sed -e '1,2d' > aircraftDatabaseu.csv
    all_acs=$(wc -l aircraftDatabaseu.csv | awk '{ print $1 }')
    echo "Generating the BaseStation database from $all_acs entries ..."
    rm -f BaseStation.sqb
    bs_sql="./bs_db.sql"
    cat >"$bs_sql" <<"EOF"
PRAGMA foreign_keys=OFF;
PRAGMA synchronous=OFF;
BEGIN TRANSACTION;
CREATE TEMP TABLE _csv_import("icao24" TEXT,"registration" TEXT,"manufacturericao" TEXT,"manufacturername" TEXT,"model" TEXT,"typecode" TEXT,"serialnumber" TEXT,"linenumber" TEXT,"icaoaircrafttype" TEXT,"operator" TEXT,"operatorcallsign" TEXT,"operatoricao" TEXT,"operatoriata" TEXT,"owner" TEXT,"testreg" TEXT,"registered" TEXT,"reguntil" TEXT,"status" TEXT,"built" TEXT,"firstflightdate" TEXT,"seatconfiguration" TEXT,"engines" TEXT,"modes" TEXT,"adsb" TEXT,"acars" TEXT,"notes" TEXT,"categoryDescription" TEXT);
.mode csv
.separator ","
.import aircraftDatabaseu.csv _csv_import
DROP TABLE IF EXISTS Aircraft;
CREATE TABLE Aircraft(ModeS PRIMARY KEY, Registration, ICAOTypeCode, OperatorFlagCode, Manufacturer, Type, RegisteredOwners);
INSERT OR IGNORE INTO Aircraft SELECT icao24, REPLACE(registration, ' ', ''), typecode, operatoricao, manufacturericao, REPLACE(model, "'", '`'), REPLACE(REPLACE(owner, X'0A', ' '), "'", '`')
FROM _csv_import WHERE registration != '' AND LENGTH(registration) < 10 ORDER BY icao24;
DROP TABLE _csv_import;
COMMIT;
.exit
EOF
    sqlite3 BaseStation.sqb < bs_db.sql
    db_acs=$(sqlite3 BaseStation.sqb 'SELECT count(*) FROM Aircraft;')
    rm -f aircraftDatabase.csv aircraftDatabaseu.csv bs_db.sql
    echo "BaseStation database created successfully with $db_acs aircraft records."
fi
exit
