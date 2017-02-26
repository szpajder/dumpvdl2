# Message filtering in dumpvdl2

Message filtering in `dumpvdl2` is driven by the command line option:

        --msg-filter <filter_spec>

`<filter_spec>` is a comma-separated list of words specifying message types which should
be displayed. Each word may optionally be preceded by a `-` sign to negate its meaning
(ie. to indicate that a particular message type shall not be displayed).

Complete and current list of supported message types can be displayed with:

        dumpvdl2 --msg-filter help

When `--msg-filter` option is not used, all messages are displayed. But when it is,
the filter is first reset to empty, ie. you have to explicitly enable all message
types that you wish to see. Word list is parsed from left to right, so the last match
wins.

Examples:

- Show all messages:

        --msg-filter all

- Show no messages (negation of `all`):

        --msg-filter -all

### How to disable a subset of messages

- Display all messages except these from ground stations (only messages from aircraft
are displayed as a result):

        --msg-filter all,-uplink

- Display everything except empty ACARS messages, X.25 Control packets (Call Request,
Call Accepted, Clear Request, Clear Confirm, etc) and IDRP Keepalive PDUs:

        --msg-filter all,-acars-nodata,-x25_control,-idrp_keepalive

- Filter out pretty much all boring stuff:

        --msg-filter all,-avlc_s,-acars_nodata,-gsif,-x25_control,-idrp_keepalive,-esis

### How to enable only a particular subset of messages

Things get tricky here, because:

- you have to always supply at least one of `uplink` or `downlink` filters - otherwise
  both of these message classes will be denied, so no messages will be shown.

- you have to enable not only the message type you are interested in, but also all lower
  level message types which your favorite message is encapsulated in.

Examples:

- Display ACARS downlink messages

First, observe how such a message is structured:

        [2017-02-26 19:48:23] [136.975] [-29.9/-46.0 dBFS] [16.1 dB]
        4B17DF (Aircraft, Airborne) -> 10920A (Ground station): Command
        AVLC type: I sseq: 1 rseq: 2 poll: 0
        ACARS:
        Reg: .HB-JBB Flight: LX135Y
        Mode: 2 Label: H1 Blk id: 5 Ack: ! Msg no.: C58A
        Message:
        #CFB                          Page #1
        REPORT: Fault Messages (Condensed)
        REPORT DATE/TIME: 28-Dec-2016 17:49
        AIRCRAFT TAIL NUMBER: ---------
        -----------------------------------
         
        4951F0006      28Dec2016

It is encapsulated in AVLC frame of type I (Information frame). Hence, it is not enough
to say:

        --msg-filter downlink,acars    (WRONG!)

The correct way is:

        --msg-filter downlink,avlc_i,acars

- Show only ES-IS hello PDUs:

These may come in two types. They might be transmitted in X.25 Data packets, like this:

        [2017-02-26 19:52:11] [136.975] [-30.7/-47.0 dBFS] [16.2 dB]
        10920A (Ground station, On ground) -> 489C05 (Aircraft): Command
        AVLC type: I sseq: 5 rseq: 2 poll: 0
        X.25 Data: grp: 11 chan: 255 sseq: 3 rseq: 0 more: 0
        ES-IS IS Hello: Hold Time: 30 sec
         NET: 47 00 27 01 58 41 41 00 00 00 02 00 93 02 00 ac 13 93 c6 00       "G.'.XAA............."

or they may be piggybacked to X.25 Control packets, like Call Request or Call Accepted. This
feature is called "Fast Select":

        [2017-02-26 19:52:11] [136.975] [-22.6/-46.9 dBFS] [24.3 dB]
        4C4C47 (Aircraft, Airborne) -> 10920A (Ground station): Command
        AVLC type: I sseq: 4 rseq: 6 poll: 0
        X.25 Call Request: grp: 11 chan: 255 src: 23046107 dst: none
        Facilities:
         Fast Select: 80
         Packet size: 09 09
         Window size: 07 07
         Marker (non-X.25 facilities follow): 0f
         Called address extension: 8c 58 41 41 02 00 93
        Compression support: LREF
        ES-IS IS Hello: Hold Time: 65534 sec
         NET: 47 00 27 41 4c 4f 54 00 48 95 27 00 00 00 00 00 00 00 00 00       "G.'ALOT.H.'........."
         Options:
         (Unknown code 0x88): 01

Both of these are in turn encapsulated in AVLC information frames. To catch both of these,
the following filter is required:

        --msg-filter uplink,downlink,avlc_i,x25_control,x25_data,esis

However you may notice that some extraneous message types have sneaked in:

        [2017-02-26 19:55:49] [136.975] [-29.8/-47.3 dBFS] [17.5 dB]
        4B17DF (Aircraft, Airborne) -> 10920A (Ground station): Command
        AVLC type: I sseq: 0 rseq: 2 poll: 0
        X.25 Receive Ready: grp: 11 chan: 255

        [2017-02-26 19:57:01] [136.975] [-22.4/-47.5 dBFS] [25.0 dB]
        4C4C47 (Aircraft, Airborne) -> 10920A (Ground station): Command
        AVLC type: I sseq: 3 rseq: 5 poll: 0
        X.25 Clear Request: grp: 11 chan: 255
        Cause: 00
        Diagnostic code: 00

This is because there is currently no filter word which explicitly catches these, so they
are all caught by the more generic `x25_control` filter. This is a limitation of the current
filtering architecture. It might be fixed in the future. For now, the `--msg-filter` option
shall preferably be used to disable a particular subset of messages rather than disabling
everything and showing only a specific subset.
