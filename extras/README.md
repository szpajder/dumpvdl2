# dumpvdl2 extras

- `vdl2grep` - Perl script for grepping dumpvdl2 log files. While standard grep
  displays only matching lines, vdl2grep shows whole VDL2 messages.

- `multitail-dumpvdl2.conf` - an example coloring scheme for dumpvdl2 log files.
  To be used with `multitail` program.

- `makebsdb.sh` - Shell script to generate the BaseStation.sqb SQLite3 database, 
  in order for dumpvdl2 to enrich aircraft information.
  - Requires sqlite3 command line interface, install 'sqlite3' cli.

- `ap4dumpvdl2.sh` - Shell script to generate an airports SQLite3 database, 
  in order for dumpvdl2 to enrich Destination airports and Alternate ground stations information.
  - Requires sqlite3 command line interface, install 'sqlite3' cli.
  - Optional, more Airports data, not included in ourairports.com database file, "more-aps.sql" file.

- `xform-gs-txt.sh` - Shell script to transform 'VDL2_Ground_Stations.txt' file, 
  in order for dumpvdl2 to enrich with more informative GS details.
  - Requires the airports database, built with 'ap4dumpvdl2.sh' script.
  - Optional, recover some GSs that could not be auto-transformed correctly, "more-gss.txt" file.

