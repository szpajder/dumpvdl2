#!/bin/bash
#
# Shell script that generates the Airports database for dumpvdl2 ( https://github.com/szpajder/dumpvdl2 )
# Data provided courtesy of OurAirports Network airports database file ( https://ourairports.com )
#
# Requires sqlite3 command line interface, install 'sqlite3' cli ( $ sudo apt install sqlite3 )
# Optional, more Airports data, not included in ourairports.com database file, "more-aps.sql" file
#
# Created by Dimitris Georgiou <olyair11@gmail.com>
#
echo "..."
echo "Shell script that generates the Airports database for dumpvdl2."
echo "Data provided courtesy of OurAirports Network airports database file ( https://ourairports.com )"
echo "..."
ap_sql="./ap4dumpvdl2.sql"
cat >"$ap_sql" <<"EOF"
PRAGMA foreign_keys=OFF;
PRAGMA synchronous=OFF;
BEGIN TRANSACTION;
CREATE TEMP TABLE _airports_import("id" INTEGER,"ident" TEXT,"type" TEXT,"name" TEXT,"latitude_deg" REAL,"longitude_deg" REAL,"elevation_ft" INTEGER,"continent" TEXT,"country_name" TEXT,"iso_country" TEXT,"region_name" TEXT,"iso_region" TEXT,"local_region" TEXT,"municipality" TEXT,"scheduled_service" TEXT,"gps_code" TEXT,"iata_code" TEXT,"local_code" TEXT,"home_link" TEXT,"wikipedia_link" TEXT,"keywords" TEXT,"score" TEXT,"last_updated" TEXT);
.mode csv
.separator ","
.import airports4.csv _airports_import
DROP TABLE IF EXISTS AIRPORTS;
CREATE TABLE AIRPORTS(ICAO TEXT PRIMARY KEY, IATA TEXT, NAME TEXT, CITY TEXT, COUNTRISO TEXT, COUNTRY TEXT, LAT REAL, LON REAL);
INSERT OR IGNORE INTO AIRPORTS SELECT gps_code, iata_code, name,
CASE WHEN iso_country = "US" THEN  municipality || ", " || local_region ELSE CASE WHEN LENGTH(municipality) = 0 THEN "Unknown City" ELSE municipality END END, 
iso_country, country_name, latitude_deg, longitude_deg 
FROM _airports_import 
WHERE gps_code != '' AND LENGTH(gps_code) = 4 AND (type = 'medium_airport' OR type = 'large_airport' OR type = 'small_airport') 
ORDER BY gps_code;
DROP TABLE _airports_import;
COMMIT;
.exit
EOF
more_aps=0
min_airports_size=11000000
if [ ! -f more-aps.sql ]; then
    echo "Optional 'more-aps.sql' file not found."
    read -p "Hit Enter key to continue, Ctrl+c to stop ..."
else
    ((more_aps++))
fi
if ! command sqlite3 -version >/dev/null; then
    echo "sqlite3 command not found. Install 'sqlite3' cli."
    exit
fi
echo "Stand by while downloading latest ourairports.com file ..."
rm -f airports.csv
wget -q https://ourairports.com/airports.csv
if [ ! -f airports.csv ]; then
    echo "Could not download from ourairports.com ..."
    exit
fi
airports_size=$(stat -c %s airports.csv)
printf "Downloaded airports.csv file size is %d bytes.\n" $airports_size
if [ $airports_size -lt $min_airports_size ]; then
	echo "Downloaded airports.csv file size is smaller than $min_airports_zize, corrupted file ?"
    exit
else
    rm -f ap4dumpvdl2.sqb
    sed -i 1d airports.csv
    sed 's/ \/ /\//g' airports.csv | sed 's/%/_/g' | sed 's/\x27/`/g' > airports4.csv
    all_aps=$(wc -l airports4.csv | awk '{ print $1 }')
    echo "Generating the Airports database  from $all_aps entries ..."
    sqlite3 ap4dumpvdl2.sqb < ap4dumpvdl2.sql
    if [ $more_aps -ne 0 ]; then
	sqlite3 ap4dumpvdl2.sqb < more-aps.sql
    fi
    rm -f airports.csv airports4.csv ap4dumpvdl2.sql
    db_aps=$(sqlite3 ap4dumpvdl2.sqb 'SELECT count(*) FROM AIRPORTS;')
    echo "Airports database for dumpvdl2 created successfully with $db_aps airports records."
fi
exit
