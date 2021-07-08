# dumpvdl2

dumpvdl2 is a VDL Mode 2 message decoder and protocol analyzer.

Current stable version: 2.1.1 (released July 8, 2021)

## Features

- Runs under Linux (tested on: x86, x86-64, ARM) and MacOS (not tested very
  well, feedback welcome)
- Supports following SDR hardware:
  - RTLSDR (via [rtl-sdr library](http://osmocom.org/projects/sdr/wiki/rtl-sdr))
  - Mirics SDR (via [libmirisdr-4](https://github.com/f4exb/libmirisdr-4))
  - SDRPlay RSP (native support through official driver version 2 and 3)
  - SoapySDR (via [soapy-sdr project](https://github.com/pothosware/SoapySDR/wiki))
  - prerecorded IQ data from a file
- Decodes up to 8 VDL2 channels simultaneously
- Automatically reassembles multiblock ACARS messages, MIAM file transfers, and
  fragmented X.25 packets
- Supports various outputs and output formats (see below)
- Enriches logged messages with ground station details read from a text file
  (MultiPSK format)
- Enriches logged messages with aircraft data read from Basestation SQLite
  database
- Supports message filtering by type or direction (uplink, downlink)
- Can store raw frames in a binary file for later decoding or archiving
  purposes.
- Produces decoding statistics using [Etsy StatsD](https://github.com/etsy/statsd) protocol

## Supported output formats

- Human readable text
- JSON
- Single-line ACARS format accepted by Planeplotter
- Custom binary format (suitable for storing raw frames)

## Supported output types

- file (with optional daily or hourly file rotation)
- reliable network messaging via [ZeroMQ](https://zeromq.org/)
- UDP socket

## Example

![dumpvdl2 screenshot](example.png?raw=true)

## Supported protocols

- Aviation Link Control (AVLC)
- ACARS over AVLC
- ISO 8208 / X.25 DTE-DCE Interface
- ISO 8473 / X.233 Connectionless Network Protocol (CLNP)
- ISO 8073 / X.224 Connection Oriented Transport Protocol (COTP)
- ISO 8327 / X.225 Session Protocol
- ISO 8650 / X.227 Association Control Service Element (ACSE)
- ISO 9542 End System to Intermediate System (ES-IS)
- ISO 10747 Inter-Domain Routing Protocol (IDRP)
- ATN-B1 Context Management
- ATN-B1 Controller-Pilot Data Link Communications, version 1 (CPDLC)
- ATN-B2 Automatic Dependent Surveillance - Contract, version 2 (ADS-C)
- All applications and protocols handled by libacars library (full list [here](https://github.com/szpajder/libacars/blob/master/README.md#supported-message-types))

## Installation

### Dependencies

Mandatory dependencies:

- gcc
- make
- cmake
- pkg-config
- git (unless you intend to use only packaged releases of dumpvdl2 and all
  dependencies)
- glib2
- libacars 2.1.0 or later

Optional dependencies:

- SDR device drivers:
  - librtlsdr
  - libmirisdr-4
  - SDRPlay binary driver
  - SoapySDR
- Dependencies for optional features:
  - sqlite3 (for enriching messages with aircraft data read from SQB database)
  - statsd-c-client (for Etsy StatsD statistics)
  - libprotobuf-c 1.3.0 or later (for binary format support)
  - libzmq 3.2.0 or later (for ZeroMQ networked output)

Install necessary dependencies (unless you have them already). Example for
Debian / Raspbian:

```
sudo apt install build-essential cmake git libglib2.0-dev pkg-config
```

Install `libacars` library - either:

- download a stable release package from [here](https://github.com/szpajder/libacars/releases)
- or clone the source repository with:

```
cd
git clone https://github.com/szpajder/libacars
cd libacars
```

Compile and install the library:

```
mkdir build
cd build
cmake ../
make
sudo make install
sudo ldconfig
```

#### RTLSDR support (optional)

To use RTL dongles, install `librtlsdr` library (unless you have it already).
Raspbian has a packaged version:

```
apt install librtlsdr-dev
```

If your distribution does not provide a package, then clone the source
repository and compile manually:

```
apt install libtool autoconf libusb-1.0-0-dev
cd
git clone git://git.osmocom.org/rtl-sdr.git
cd rtl-sdr/
autoreconf -i
./configure
make
sudo make install
sudo ldconfig
sudo cp $HOME/rtl-sdr/rtl-sdr.rules /etc/udev/rules.d/rtl-sdr.rules
```

#### Mirics support (optional)

libmirisdr-4 is an open-source alternative to SDRPlay binary driver (Mirics is
the chipset brand which SDRPlay RSPs are based on). However, as of December
2017, it works properly with RSP1 only. For other RSP types (RSP2, RSP/1A) gain
control does not work too well, so the native closed source driver is a better
option (see next section).  libmirisdr-4 is a good choice for RSP1 and various
Mirics-based DVB-T dongles which are detected as RSP1 device. An advantage over
RSP binary API is lower CPU utilization in dumpvdl2 thanks to a lower sampling
rate.

Install `libmirisdr-4` library:

```
apt install libusb-1.0-0-dev
cd
git clone https://github.com/f4exb/libmirisdr-4.git
cd libmirisdr-4
./build.sh
cd build
sudo make install
sudo ldconfig
sudo cp $HOME/libmirisdr-4/mirisdr.rules /etc/udev/rules.d/mirisdr.rules
```

#### SDRPLAY RSP support (optional)

Download and install API/hardware driver package from [here](
http://www.sdrplay.com/downloads/).  Make sure you have selected the right
hardware platform before downloading, otherwise the installer will fail.
dumpvdl2 supports both version 2 and 3 of the driver. Version 3 is needed
for newer devices (like RSPdx). Older hardware works with both versions.
You can have both versions installed simultaneously and choose either one
when running the program.

#### SoapySDR support (optional)

Download and install the SoapySDR library from [here](https://github.com/pothosware/SoapySDR).
Then install the driver module for your device. Refer to [SoapySDR wiki](https://github.com/pothosware/SoapySDR/wiki)
for a list of all supported modules.

**Note:** The device must support a sampling rate of 2100000 samples per second
to work correctly with dumpvdl2. It is therefore not possible to use devices
which only support predefined, fixed sampling rates (notably Airspies). This
limitation will be removed in a future release of dumpvdl2.

#### SQLite (optional)

VDL2 message addressing is based on ICAO 24-bit hex codes (same as ADS-B).
dumpvdl2 may use your basestation.sqb database to enrich logged messages with
aircraft data (registration number, operator, type, etc). If you want this
feature, install SQLite3 library:

```
sudo apt install libsqlite3-dev
sudo ldconfig
```

#### Etsy StatsD statistics (optional)

Install `statsd-c-client` library from https://github.com/romanbsd/statsd-c-client:

```
cd
git clone https://github.com/romanbsd/statsd-c-client.git
cd statsd-c-client
make
sudo make install
sudo ldconfig
```

#### Binary input/output format support (optional)

dumpvdl2 can write raw AVLC frames into a binary file. Each frame is stored
together with its metadata (ie. timestamp of reception, channel frequency,
signal level, noise level, etc). Frames stored in such a file can be later read
back and decoded as if they were just received from the air.  To enable this
feature, install [protobuf-c](https://github.com/protobuf-c/protobuf-c) library.
On Debian/Rasbian Buster just do this:

```
sudo apt install libprotobuf-c-dev
```
It won't work on Debian/Raspbian versions older than Buster, since protobuf-c
library shipped with these is too old.

#### ZeroMQ networked output support (optional)

ZeroMQ is a library that allows reliable messaging between applications to be
set up easily. dumpvdl2 can publish decoded messages on a ZeroMQ socket and
other apps can receive them over the network using reliable transport (TCP).
To enable this feature, install libzmq library:

```
sudo apt install libzmq3-dev
```

It won't work on Debian/Raspbian versions older than Buster, since libzmq
library shipped with these is too old.

### Compiling dumpvdl2

- Download a stable release package from [here](https://github.com/szpajder/dumpvdl2/releases) and unpack it...
- ...or clone the repository:

```
cd
git clone https://github.com/szpajder/dumpvdl2.git
cd dumpvdl2
```

Configure the build:

```
mkdir build
cd build
cmake ../
```

`cmake` attempts to find all required libraries and SDR drivers. If a mandatory
dependency is not installed, it will throw out an error. Missing optional
dependencies cause relevant features to be disabled. At the end of the process
`cmake` displays a short configuration summary, like this:

```
-- dumpvdl2 configuration summary:
-- - SDR drivers:
--   - librtsdr:                requested: ON, enabled: TRUE
--   - mirisdr:                 requested: ON, enabled: TRUE
--   - sdrplay v2:              requested: ON, enabled: TRUE
--   - sdrplay v3:              requested: ON, enabled: TRUE
--   - soapysdr:                requested: ON, enabled: TRUE
-- - Other options:
--   - Etsy StatsD:             requested: ON, enabled: TRUE
--   - SQLite:                  requested: ON, enabled: TRUE
--   - ZeroMQ:                  requested: ON, enabled: TRUE
--   - Raw frame output:        requested: ON, enabled: TRUE
-- Configuring done
```

Here you can verify whether all the optional components that you need were properly
detected and enabled. Then compile and install the program:

```
make
sudo make install
```

The last command installs the binary named `dumpvdl2` to the default bin
directory (on Linux it's `/usr/local/bin`). To display a list of available
command line options, run:

```
/usr/local/bin/dumpvdl2 --help
```

or just `dumpvdl2 --help` if `/usr/local/bin` is in your `PATH`.

### Build options

Build options can be configured with `-D` option to `cmake`, for example:

```
cmake -DRTLSDR=FALSE ../
```

causes RTLSDR support in dumpvdl2 to be disabled. It will not be compiled in,
even if librtlsdr library is installed.

Disabling optional features:

- `-DRTLSDR=FALSE`
- `-DMIRISDR=FALSE`
- `-DSDRPLAY=FALSE`
- `-DSOAPYSDR=FALSE`
- `-DSQLITE=FALSE`
- `-DETSY_STATSD=FALSE`
- `-DRAW_BINARY_FORMAT=FALSE`
- `-DZMQ=FALSE`

Setting build type:

- `-DCMAKE_BUILD_TYPE=Debug` - builds the program without optimizations and
  enables `--debug` command line option which enables debug messages (useful for
  troubleshooting, not recommended for general use)
- `-DCMAKE_BUILD_TYPE=Release` - debugging output disabled (the default)

**Note:** Always recompile the program with `make` command after changing build
options.

**Note:** `cmake` stores build option values in its cache. Subsequent runs of
`cmake` will cause values set during previous runs to be preserved, unless they
are explicitly overriden with `-D` option. So if you disable a feature with, eg.
`-DRTLSDR=FALSE` and you want to re-enable it later, you have to explicitly use
`-DRTLSDR=TRUE` option. Just omitting `-DRTLSDR=FALSE` will not revert the
option value to the default.

### Enabling and disabling optional features

As described in the "Dependencies" section, optional features (like SQLite
support, binary format support, etc) are enabled automatically whenever
libraries they depend upon are found during cmake run. Results of library
searches are also stored in cmake's cache. If the program has initially been
built without a particular feature and you later change your mind and decide to
enable it, you need to:

- install the library required by the feature
- remove cmake's cache file to force all checks to be done again:

```
cd build
rm CMakeCache.txt
```

- rerun cmake and recompile the program as described in "Compiling dumpvdl2"
  section.

## Basic usage

### RTL-SDR

Simplest case on RTLSDR dongle - uses RTL device with index 0, sets the tuner
gain to 40 dB and tuning correction to 42 ppm, listens to the default VDL2
frequency of 136.975 MHz, outputs to standard output:

```
./dumpvdl2 --rtlsdr 0 --gain 40 --correction 42
```

Device ID numbers are not persistent - they depend on the USB device order and
the sequence which they were plugged in. You may specify the device by its
serial number to get deterministic behavior:

```
./dumpvdl2 --rtlsdr 771111153 --gain 40 --correction 42
```

Use `rtl_test` utility to get serial numbers of your devices. dumpvdl2 prints
them to the screen on startup as well.

If you want to decode a different VDL2 channel than the default, just add its
frequency as a last parameter:

```
./dumpvdl2 --rtlsdr 0 --gain 40 --correction 42 136725000
```

dumpvdl2 can decode up to 8 VDL2 channels simultaneously. Just list their
frequencies at the end of the command line:

```
./dumpvdl2 --rtlsdr 0 --gain 40 --correction 42 136725000 136975000 136875000
```

If your receiver has a large center spike, you can set the center frequency a
bit to the side of the desired channel frequency, like this:

```
./dumpvdl2 --rtlsdr 0 --gain 40 --correction 42 --centerfreq 137100000 <channel freqs here...>
```

### Mirics

Mirics is similar, however `libmirisdr-4` library currently lacks support for
configuring correction in ppm. If your receiver needs a non-zero correction, you
can pass the appropriate value in Hertz, instead of ppm. **Note:** this value
will be subtracted from the center frequency, so if your receiver tunes a bit
too low, the parameter value shall be negative:

```
./dumpvdl2 --mirisdr 0 --gain 100 --correction -2500
```

Device serial number can be given instead of ID, the same way, as for RTLSDR
receivers.

`libmirisdr-4` supports two types of hardware: generic Mirics (0 - the default)
and SDRPlay (1).  SDRPlay users should add `--hw-type 1` option. It uses
frequency plans optimized for SDRPlay and reportedly gives better results than
the default mode.

If you get error messages about lost samples on Raspberry Pi, try adding
`--usb-mode 1`.  This switches USB transfer mode from isochronous to bulk, which
is usually enough to rectify this problem. If it does not help, it might be that
your Pi is overloaded or not powerful enough for the task. Try reducing the
number of decoded VDL2 channels as a workaround.

### SDRplay RSP native driver, version 2

In order to use SDRplay driver version 2, select the device with  `--sdrplay`
option. The following devices are supported:

- RSP1
- RSP1A
- RSP2
- RSPduo

The following advanced configuration options are available:

- switching antenna ports (RSP2)
- bias-T (RSP2, RSP1A)
- notch filter for AM/FM broadcast bands (RSP2, RSP1A, RSPduo)
- tuner selection (RSPduo)
- Automatic Gain Control

Type `./dumpvdl2 --help` to find out all the options and their default values.

SDRPlay driver has a concept of "gain reduction", which is an amount of gain (in
decibels) that shall be deducted from the maximum gain. As a result, `--gain`
option is not available with this driver - use `--gr` option to specify
requested end-to-end gain reduction instead.  The smallest possible value is 20.
The highest value depends on receiver type, but it's not that important, because
in dumpvdl2 you will hardly be using a GR larger than 59 dB.

Another way to go is to skip the `--gr` option altogether. This will enable
Automatic Gain Control with a default set point of -30 dBFS, which shall
converge to a reasonable gain reduction value in a couple of seconds after the
program starts. AGC set point can be changed with `--agc` option, but treat this
as an "expert mode" knob, which is hardly ever needed.

Example 1: use SDRplay device ID=0, with auto gain and three VDL2 channels:

```
./dumpvdl2 --sdrplay 0 136975000 136875000 136775000
```

Example 2: use SDRplay device with serial number 35830222, set gain reduction to
40 dB, use antenna A port, disable Bias-T, enable AM/FM notch filter, set
frequency correction to -1ppm:

```
./dumpvdl2 --sdrplay 35830222 --gr 40 --correction -1 --antenna A --biast 0 --notch-filter 1 136975000
```

### SDRplay RSP native driver, version 3

In order to use SDRplay driver version 3, select the device with  `--sdrplay3`
option. The following devices are supported:

- RSP1
- RSP1A
- RSP2
- RSPduo
- RSPdx

The following advanced configuration options are available:

- switching antenna ports (RSP2, RSPdx)
- bias-T (RSP2, RSP1A, RSPdx)
- notch filter for AM/FM broadcast bands (RSP2, RSP1A, RSPduo, RSPdx)
- notch filter for DAB band (RSP1A, RSPduo, RSPdx)
- tuner selection (RSPduo)
- Automatic Gain Control

Type `./dumpvdl2 --help` to find out all the options and their default values.

When version 3 of the driver is used, dumpvdl2 allows controlling each gain
reduction component separately. `--gr` option is not available - there are two
options instead:

- `--ifgr <value_in_dB>` - controls IF gain reduction (range: 20-59 dB)
- `--lna-state <value>` - sets the gain reduction of the LNA (ie. the input RF
  stage). The parameter is a non-negative integer from 0 (meaning: no gain
  reduction) up to N, where N depends on the receiver type. The higher the
  value, the higher the gain reduction (attenuation) in decibels. Refer to the
  "Gain Reduction Tables" section in the [API documentation](https://www.sdrplay.com/docs/SDRplay_RSP_API_Release_Notes_V3.06.pdf) for a full list
  of LNA states and their respective gain reductions for each receiver.

if you want to set the gain reduction manually, specify both `--ifgr` and
`--lna-state`. If either option is omitted, the other one is ignored and
AGC is used instead.

### SoapySDR library

**Note:** The device must support a sampling rate of 2100000 samples per second.

Tested with the following devices:
 - SDRPLAY RSP2
 - RTLSDR

Using SoapySDRServer it is possible to access a SDR device connected to another
machine.

Features supported by dumpvdl2:

- switching antenna ports
- setting device-specific configuration parameters
- setting the gain globally or using individual gain components
- automatic gain control

Type `./dumpvdl2 --help` to find out all the options and their default values.

Type `SoapySDRUtil --find` to find available devices.

Example 1: use SDRPLAY device with Antenna B, AGC and biasT activated:

```
./dumpvdl2 --soapysdr soapy=0,driver=sdrplay --soapy-antenna "Antenna B" --device-settings biasT_ctrl=true 136975000 136875000 136775000
```

Example 2: use RTLSDR device with AGC

```
./dumpvdl2 --soapysdr soapy=0,driver=rtlsdr 136975000 136875000 136775000
```

Example 3: use SDRPLAY device with separate gain reduction for RFGR for LNA and
normal gain reduction IFGR

```
./dumpvdl2 --soapysdr soapy=0,driver=sdrplay --gain -1 --soapy-gain RFGR=0,IFGR=56 136975000 136875000 136775000
```

Example 4: Use a remote SDRPLAY with antenna B, Soapy server started with
command line

```
SoapySDRServer --bind
```

Then you may run dumpvdl2 on any remote machine with:

```
./dumpvdl2 --soapysdr driver=remote,remote=tcp://<ip address>:55132,remote:driver=sdrplay,remote:format=CS16 \
--gain -100 --soapy-antenna "Antenna B" 136975000 136875000 136775000
```

## Configuring outputs

### Quick start

By default dumpvdl2 formats decoded messages into human readable text and prints
it to standard output. You can direct the output to a disk file instead:

```
./dumpvdl2 --output decoded:text:file:path=/some/dir/vdl2.log [other_options]
```

If you want the file to be automatically rotated on top of every hour, do
the following:

```
./dumpvdl2 --output decoded:text:file:path=/some/dir/vdl2.log,rotate=hourly [other_options]
```

The file name will be appended with `_YYYYMMDDHH` suffix.  If file extension is
present, it will be placed after the suffix.

If you prefer daily rotation, change `rotate=hourly` to `rotate=daily`. The file
name suffix will be `_YYYYMMDD` in this case. If file extension is present, it
will be placed after the suffix.

### Output configuration syntax

The `--output` option takes a single parameter consisting of four fields
separated by colons:

```
    <what_to_output>:<output_format>:<output_type>:<output_parameters>
```

where:

- `<what_to_output>` specifies what data should be sent to the output. Two
  values are supported:

  - `decoded` - output decoded messages
  - `raw` - output AVLC frames without decoding (as raw bytes)

- `<output_format>` specifies how the data should be formatted before sending
  it to the output. The following formats are currently supported:

  - `text`
  - `json`
  - `pp_acars` - a single-line ACARS format accepted by Planeplotter via UDP.
    This format can only deal with ACARS, hence messages of all other types will
    be filtered out (ie. not sent to this particular output).
  - `binary`- a format suitable for archiving raw frames without decoding

- `<output_type>` specifies the type of the output. The following output types
  are supported:

  - `file` - output to a file
  - `udp` - output to a remote host via UDP network socket
  - `zmq` - output to a ZeroMQ publisher socket

- `<output_parameters>` - specifies options for this output. The syntax is
  as follows:

```
    param1=value1,param2=value2,...
```

The list of available formats and output types may vary depending on which
optional features have been enabled during program compilation and whether
necessary dependencies are installed (see "Dependencies" subsection above).
Run `dumpvdl2 --output help` to determine which formats and output types
are available on your system. It also shows all parameters supported by
each output type.

Back to the above example:

```
--output decoded:text:file:path=/some/dir/vdl2.log,rotate=hourly
```

It basically says: "take decoded frames, format them as text and output the
result to a file". Of course this output requires some more configuration - at
least it needs the path where the file is to be created. This is done by
specifying `path=/some/dir/vdl2.log` in the last field. The `file` output
driver also supports an optional parameter named `rotate` which indicates
how often the file is to be rotated, if at all.

A few more remarks about how output configuration works:

- Multiple simultaneous outputs are supported. Just specify `--output` option
  more than once.

- Not all combinations of `<what_to_output>` and `<output_format>` are
  supported. For example it does not make sense to specify `raw:pp_acars:....`
  because Planeplotter formatter can only deal with decoded ACARS messages.
  You will get an `Unsupported data_type:format combination: 'raw:pp_acars'`
  error message on startup if you try that.

- Not all combinations of `<output_format>` and `<output_type>` are supported.
  For example, `udp` output only accepts `text`, `json` and `pp_acars` formats.
  If you try using `binary` with that, you will get an `Unsupported
  format:output combination: 'binary:udp'` error message on startup.

- If dumpvdl2 is run without any `--output` option, it creates a default output
  of `decoded:text:file:path=-` which causes decoded frames to be formatted as
  text and printed to standard output.


### Output drivers

#### `file`

Outputs data to a file.

Supported formats: `text`, `json`, `binary`

Parameters:

- `path` (required) - path to the output file. If it already exists, the data is
  appended to it.

- `rotate` (optional) - how often to rotate the file. Supported values: `daily`
  (at midnight UTC or LT depending on whether `--utc` option is used) and `hourly`
  (rotate at the top of every hour). Default: no rotation.

#### `udp`

Sends data to a remote host over network using UDP/IP.

Supported formats: `text`, `json`, `pp_acars`

Parameters:

- `address` (required) - host name or IP address of the remote host

- `port` (required) - remote UDP port number

**Note:** UDP protocol does not guarantee successful message delivery (it works
on a "fire and forget" principle, no retransmissions, no acknowledgements, etc).
If you plan to use networked output for real, please use `zmq` driver. It works
on TCP and provides reliable transport regardless of the message size.

The primary purpose of `udp` driver is to feed Planeplotter with ACARS
messages using `pp_acars` format.

#### `zmq`

Opens a ZeroMQ publisher socket and sends data to it.

Supported formats: `text`, `json`, `pp_acars`

Parameters:

- `mode` (required) - socket mode. Can be `client` or `server`.  In the first
  case dumpvdl2 initiates a connection to the given consumer. In the latter
  case, dumpvdl2 listens on a port and expects consumers to connect to it.

- `endpoint` (required) - ZeroMQ endpoint. The syntax is: `tcp://address:port`.
  When working in server mode, it specifies the address and port where dumpvdl2
  shall listen for incoming connections. In client mode it specifies the address
  and port of the remote ZeroMQ consumer where dumpvdl2 shall connect to.

Examples:

- `mode=server,endpoint=tcp://*:5555` - listen on TCP port 5555 on all local
  addresses.

- `mode=server,endpoint=tcp://10.1.1.1:6666` - listen on TCP port 6666 on
  address 10.1.1.1 (it must be a local address).

- `mode=client,endpoint=tcp://host.example.com:1234` - connect to port 1234
  on host.example.com.

### Diagnosing problems with outputs

Outputs may fail for various reasons. A file output may fail to write to the
given path due to lack of permissions or lack of storage space, zmq output may
fail to set up a socket due to incorrect endpoint syntax, etc. Whenever an
output fails, the program disables it and prints a message on standard error,
for example:

```
Could not open output file /etc/vdl2.log: Permission denied
output_file: could not write to '/etc/vdl2.log', output disabled
```

The program will continue to run and write data to all other outputs, except
the failed one.

An output may also hang and stop processing messages (although this is
a "shouldn't happen" situation). Messages will then accumulate in that output's
queue. To prevent memory exhaustion, there is a high water mark limit on the
number of messages that might be queued for each output. By default it is set
to 1000 messages. If this value is reached, the program will not push any more
messages to that output before messages get consumed and the queue length drops
down. The following message is then printed on standard error for every dropped
message:

```
<output_type> output queue overflow, throttling
```
Other outputs won't be affected, since each one is running in a separate thread
and has its own message queue.

High water mark limit is disabled when dumpvdl2 is decoding data from a file
(ie. eiter `--iq-file` or `--raw-frames-file` option is in use). This allows
all queues to grow indefinitely, but it makes sure that no frames get dropped.

The high water mark threshold can be changed with `--output-queue-hwm` option.
Set its value to 0 to disable the limit.

### Additional options for text formatting

The following options work globally across all outputs with text format:

- Add `--utc` option if you prefer UTC timestamps rather than local timezone in
  output and filenames.

- Add `--milliseconds` to print timestamps with millisecond resolution.

- Add `--raw-frames` option to display payload of AVLC frames in raw hex for
  debugging purposes.

- Add `--dump-asn1` option to display full ASN.1 structure dumps of CPDLC and CM
  messages.

- Some ACARS and MIAM CORE messages contain XML data. Use `--prettify-xml`
  option to enable pretty-printing of such content. XML will then be reformatted
  with proper indentation for easier reading. This feature requires libacars
  built with libxml2 library support - otherwise this option has no effect.

## Enriching messages with ground station data

VDL2 messages formatted as text are normally logged like this:

```
[2020-01-10 00:02:40 CET] [136.775] [-31.8/-51.6 dBFS] [19.8 dB] [-1.2 ppm]
06A0B7 (Aircraft, Airborne) -> 29E0C5 (Ground station): Response
AVLC type: S (Receive Ready) P/F: 0 rseq: 2
```

dumpvdl2 can optionally print more information about ground stations using data
read from a text file. Each line in the file should have the following format:

```
hex_address [airport_icao_code ground_station_details] [ground_station_location]
```

Example:

```
29E0C5 [EDDB Berlin Schonefeld DE] [Berlin Schonefeld]
```

Add the following option to dumpvdl2 command line:

```
--gs-file /path/to/ground_station_file.txt
```

Provide the correct path to the file, of course.

Verbosity can be controlled with `--addrinfo` option, which takes three values:

`--addrinfo normal` (the default):

```
[2020-01-10 00:02:40 CET] [136.775] [-31.8/-51.6 dBFS] [19.8 dB] [-1.2 ppm]
06A0B7 (Aircraft, Airborne) -> 29E0C5 (Ground station): Response
GS info: EDDB, Berlin Schonefeld
AVLC type: S (Receive Ready) P/F: 0 rseq: 2
```

`--addrinfo terse`:

```
[2020-01-10 00:02:40 CET] [136.775] [-31.8/-51.6 dBFS] [19.8 dB] [-1.2 ppm]
06A0B7 (Aircraft, Airborne) -> 29E0C5 (Ground station) [EDDB]: Response
AVLC type: S (Receive Ready) P/F: 0 rseq: 2
```

`--addrinfo verbose`:

```
[2020-01-10 00:02:40 CET] [136.775] [-31.8/-51.6 dBFS] [19.8 dB] [-1.2 ppm]
06A0B7 (Aircraft, Airborne) -> 29E0C5 (Ground station): Response
GS info: EDDB Berlin Schonefeld DE
AVLC type: S (Receive Ready) P/F: 0 rseq: 2
```

dumpvdl2 reads the whole ground station data file on startup and caches it in
memory. Whenever you make changes to the file, you have to restart the program
in order for the changes to take effect.

## Enriching messages with aircraft data

If compiled with SQLite3 support, dumpvdl2 can read aircraft data from SQLite3
database in a well-known Basestation format used in various plane tracking
applications. Such data can be printed along the header of each message. Use
`--bs-db /path/to/basestation.sqb` option to enable the feature. `--addrinfo`
controls aircraft data verbosity in the same way as for ground stations (see
above). Example with `--addrinfo` set to `normal`:

```
[2020-01-10 00:02:40 CET] [136.775] [-31.8/-51.6 dBFS] [19.8 dB] [-1.2 ppm]
06A0B7 (Aircraft, Airborne) -> 29E0C5 (Ground station): Response
AC info: A7-BCS, B788, QTR
GS info: EDDB, Berlin Schonefeld
AVLC type: S (Receive Ready) P/F: 0 rseq: 2
```

dumpvdl2 reads data from `Aircraft` table. The following fields are used:

- `--addrinfo terse`: Registration
- `--addrinfo normal`: Registration, ICAOTypeCode, OperatorFlagCode
- `--addrinfo verbose`: Registration, Manufacturer, Type, RegisteredOwners

ICAO hex code is read from ModeS field. All fields are expected to have a data
type `varchar`. ModeS field must be unique and non-NULL. Other fields are
allowed to be NULL (the program will substitute each NULL value with a dash).

Entries from the database are read on the fly, when needed. They are cached in
memory for 30 minutes and then re-read from the database or purged.

## Decoding upper-level protocols in fragmented packets

ACARS messages, MIAM file transfers and X.25 packets are limited in size.
Whenever there is a need to send a larger message than the protocol allows, it
must be split into smaller parts (fragments). dumpvdl2 will automatically
reassemble such messages. Individual fragments will be logged too, as they
arrive. Only when all parts of the message are successfully received, dumpvdl2
will reassemble the message and log its payload in one piece.

ACARS messages, MIAM file transfers and X.25 packets often contain binary data
of higher level protocols or applications, which dumpvdl2 can also decode (for
example: CPDLC, ADS-C, IDRP or MIAM CORE). Such protocols will only decode
correctly when a complete payload is presented to the decoder. If the message is
fragmented, there is no point in decoding higher level protocol in individual
fragments, because this will often result in a partial decode with an
`-- Unparseable <protocol name> PDU` error printed to the log file. To reduce
log file cluttering, dumpvdl2 does not decode higher level protocols in
fragmented packets. Data contained in individual fragments will be printed in
hex, but this does not mean the packet type is unknown - it's just not decodable
yet. When all the fragments have been received and correctly reassembled, the
packet will be decoded and logged in full.

Before version 1.8.0, dumpvdl2 always attempted to decode higher level
protocols, regardless of whether the packet was a fragment of a larger packet or
not. You can enable the old behaviour by adding `--decode-fragments` command
line option.

## Integration with Planeplotter

dumpvdl2 can send ACARS messages to Planeplotter, which in turn can extract
aircraft position information from them and display blips on the map. First,
configure your Planeplotter as follows:

- Stop data processing (press 'Stop' button on the toolbar)

- Go to Options / I/O Settings...

- Tick 'UDP/IP Data from net'

- Set 'UDP/IP local port' to some value (default is 9742)

- Close the settings window by clicking OK and restart data processing

Supply dumpvdl2 with the address (or host name) and port where the Planeplotter
is listening:

```
./dumpvdl2 --output decoded:pp_acars:udp:address=10.10.10.12,port=9742 [other_options]
```

That's all. Switch to 'Message view' in Planeplotter and look for incoming
messages.

## Message filtering

By default dumpvdl2 logs all decoded messages. You can use `--msg-filter` option
to ignore things you don't want to see. If you do not want messages sent by
ground stations, run the program like this:

```
./dumpvdl2 --msg-filter all,-uplink [other_options]
```

Or if you want to filter out empty ACARS messages, because they are boring, use
this:

```
./dumpvdl2 --msg-filter all,-acars_nodata [other_options]
```

For full list of supported filtering options, run:

```
./dumpvdl2 --msg-filter help
```

Refer to `doc/FILTERING_EXAMPLES.md` file for more examples and details.

## Debugging output

If the program has been compiled with `-DCMAKE_BUILD_TYPE=Debug`, there is
`--debug` option available. It controls debug message classes which should (or
should not) be printed to standard error. This works in the same way as message
filters described above. Run the program with `--debug help` to list all debug
message classes available.

## Statistics

The program does not calculate statistics by itself. Instead, it sends metric
values (mostly counters) to the external collector using Etsy StatsD protocol.
It's the collector's job to receive, aggregate, store and graph them. Some
examples of software which can be used for this purpose:

- [Collectd](https://collectd.org/) is a statistics collection daemon which
  supports a lot of metric sources by using various [plugins](https://collectd.org/wiki/index.php/Table_of_Plugins).
  It has a [StatsD](https://collectd.org/wiki/index.php/Plugin:StatsD) plugin which can
  receive statistics emitted by dumpvdl2, aggregate them and write to various
  time-series databases like RRD, Graphite, MongoDB or TSDB.

- [Graphite](https://graphiteapp.org/) is a time-series database with powerful
  analytics and aggregation functions. Its graphing engine is quite basic,
  though.

- [Grafana](http://grafana.org/) is a sophisticated and elegant graphing
  solution supporting a variety of data sources.

Here is an example of some dumpvdl2 metrics being graphed by Grafana:

![Statistics](statistics.png?raw=true)

Metrics are quite handy when tuning the antenna installation or receiver
parameters (like gain or correction).

To enable statistics just give dumpvdl2 your StatsD collector's hostname (or IP
address) and UDP port number, for example:

```
./dumpvdl2 --statsd 10.10.10.15:1234 [other_options]
```

## Processing recorded IQ data from file

The syntax is:

```
dumpvdl2 --iq-file <file_name> [--sample-format <sample_format>] [--oversample <oversample_rate>]
  [--centerfreq <center_frequency>] [vdl_freq_1] [vdl_freq_2] [...]
```

The symbol rate for VDL2 is 10500 symbols/sec. dumpvdl2 internal processing rate
is 10 samples per symbol. Therefore the file must be recorded with sampling rate
set to an integer multiple of 105000. Specify the multiplier value with
`--oversample` option. The default value is 10, which is valid for files sampled
as 1050000 samples/sec. For example, if you have recorded your file at 2100000
samples/sec, then use `--oversample 20` (because 105000 * 20 = 2100000).

The program accepts raw data files without any header. Files produced by
`rtl_sdr` and `miri_sdr` programs are perfectly valid input files. Different
radios produce samples in different formats, though. dumpvdl2 currently supports
following sample formats:

- `U8` - unsigned 8-bit samples. This is the format produced by `rtl_sdr`
  utility.
- `S16_LE` - 16-bit signed, little endian. Produced by `miri_sdr` utility (by
  default).

Use `--sample-format` option to set the format. The default format is `U8`.

The program assumes that the VDL2 channel is located at baseband (0 Hz), ie. the
center frequency of your radio was set to the VDL2 channel frequency during
recording. If this is not the case, you have to provide correct center frequency
and channel frequency. For example, if your receiver was tuned to 136.955 MHz
during recording and you want to decode the VDL2 channel located at 136.975 MHz,
then use this:

```
dumpvdl2 --iq-file <file_name> --centerfreq 136955000 136975000
```

Putting it all together:

```
dumpvdl2 --iq-file iq.dat --sample-format S16_LE --oversample 13 --centerfreq 136955000 136975000 136725000
```

processes `iq.dat` file recorded at 1365000 samples/sec using 16-bit signed
samples, with receiver center frequency set to 136.955 MHz. VDL2 channels
located at 136.975 and 136.725 MHz will be decoded.

## Decoding raw AVLC frames from a binary file

Raw AVLC frames saved in a file with:

```
dumpvdl2 --output raw:binary:file:path=/some/dir/file.raw [...]
```

can be decoded anytime later with:

```
dumpvdl2 --raw-frames-file /some/dir/file.raw --output [...]
```

As there is no demodulation done in this case, there is no need to specify
any radio-related options, like `--centerfreq` or channel frequencies.

## Launching dumpvdl2 in background on system boot

There is an example systemd unit file in `etc` subdirectory (which means you
need a systemd-based distribution, like Debian/Raspbian Jessie or newer).

First, go to dumpvdl2 source directory and install the binary to `/usr/local/bin`:

```
sudo make install
```

Copy the unit file to the systemd unit directory:

```
sudo cp etc/dumpvdl2.service /etc/systemd/system/
```

Copy the example environment file to `/etc/default` directory:

```
sudo cp etc/dumpvdl2 /etc/default/
```

Edit `/etc/default/dumpvdl2` with a text editor (eg. nano). Uncomment the
`DUMPVDL2_OPTIONS=` line and put your preferred dumpvdl2 option set there.
Example:

```
DUMPVDL2_OPTIONS="--rtlsdr 0 --gain 39 --correction 0 --output decoded:text:file:path=/home/pi/vdl2.log,rotate=daily 136975000 136875000 136775000"
```

Reload systemd configuration:

```
sudo systemctl daemon-reload
```

Start the service:

```
sudo systemctl start dumpvdl2
```

Verify if it's running:

```
systemctl status dumpvdl2
```

It should show: `Active: active (running) since <date>`. If it failed, it might
be due to an error in the `DUMPVDL2_OPTIONS` value. Read the log messages in the
status output and fix the problem.

If everything works fine, enable the service, so that systemd starts it
automatically at boot:

```
systemctl enable dumpvdl2
```

## Extras

There are a few additions to the program in the `extras` directory in the source
tree. Refer to the README.md file in that directory for the current list of
extras and their purpose.

## Frequently Asked Questions

### What is VDL Mode 2?

VDL (VHF Data Link) Mode 2 is a communication protocol between aircraft and a
network of ground stations. It has a higher capacity than ACARS and a lot more
applications. More information can be found on [Wikipedia](https://en.wikipedia.org/wiki/VHF_Data_Link)
or [SigIdWiki](http://www.sigidwiki.com/wiki/VHF_Data_Link_-_Mode_2_(VDL-M2)).

### Who uses it?

Large transport aircraft operators - civil airlines and some military.

### What frequencies it runs on?

The most ubiquitous is 136.975 MHz (so called Common Signalling Channel). In
some areas where the capacity of a single channel is not enough, 136.725,
136.775 or 136.875 is used as well.  Because they are closely spaced, dumpvdl2
can receive all of them simultaneously with a single receiver.

### Is it used in my area?

It's quite probable. Launch your favorite SDR Console (like SDRSharp or
GQRX), tune 136.975 MHz and place your antenna outside (or near the window, at
least). If you see short bursts every now and then, it's there.

### What antenna shall I use?

VDL2 runs on VHF airband, so if you already have a dedicated antenna for ACARS
or airband voice, it will be perfect for VDL2. However VDL2 transmissions are
not very powerful, so do not expect thousands of messages per hour, if your
antenna is located indoors. If you have already played with ADS-B, you know,
what to do - put the antenna outside and high with unobstructed sky view, use
short and good quality feeder cable, shield your radio from external RF
interference.

### Two hours straight and zero messages received. What's wrong?

It basically comes down to three things:

#### The signal has to be strong enough (preferably 15 dB over noise floor, or better)

- set your tuner gain quite high. I get good results with 40 dB for RTLSDR and
  75 dB for Mirics dongles. Do not be tempted to crank the gain up to the max.
  Keep your noise floor low because higher noise yields higher bit error rate and
  may cause signal clipping when the transmission is strong (eg. the transmitting
  aircraft is just overflying your antenna).  On SDRPlay it should be good enough
  to use auto gain control.

- observe the waterfall in your favorite SDR console app, using the same gain
  setting - do you see data bursts clearly?  (they are very short, like pops).

- if your DC spike is very high, set the center frequency manually to dodge it
  (use `--centerfreq` option).

- RTL dongles are cheap - some of them have higher noise figure than others. If
  you have several dongles at hand, just try another one.

#### Channel frequency must be correct

- initially, just don't set it manually, use the default of 136.975 MHz. It is
  used everywhere where VDL2 is available.

#### PPM correction setting must be (more or less) accurate

- oscillators in cheap receivers are not 100% accurate. It is usually necessary
  to introduce manual correction to get precise tuning. There is no
  one-size-fits-all correction value - it is receiver-specific. See next question.

### How do I estimate PPM correction value for my dongle?

**Method 1:** use `rtl_test` utility which comes with `librtlsdr` library. Run
it with `-p` option and observe the output:

```
root@linux:~ # rtl_test -p
Found 1 device(s):
  0:  Realtek, RTL2838UHIDIR, SN: 00000002

Using device 0: Generic RTL2832U OEM
Found Rafael Micro R820T tuner
Supported gain values (29): 0.0 0.9 1.4 2.7 3.7 7.7 8.7 12.5 14.4 15.7 16.6 19.7 20.7 22.9 25.4 28.0 29.7 32.8 33.8 36.4 37.2 38.6 40.2 42.1 43.4 43.9 44.5 48.0 49.6
[R82XX] PLL not locked!
Sampling at 2048000 S/s.
Reporting PPM error measurement every 10 seconds...
Press ^C after a few minutes.
Reading samples in async mode...
real sample rate: 2048207 current PPM: 101 cumulative PPM: 101
real sample rate: 2048159 current PPM: 78 cumulative PPM: 89
real sample rate: 2048137 current PPM: 67 cumulative PPM: 81
real sample rate: 2048184 current PPM: 90 cumulative PPM: 84
real sample rate: 2048163 current PPM: 80 cumulative PPM: 83
real sample rate: 2048165 current PPM: 81 cumulative PPM: 82
real sample rate: 2048140 current PPM: 69 cumulative PPM: 81
real sample rate: 2048178 current PPM: 87 cumulative PPM: 81
real sample rate: 2048168 current PPM: 82 cumulative PPM: 81
real sample rate: 2048117 current PPM: 57 cumulative PPM: 79
real sample rate: 2048202 current PPM: 99 cumulative PPM: 81
real sample rate: 2048173 current PPM: 85 cumulative PPM: 81
real sample rate: 2048164 current PPM: 80 cumulative PPM: 81
real sample rate: 2048135 current PPM: 66 cumulative PPM: 80
real sample rate: 2048179 current PPM: 88 cumulative PPM: 80
real sample rate: 2048170 current PPM: 83 cumulative PPM: 81
real sample rate: 2048167 current PPM: 82 cumulative PPM: 81
real sample rate: 2048155 current PPM: 76 cumulative PPM: 80
real sample rate: 2048160 current PPM: 78 cumulative PPM: 80
real sample rate: 2048159 current PPM: 78 cumulative PPM: 80
real sample rate: 2048154 current PPM: 75 cumulative PPM: 80
real sample rate: 2048155 current PPM: 76 cumulative PPM: 80
real sample rate: 2048181 current PPM: 89 cumulative PPM: 80
```

After a couple of minutes the cumulative PPM value converges to a stable
reading. This is an approximate correction value for your dongle. Run dumpvdl2
with `--correction <value>` option. dumpvdl2 can compensate correction errors up
to a certain amount. Once you have received some messages, look for the
frequency offset field which is printed in the header of each message (it's the
value expressed in ppm). Your tuning is good, when this value is close to 0.  If
you see a systematic offset from 0, tweak your correction value to compensate
it.

### What do these numbers in the message header mean?

```
[2017-02-26 19:18:00 GMT] [136.975] [-18.9/-43.9 dBFS] [25.0 dB] [0.4 ppm]
```

From left to right:

- date and time with timezone.

- channel frequency on which the message has been received.

- signal power level (averaged over all symbol sampling points in the burst).
  Full scale is 0 dB.

- noise floor power level. Full scale is 0 dB.

- signal to noise ratio (ie. signal power level minus noise floor power level).

- frequency offset of the received burst from the channel center frequency, in
  parts per million.

There is an `--extended-header` command line option which enables additional
fields:

```
[2017-02-26 19:18:00 GMT] [136.975] [-18.9/-43.9 dBFS] [25.0 dB] [0.4 ppm] [S:0] [L:34] [F:0] [#0]
```

- number of bit errors corrected in the VDL2 burst header (up to 2).

- burst length in octets.

- number of octets corrected by Reed-Solomon FEC.

- number of frame in this particular transmission. Multiple AVLC frames
  (messages) may be concatenated and sent as a single transmission burst. When a
  multiframe burst is received, frames will be numbered incrementally.

### I want colorized logs, like on the screenshot.

dumpvdl2 does not have log colorization feature. But there is an app named
[MultiTail](https://www.vanheusden.com/multitail/) which you can use to follow dumpvdl2 log file in real time, as it grows,
with optional colorization. It's just a matter of writing a proper colorization
scheme (it tells the program what words or phrases to colorize and what color to
use). Refer to `multitail-dumpvdl2.conf` file in the `extras` subdirectory for
an example. To use it:

- Install the program:

```
sudo apt install multitail
```

- Copy the example colorization scheme to `/etc`:

```
sudo cp extras/multitail-dumpvdl2.conf /etc
```

- Include the color scheme into the main MultiTail configuration file:

```
sudo echo "include:/etc/multitail-dumpvdl2.conf" >> /etc/multitail.conf
```

- You can only colorize the file while it grows.  So assuming that dumpvdl2 is
  running and writing its log into `vdl2.log` file in the current directory,
  type the following:

```
multitail -cS dumpvdl2 vdl2.log
```

`-cS dumpvdl2` option select the color scheme named `dumpvdl2`.

### Can I concatenate several raw binary files into one?

Yes. There is no header in the file, just data. Concatenated file should
therefore decode correctly.

### I want to extract raw data from a raw binary file. Where is the format specification?

Here it is:

```
<length><frame_data><length><frame_data>...
```

where:

- `<length>` is an unsigned 16-bit value in network order (ie. most significant
  byte first). It indicates the length of the following `<frame_data>` block
  plus the length of the length field itself. So the length of `00 5b` indicates
  that the following `<frame_data>` block is 89 bytes long (2+89 = 91 = 0x5b).

- `<frame_data>` is a structure containing raw AVLC frame octets and its
  metadata, encoded as a Google protocol buffer. Refer to the
  `proto/dumpvdl2.proto` file for the specification of the structure.

You can learn how to deal with protocol buffers from [here](https://developers.google.com/protocol-buffers/docs/overview).

### How to receive data from dumpvdl2 using ZeroMQ sockets?

Here is how to do it in Python, assuming that you are running Raspbian Buster or later.

First, install python3-zmq:

```
apt install python3-zmq
```

**Scenario 1:** dumpvdl2 works as a client, Python script is a server:

```python
import zmq, sys
context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.bind(sys.argv[1])
socket.setsockopt_string(zmq.SUBSCRIBE, '')
while True:
    string = socket.recv_string()
    print(string)
```

Save the script in a file (for example, zmqserver.py) and run it like so:

```
python3 zmqserver.py tcp://*:5555
```

Assuming that the above script is running on a machine with an IP address of
10.10.10.1, you can then run dumpvdl2 with `zmq` output set to client mode like
this:

```
dumpvdl2 --output decoded:text:zmq:mode=client,endpoint=tcp://10.10.10.1:5555 [...]
```

**Scenario 2:** dumpvdl2 works as a server, Python script is a client:

```python
import zmq,sys
context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect(sys.argv[1])
socket.setsockopt_string(zmq.SUBSCRIBE, '')
while True:
    string = socket.recv_string()
    print(string)
```

So the only difference is that now the script calls `socket.connect()` instead
of `socket.bind()`. Assuming that dumpvdl2 will be run on a machine with an IP
address of 10.10.10.2, save the script as `zmqclient.py` and run it as follows:

```
python3 zmqclient.py tcp://10.10.10.2:5555
```

Then start dumpvdl2 in ZeroMQ server mode:

```
dumpvdl2 --output decoded:text:zmq:mode=server,endpoint=tcp://*:5555
```

Both scripts print arriving messages to standard output.

The advantage of the second scenario is that dumpvdl2 can serve multiple clients
using just a single `zmq` output. However the first scenario may come in handy
if dumpvdl2 is running behind a firewall which does not permit connections from
the outside. In this case if the output is to be sent to multiple consumers,
each one must be configured as a separate `zmq` output.

### I collect data in JSON format over network from several receivers. Is there a way to determine which receiver each frame came from?

Use `--station-id <name>` option to set the name of the receiver. This name will
be put into `station` attribute at top level of every JSON-formatted message.

### Can you add support for [*my favourite SDR receiver type*]?

Maybe. However do not expect me to purchase all SDRs available on the market
just to make dumpvdl2 work with them. If your life absolutely depends on it,
consider donating, or at least lending me the hardware for some time for
development and testing.

Alternatively, if you can write code, you may do the work by yourself and submit
it as a pull request. Most of the program code is hardware-agnostic anyway.
Adding new device type basically comes down to the following:

- `dumpvdl2.c`, `dumpvdl2.h` - add new input type and necessary command line
  options.

- `rtl.c`, `rtl.h` - this is the code specific to the RTLSDR hardware. Make a
  copy and modify it to use the API of your SDR device. Or you can start off
  from `mirics.c` and `mirics.h`, if you prefer.

- `demod.c` - if your SDR device uses a sample format other than 8-bit unsigned
  and 16-bit signed, it is necessary to write a routine which handles this
  format and converts the samples to signed float in the <-1;1> range. Refer to
  `process_buf_uchar()` and `process_buf_short()` routines for details.

- `CMakeLists.txt` - copy the section containing `find_package(RTLSDR)` and
  modify it, so that it finds all the necessary libraries and header file
  locations and appends them to relevant build variables. Make sure that the
  program still builds correctly when the library for your new SDR type is
  not installed or has been disabled by the user. Add the appropriate
  information to the configuration summary which is printed at the end.

### Can you add support for Windows?

Maybe. I may do it one day, but it's not currently top priority.

## Credits and thanks

I hereby express my gratitude to everybody who helped with the development and
testing of dumpvdl2. Special thanks go to:

- Fabrice Crohas
- Dick van Noort
- acarslogger
- Piotr Herko, SP5XSB
- LamaBleu

## License

Copyright (c) 2017-2020 Tomasz Lemiech <szpajder@gmail.com>

Contains code from the following software projects:

- libfec, (c) 2006 by Phil Karn, KA9Q

- Rocksoft^tm Model CRC Algorithm Table Generation Program V1.0
  by Ross Williams

- DarwinPthreadBarrier, (c) 2015, Aleksey Demakov

- librtlsdr-keenerd, (c) 2013-2014 by Kyle Keen

- asn1c, (c) 2003-2017 by Lev Walkin and contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

// vim: textwidth=80
