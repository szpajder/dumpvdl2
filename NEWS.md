# NEWS

### Version 1.3.1 (2018-05-27):
* Added `decpdlc` utility which decodes FANS-1/A CPDLC messages
  supplied from command line or from a file. Can be used to decode
  CPDLC traffic received via other media than VDL-2 (eg. ACARS,
  HFDL, SATCOM). Refer to "decpdlc" section in README.md for details.

### Version 1.3.0 (2018-04-11):
* Added decoder for FANS-1/A ADS-C messages
* Added decoder for FANS-1/A CPDLC messages
* Human-readable formatting of CPDLC and Context Management messages
  (if you still want the old format, add `--dump-asn1` command line
  option)
* Added install rule to Makefile
* Added a sample systemd service file
* Bug fixes

### Version 1.2.0 (2018-01-01):
* Better input signal filtering - reduced bit error rate, 10-50% more
  messages successfully decoded.
* SDRPlay: major bugfixes in sample buffer handling code - shall now
  perform equally well as RTLSDR (or even better).
* SDRPlay: --gain knob replaced with --gr (gain reduction). This makes
  gain configuration in dumpvdl2 compatible with other SDRPlay apps.
  See README.md for details.
* SDRPlay: support for selecting device by serial number.
* Added optional build-time PLATFORM knob which sets CPU-specific
  compiler flags to optimize the code for various flavours of
  Raspberry Pi. Supported values: rpiv1, rpiv2, rpiv3.
* Other minor bug fixes

### Version 1.1.0 (2017-06-20):
* Support for SDRPlay RSP1 and 2 using native binary API
* Added dissector for ISO 8073/X.224 Connection-oriented Transport Protocol
* Added dissectors for ICAO Context Management and CPDLC
* Enhancements for ES-IS and XID dissectors
* Support for referring to dongles using their serial numbers
* Output formatting fixes
* Bug fixes

### Version 1.0.0 (2017-02-26):
* First public release
