# NEWS

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
