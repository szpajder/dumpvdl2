# NEWS

## Version 2.5.1 (2026-01-06)

* Fixed failed decoding of VDL2 blocks whose length is a multiple of 249
  octets (thx @terrymarkovich).

## Version 2.5.0 (2025-11-02)

* Added `--max-ppm <ppm>` option which drops messages if their PPM error exceeds
  a configured threshold. This is useful for filtering out loud message
  crosstalk from adjacent channels and avoiding cross-channel duplicates (thx
  @f00b4r0).
* `--oversample` option may now be used for any input type, not only
  `--iq-file`.
* Decoding of raw frames from standard input is now done without buffering. This
  improves interaction with 3rd party message generators and simulators (thx
  @sbabbi).

## Version 2.4.0 (2024-10-10)

* Allow specifying frequencies in kHz, MHz or GHz. Frequencies might be
  specified in Hertz (as integer numbers) or in kHz, MHz, GHz (as integer or
  floating-point numbers followed by any of the following suffixes: k, K, m, M,
  g, G). This applies to the --centerfreq option argument and VDL2 channel
  frequencies.
* Added support for RTLSDR bias tee. "--bias 1" command line option enables
  it, "--bias 0" disables. Default: disabled. (thx @Reoost)
* Fix RTLSDR device selection. If the --rtlsdr option argument is exactly
  8 characters long, the program will now always attempt to find the device
  by its serial number rather than by the device index. Now "--rtlsdr 00000002"
  means "device with a serial number 00000002" rather than "device with
  and index of 2". (thx @f00b4r0)
* Exit on SoapySDR errors. If the program is unable to continue reading
  samples from the radio, it terminates, so that it could be restarted by
  the service manager (eg. systemd). (thx @wiedehopf)
* Do not round timestamps to the nearest millisecond, truncate it instead.
  Rounding may have caused message timestamps to be pushed forward. (thx
  @f00b4r0)
* Report loud (ie. possibly overloaded) messages via statsd statistics.
  Introduces a new counter "decoder.msg.good_loud" which gets incremented on
  every good message with a signal level above 0 dBFS. Might be useful for
  tuning SDR gain levels. (thx @f00b4r0)
* Enable IF bandwidth filter on RTLSDR and SoapySDR devices. This could
  potentially help in noisy environments by improving rejection of nearby strong
  signals. The filter is automatically enabled if supported by the SDR. (thx
  @f00b4r0)
* Print Maintenance/Initialized (M/I) status bit in text output of X.25 Call
  Request and Call Accepted messages.
* Fix libacars library search issue at runtime on MacOS 14 and 15.

## Version 2.3.0 (2023-08-22)
* Allow reading raw frames or I/Q data from standard input. To enable this,
  specify "-" as the argument to `--iq-file` or `--raw-frames-file` option,
  respectively.
* Added `--prettify-json` command line option which enables prettification of
  JSON payloads in libacars >= 2.2.0. This currently applies to OHMA messages
  only.
* Fixed incompatibility with libacars 2.2.0 which might cause a crash during
  reassembly of CLNP packets.

## Version 2.2.0 (2022-06-11)

* Added support for reassembly of multipart CLNP and COTP packets. This brings
  the ability to decode ADS-C v2 messages which are too large to fit in a single
  CLNP / COTP packet, even if fragmented in the X.25 layer.
* Added proper formatting of Route Clearance CPDLC message elements.
* Removed the obsolete 8-channel limit. The maximum number of VDL2 channels that
  can be decoded simultaneously is now limited only by the hardware (thx
  @rpatel3001).
* Station ID (specified with --station-id option) is now appended to the StatsD
  namespace if StatsD metric collection is enabled. For example, if the station
  ID is set to KLAX, the new namespace would be "dumpvdl2.KLAX" instead of just
  "dumpvdl2". This allows simultaneous monitoring of several dumpvdl2 instances
  separately in Grafana (thx @cdschuett).
* Fixed a crash which occurred when the ground station list file could not be
  opened.

## Version 2.1.1 (2021-07-08)

* Fixed wrong ZeroMQ version check, which incorrectly declared ZMQ version
4.0.x as too old (#18)

## Version 2.1.0 (2020-11-07)

* Added full JSON formatting for all protocols and message types. This is
  supported on all output types - file, udp and zmq.  Together with multiple
  outputs feature that was introduced in version 2.0.0 this enables arbitrarily
  flexible arrangements, like logging to a file as text, logging to another
  file as JSON and sending JSON across the network using UDP and/or ZMQ. When
  collecting data from multiple receivers, --station-id option may be used to
  assign unique names to  receivers in order to discriminate input from each
  one.

* Removed a few duplicate routines that have their counterparts in libacars.

## Version 2.0.1 (2020-08-25)

* Fixed build failure with gcc version 10.

## Version 2.0.0 (2020-08-24)

* Major overhaul of the output subsystem. The new architecture is modular,
  multithreaded and can easily be extended with new output drivers. It also
  allows using multiple outputs simultaneously.
* New output configuration scheme. `--output-file` `--daily`, `--hourly` and
  `--output-acars-pp` options have been removed. All outputs are now configured
  using `--output` option. Adjusting command line parameters is therefore
  required to run the program. Run `dumpvdl2 --output help` for instructions or
  see "Output configuration" section in README.md for details and examples.
* Support for streaming messages over network using UDP/IP.
* Support for streaming messages over network using ZeroMQ publisher sockets.
  Requires libzmq library.
* Support for storing AVLC frames as raw data (without decoding) in binary
  files for archiving purposes. File contents can then be decoded anytime
  later, as if the frames were just received from the air. Requires protobuf-c
  library.
* Minor bugfixes.

## Version 1.10.1 (2020-07-05)

* Added support for SDRPlay API version 3 which is required for new devices
  (notably RSPdx). API version 2 is still supported, however it will probably
  be removed in a future release. It is possible to have both versions
  installed simultaneously and to choose which one to use at runtime. See
  README.md for details.
* Added `--milliseconds` option which enables printing timestamps in message
  headers up to a millisecond precision. Note that the arrival timestamp is
  stored per VDL2 burst, not per VDL2 message. Since a VDL2 burst may contains
  more than one VDL2 message, all messages extracted from a single burst will
  still have exactly the same timestamp value, regardless of whether
  millisecond precision has been enabled or not.

## Version 1.10.0 (not released)

## Version 1.9.0 (2020-02-13)

* Added decoder for ISO 8650 / X.227 Association Control Service (ACSE).
* Improved decoder for ISO 8327 / X.225 Session Protocol.
* Restructured code of ATN applications decoder. The previous implementation
  was a little messy and was unable to handle a few uncommon types of messages
  (like ACSE Abort PDU with no payload).  This should bring the number of
  undecoded ATN messages down to zero.
* Enable debugging output from SDRPlay driver only if `--debug sdr` option is used.
* Fix formatting of geo coordinates in ADS-C v2 messages.

## Version 1.8.2 (2020-01-27)

* Fix another crash opportunity due to missing sanity check on unparseable ACARS
  messages.
* Fix ACARS output to Planeplotter. Only first three characters of Message
  Serial Number (MSN) field were sent instead of all four.

## Version 1.8.1 (2020-01-23)

* Fix a bug where an unparseable ACARS message could cause the program to crash
* Add a little bit of optimization when compiling with debugging enabled. Debug
  builds are now compiled with -Og flag rather than -O0. This makes debugging on
  a Raspberry Pi feasible - when -O0 was used, CPU usage was often too high and
  caused loss of samples and reduced decoding success rate.

## Version 1.8.0 (2020-01-16)

* Support for ATN-B2 Automatic Dependent Surveillance-Contract (ADS-C)
  version  2.
* Support for automatic reassembly of multiblock ACARS messages, MIAM file
  transfers and fragmented X.25 packets. Contents of reassembled messages is now
  logged in one piece and passed upwards to decoders of higher layer protocols /
  applications. Thanks to this, many large messages which so far were decoded
  partially and logged with "Unparseable ... PDU" errors due to fragmentation,
  are now decoded correctly. StatsD metrics for monitoring reassembly engine
  performance have been added.
* Logged messages may be enriched with ground stations details read from a text
  file in MultiPSK GS format. Look up `--gs-file` and `--addrinfo` options in
  README.md and dumpvdl2 usage text for more details.
* Logged messages may be enriched with aircraft details read from a Basestation
  SQLite database. SQLite3 library must be installed prior to compiling dumpvdl2
  in order for this feature to be enabled. Look up `--bs-db` and `--addrinfo`
  options in README.md and dumpvdl2 usage text for more details. Database
  entries are cached in memory. There are new StatsD metrics for monitoring
  performance and utilization of the cache.
* New `--prettify-xml` option enables pretty-printing of XML documents carried
  in ACARS and MIAM CORE messages. The purpose is to improve readability. If
  enabled, XML content will be printed as multiline text with proper indentation.
  This requires libacars built with libxml2 support.
* ACARS sublabel and MFI fields are now stripped from message text and logged as
  separate fields (if present).
* When compiled with debugging support, dumpvdl2 now has a new `--debug` command
  line option allowing configurable verbosity of debug messages. No debug output
  is produced by default. Refer to README.md for more details.
* Added `extras` subdirectory to the source tree. Additional content related to
  dumpvdl2 will be stored here. Currently bundled extras are: multitail color
  scheme for dumpvdl2 log files and vdl2grep script for grepping dumpvdl2 log
  files.
* Bug fixes, code cleanups.
* libacars version 2.0.0 or later is now required to compile and run dumpvdl2.

## Version 1.7.1 (2019-11-11)

* Fixed an issue where a truncated or corrupted ICAO APDU could cause a
  crash in ASN.1 decoder.
* Fixed an issue where NULL characters in XID attribute values could
  cause them do be printed partially. Any non-printable characters
  in octet strings printed as text are now replaced with periods.

## Version 1.7.0 (2019-08-11)

* Complete overhaul of output generation code. Messages are no longer printed
  directly into the output file, but rather stored in memory in a structured
  manner and serialized into an output string (still in memory), which is then
  printed into an output file. This change does not bring significant benefits
  or features yet, but is a major step towards multi-output, multi-format
  architecture in the next release. A side effect of this is more concise
  output formatting (indentation). The purpose of this is to better visualise
  message hierarchy (layered structure). **Note**: libacars version 1.3.0
  or later is now required to compile and run dumpvdl2.
* Significant enhancements to CLNP, COTP and IDRP decoders. Most if not all
  protocol headers are now decoded and included in the output.
* Added a decoder for X.225 Session Protocol SPDUs.
* Added a decoder for VDL SNDCF error reports.
* XID: improved decoding of XID sequencing, Frequency support and System mask
  attributes.
* X.25: improved decoding of facility fields, call clearing causes and
  diagnostic codes.
* Minor bug fixes.

## Version 1.6.0 (2019-01-19)

* New build system based on cmake. Refer to README.md for new installation
  instructions.
* FANS/1-A CPDLC, ADS-C and ACARS decoders have been removed. These features
  have been moved to libacars library, which is now a mandatory dependency
  (compilation will fail if libacars is not installed). Refer to README.md for
  details.
* Added support for SoapySDR library (contributed by Fabrice Crohas). However
  there are still some limitations with respect to SDR types which dumpvdl2
  can work with. As of now, the device must support a sampling rate of 2100000
  samples per second. This will be addressed in a future release.
* The program should now compile and run on MacOS, however this hasn't been
  tested well. Feedback and bug reports are welcome.
* Minor bugfixes

## Version 1.5.0 (2018-11-17)

* Sync with the latest SDRPlay API v. 2.13 which is now required to compile the
  program
* Added support for SDRPlay RSPduo (with tuner selection, bias-T and notch
  filters)
* Added support for RSP1A features: Bias-T and broadcast notch filter
* Minor bugfixes

## Version 1.4.0 (2018-08-01)

* Major improvements to the DSP and decoding engine - gives much better decoding
  success rate, especially for weak transmissions from distant aircraft.
* Each VDL2 channel is now processed in a separate thread, which allows
  balancing the load onto multiple CPU cores.
* Fixed a nasty decoder bug which caused good frames to be corrupted during
  decoding stage and therefore missed.
* Added CRC checking for ACARS messages. If there is a CRC mismatch, the message
  will still be logged, but with an appropriate warning.
* New StatsD metric `avlc.msg.air2air` - counts messages from aircraft to
  aircraft (however strange it seems, such things happen in real life).
* New StatsD metric `decoder.errors.too_long` - counts VDL2 bursts dropped due
  to unreasonably large value in the length field.
* Due to changes in the decoding engine, the following StatsD metric are now
  obsolete and have been removed: `avlc.errors.no_flag_start`,
  `avlc.errors.no_flag_end`, `decoder.errors.no_preamble`.
* Added `--extended-header` command line option which causes a few decoder
  diagnostic parameters to be logged for each message.  Refer to the FAQ in
  README.md for description of the fields.
* Other minor bugfixes and improvements.
* C11-capable compiler is now required to build the program.

## Version 1.3.1 (2018-05-27)

* Added `decpdlc` utility which decodes FANS-1/A CPDLC messages supplied from
  command line or from a file. Can be used to decode CPDLC traffic received via
  other media than VDL-2 (eg. ACARS, HFDL, SATCOM). Refer to "decpdlc" section in
  README.md for details.

## Version 1.3.0 (2018-04-11)

* Added decoder for FANS-1/A ADS-C messages
* Added decoder for FANS-1/A CPDLC messages
* Human-readable formatting of CPDLC and Context Management messages (if you
  still want the old format, add `--dump-asn1` command line option)
* Added install rule to Makefile
* Added a sample systemd service file
* Bug fixes

## Version 1.2.0 (2018-01-01)

* Better input signal filtering - reduced bit error rate, 10-50% more messages
  successfully decoded.
* SDRPlay: major bugfixes in sample buffer handling code - shall now perform
  equally well as RTLSDR (or even better).
* SDRPlay: --gain knob replaced with --gr (gain reduction). This makes gain
  configuration in dumpvdl2 compatible with other SDRPlay apps. See README.md
  for details.
* SDRPlay: support for selecting device by serial number.
* Added optional build-time PLATFORM knob which sets CPU-specific compiler flags
  to optimize the code for various flavours of Raspberry Pi. Supported values:
  rpiv1, rpiv2, rpiv3.
* Other minor bug fixes

## Version 1.1.0 (2017-06-20)

* Support for SDRPlay RSP1 and 2 using native binary API
* Added dissector for ISO 8073/X.224 Connection-oriented Transport Protocol
* Added dissectors for ICAO Context Management and CPDLC
* Enhancements for ES-IS and XID dissectors
* Support for referring to dongles using their serial numbers
* Output formatting fixes
* Bug fixes

## Version 1.0.0 (2017-02-26)

* First public release

// vim: textwidth=80
