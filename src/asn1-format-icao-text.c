/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2020 Tomasz Lemiech <szpajder@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gmodule.h>                            // GByteArray
#include <libacars/dict.h>                      // la_dict
#include <libacars/vstring.h>                   // la_vstring, LA_ISPRINTF()
#include "asn1/ACSE-apdu.h"                     // asn_DEF_ACSE_apdu
#include "asn1/ABRT-source.h"                   // ABRT_source_*
#include "asn1/ATCDownlinkMessage.h"            // ATCDownlinkMessage_t and dependencies
#include "asn1/ATCUplinkMessage.h"              // ATCUplinkMessage_t and dependencies
#include "asn1/Associate-result.h"              // Associate_result_*
#include "asn1/CMAircraftMessage.h"             // asn_DEF_AircraftMessage
#include "asn1/CMContactRequest.h"              // asn_DEF_CMContactRequest
#include "asn1/CMGroundMessage.h"               // asn_DEF_CMGroundMessage
#include "asn1/ProtectedAircraftPDUs.h"         // asn_DEF_ProtectedAircraftPDUs
#include "asn1/ProtectedGroundPDUs.h"           // asn_DEF_ProtectedGroundPDUs
#include "asn1/ADSAircraftPDU.h"                // asn_DEF_ADSAircraftPDU
#include "asn1/ADSAircraftPDUs.h"               // asn_DEF_ADSAircraftPDUs
#include "asn1/ADSAccept.h"                     // asn_DEF_ADSAccept
#include "asn1/ADSGroundPDU.h"                  // asn_DEF_ADSGroundPDU
#include "asn1/ADSGroundPDUs.h"                 // asn_DEF_ADSGroundPDUs
#include "asn1/ADSNonCompliance.h"              // asn_DEF_ADSNonCompliance
#include "asn1/ADSPositiveAcknowledgement.h"    // asn_DEF_ADSPositiveAcknowledgement
#include "asn1/ADSRequestContract.h"            // asn_DEF_ADSRequestContract
#include "asn1/ADSReject.h"                     // asn_DEF_ADSReject
#include "asn1/ADSReport.h"                     // asn_DEF_ADSReport
#include "asn1/Release-request-reason.h"        // Release_request_reason_*
#include "asn1/Release-response-reason.h"       // Release_response_reason_*
#include "dumpvdl2.h"                           // XCALLOC, dict_search()
#include <libacars/asn1-util.h>                 // la_asn1_formatter_func, la_asn1_output()
#include <libacars/asn1-format-common.h>        // common formatters and helper functions

// forward declarations
la_asn1_formatter const asn1_icao_formatter_table_text[];
size_t asn1_icao_formatter_table_text_len;
la_asn1_formatter const asn1_acse_formatter_table_text[];
size_t asn1_acse_formatter_table_text_len;

la_dict const Associate_result_labels[] = {
	{ .id = Associate_result_accepted, .val = "accept" },
	{ .id = Associate_result_rejected_permanent, .val = "reject (permanent)" },
	{ .id = Associate_result_rejected_transient, .val = "reject (transient)" },
	{ .id = 0, .val = NULL }
};

la_dict const Release_request_reason_labels[] = {
	{ .id = Release_request_reason_normal, .val = "normal" },
	{ .id = Release_request_reason_urgent, .val = "urgent" },
	{ .id = Release_request_reason_user_defined, .val = "user defined" },
	{ .id = 0, .val = NULL }
};

la_dict const Release_response_reason_labels[] = {
	{ .id = Release_response_reason_normal, .val = "normal" },
	{ .id = Release_response_reason_not_finished, .val = "not finished" },
	{ .id = Release_response_reason_user_defined, .val = "user defined" },
	{ .id = 0, .val = NULL }
};

la_dict const ABRT_source_labels[] = {
	{ .id = ABRT_source_acse_service_user, .val = "user" },
	{ .id = ABRT_source_acse_service_provider, .val = "provider" },
	{ .id = 0, .val = NULL }
};

la_dict const ATCUplinkMsgElementId_labels[] = {
	{ ATCUplinkMsgElementId_PR_uM0NULL, "UNABLE" },
	{ ATCUplinkMsgElementId_PR_uM1NULL, "STANDBY" },
	{ ATCUplinkMsgElementId_PR_uM2NULL, "REQUEST DEFERRED" },
	{ ATCUplinkMsgElementId_PR_uM3NULL, "ROGER" },
	{ ATCUplinkMsgElementId_PR_uM4NULL, "AFFIRM" },
	{ ATCUplinkMsgElementId_PR_uM5NULL, "NEGATIVE" },
	{ ATCUplinkMsgElementId_PR_uM6Level, "EXPECT [level]" },
	{ ATCUplinkMsgElementId_PR_uM7Time, "EXPECT CLIMB AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM8Position, "EXPECT CLIMB AT [position]" },
	{ ATCUplinkMsgElementId_PR_uM9Time, "EXPECT DESCENT AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM10Position, "EXPECT DESCENT AT [position]" },
	{ ATCUplinkMsgElementId_PR_uM11Time, "EXPECT CRUISE CLIMB AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM12Position, "EXPECT CRUISE CLIMB AT [position]" },
	{ ATCUplinkMsgElementId_PR_uM13TimeLevel, "AT [time] EXPECT CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM14PositionLevel, "AT [position] EXPECT CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM15TimeLevel, "AT [time] EXPECT DESCENT TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM16PositionLevel, "AT [position] EXPECT DESCENT TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM17TimeLevel, "AT [time] EXPECT CRUISE CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM18PositionLevel, "AT [position] EXPECT CRUISE CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM19Level, "MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM20Level, "CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM21TimeLevel, "AT [time] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM22PositionLevel, "AT [position] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM23Level, "DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM24TimeLevel, "AT [time] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM25PositionLevel, "AT [position] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM26LevelTime, "CLIMB TO REACH [level] BY [time]" },
	{ ATCUplinkMsgElementId_PR_uM27LevelPosition, "CLIMB TO REACH [level] BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM28LevelTime, "DESCEND TO REACH [level] BY [time]" },
	{ ATCUplinkMsgElementId_PR_uM29LevelPosition, "DESCEND TO REACH [level] BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM30LevelLevel, "MAINTAIN BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM31LevelLevel, "CLIMB TO AND MAINTAIN BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM32LevelLevel, "DESCEND TO AND MAINTAIN BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM33NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM34Level, "CRUISE CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM35Level, "CRUISE CLIMB ABOVE [level]" },
	{ ATCUplinkMsgElementId_PR_uM36Level, "EXPEDITE CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM37Level, "EXPEDITE DESCENT TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM38Level, "IMMEDIATELY CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM39Level, "IMMEDIATELY DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM40NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM41NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM42PositionLevel, "EXPECT TO CROSS [position] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM43PositionLevel, "EXPECT TO CROSS [position] AT OR ABOVE [level]" },
	{ ATCUplinkMsgElementId_PR_uM44PositionLevel, "EXPECT TO CROSS [position] AT OR BELOW [level]" },
	{ ATCUplinkMsgElementId_PR_uM45PositionLevel, "EXPECT TO CROSS [position] AT AND MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM46PositionLevel, "CROSS [position] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM47PositionLevel, "CROSS [position] AT OR ABOVE [level]" },
	{ ATCUplinkMsgElementId_PR_uM48PositionLevel, "CROSS [position] AT OR BELOW [level]" },
	{ ATCUplinkMsgElementId_PR_uM49PositionLevel, "CROSS [position] AT AND MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM50PositionLevelLevel, "CROSS [position] BETWEEN [level] AND [level]" },
	{ ATCUplinkMsgElementId_PR_uM51PositionTime, "CROSS [position] AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM52PositionTime, "CROSS [position] AT OR BEFORE [time]" },
	{ ATCUplinkMsgElementId_PR_uM53PositionTime, "CROSS [position] AT OR AFTER [time]" },
	{ ATCUplinkMsgElementId_PR_uM54PositionTimeTime, "CROSS [position] BETWEEN [time] AND [time]" },
	{ ATCUplinkMsgElementId_PR_uM55PositionSpeed, "CROSS [position] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM56PositionSpeed, "CROSS [position] AT OR LESS THAN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM57PositionSpeed, "CROSS [position] AT OR GREATER THAN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM58PositionTimeLevel, "CROSS [position] AT [time] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM59PositionTimeLevel, "CROSS [position] AT OR BEFORE [time] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM60PositionTimeLevel, "CROSS [position] AT OR AFTER [time] AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM61PositionLevelSpeed, "CROSS [position] AT AND MAINTAIN [level] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM62TimePositionLevel, "AT [time] CROSS [position] AT AND MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM63TimePositionLevelSpeed, "AT [time] CROSS [position] AT AND MAINTAIN [level] AT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM64DistanceSpecifiedDirection, "OFFSET [offset] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM65PositionDistanceSpecifiedDirection, "AT [position] OFFSET [offset] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM66TimeDistanceSpecifiedDirection, "AT [time] OFFSET [offset] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM67NULL, "PROCEED BACK ON ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM68Position, "REJOIN ROUTE BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM69Time, "REJOIN ROUTE BY [time]" },
	{ ATCUplinkMsgElementId_PR_uM70Position, "EXPECT BACK ON ROUTE BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM71Time, "EXPECT BACK ON ROUTE BY [time]" },
	{ ATCUplinkMsgElementId_PR_uM72NULL, "RESUME OWN NAVIGATION" },
	{ ATCUplinkMsgElementId_PR_uM73DepartureClearance, "[DepartureClearance]" },
	{ ATCUplinkMsgElementId_PR_uM74Position, "PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM75Position, "WHEN ABLE PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM76TimePosition, "AT [time] PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM77PositionPosition, "AT [position] PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM78LevelPosition, "AT [level] PROCEED DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM79PositionRouteClearance, "CLEARED TO [position] VIA [routeClearance]" },
	{ ATCUplinkMsgElementId_PR_uM80RouteClearance, "CLEARED [routeClearance]" },
	{ ATCUplinkMsgElementId_PR_uM81ProcedureName, "CLEARED [procedureName]" },
	{ ATCUplinkMsgElementId_PR_uM82DistanceSpecifiedDirection, "CLEARED TO DEVIATE UP TO [offset] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM83PositionRouteClearance, "AT [position] CLEARED [routeClearance]" },
	{ ATCUplinkMsgElementId_PR_uM84PositionProcedureName, "AT [position] CLEARED [procedureName]" },
	{ ATCUplinkMsgElementId_PR_uM85RouteClearance, "EXPECT [routeClearance]" },
	{ ATCUplinkMsgElementId_PR_uM86PositionRouteClearance, "AT [position] EXPECT [routeClearance]" },
	{ ATCUplinkMsgElementId_PR_uM87Position, "EXPECT DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM88PositionPosition, "AT [position] EXPECT DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM89TimePosition, "AT [time] EXPECT DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM90LevelPosition, "AT [level] EXPECT DIRECT TO [position]" },
	{ ATCUplinkMsgElementId_PR_uM91HoldClearance, "HOLD AT [position] MAINTAIN [level] INBOUND TRACK [degrees] [direction] TURNS [legtype]" },
	{ ATCUplinkMsgElementId_PR_uM92PositionLevel, "HOLD AT [position] AS PUBLISHED MAINTAIN [level]" },
	{ ATCUplinkMsgElementId_PR_uM93Time, "EXPECT FURTHER CLEARANCE AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM94DirectionDegrees, "TURN [direction] HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM95DirectionDegrees, "TURN [direction] GROUND TRACK [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM96NULL, "CONTINUE PRESENT HEADING" },
	{ ATCUplinkMsgElementId_PR_uM97PositionDegrees, "AT [position] FLY HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM98DirectionDegrees, "IMMEDIATELY TURN [direction] HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM99ProcedureName, "EXPECT [procedureName]" },
	{ ATCUplinkMsgElementId_PR_uM100TimeSpeed, "AT [time] EXPECT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM101PositionSpeed, "AT [position] EXPECT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM102LevelSpeed, "AT [level] EXPECT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM103TimeSpeedSpeed, "AT [time] EXPECT [speed] TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM104PositionSpeedSpeed, "AT [position] EXPECT [speed] TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM105LevelSpeedSpeed, "AT [level] EXPECT [speed] TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM106Speed, "MAINTAIN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM107NULL, "MAINTAIN PRESENT SPEED" },
	{ ATCUplinkMsgElementId_PR_uM108Speed, "MAINTAIN [speed] OR GREATER" },
	{ ATCUplinkMsgElementId_PR_uM109Speed, "MAINTAIN [speed] OR LESS" },
	{ ATCUplinkMsgElementId_PR_uM110SpeedSpeed, "MAINTAIN [speed] TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM111Speed, "INCREASE SPEED TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM112Speed, "INCREASE SPEED TO [speed] OR GREATER" },
	{ ATCUplinkMsgElementId_PR_uM113Speed, "REDUCE SPEED TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM114Speed, "REDUCE SPEED TO [speed] OR LESS" },
	{ ATCUplinkMsgElementId_PR_uM115Speed, "DO NOT EXCEED [speed]" },
	{ ATCUplinkMsgElementId_PR_uM116NULL, "RESUME NORMAL SPEED" },
	{ ATCUplinkMsgElementId_PR_uM117UnitNameFrequency, "CONTACT [unitname] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM118PositionUnitNameFrequency, "AT [position] CONTACT [unitname] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM119TimeUnitNameFrequency, "AT [time] CONTACT [unitname] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM120UnitNameFrequency, "MONITOR [unitname] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM121PositionUnitNameFrequency, "AT [position] MONITOR [unitname] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM122TimeUnitNameFrequency, "AT [time] MONITOR [unitname] [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM123Code, "SQUAWK [code]" },
	{ ATCUplinkMsgElementId_PR_uM124NULL, "STOP SQUAWK" },
	{ ATCUplinkMsgElementId_PR_uM125NULL, "SQUAWK MODE CHARLIE" },
	{ ATCUplinkMsgElementId_PR_uM126NULL, "STOP SQUAWK MODE CHARLIE" },
	{ ATCUplinkMsgElementId_PR_uM127NULL, "REPORT BACK ON ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM128Level, "REPORT LEAVING [level]" },
	{ ATCUplinkMsgElementId_PR_uM129Level, "REPORT MAINTAINING [level]" },
	{ ATCUplinkMsgElementId_PR_uM130Position, "REPORT PASSING [position]" },
	{ ATCUplinkMsgElementId_PR_uM131NULL, "REPORT REMAINING FUEL AND PERSONS ON BOARD" },
	{ ATCUplinkMsgElementId_PR_uM132NULL, "REPORT POSITION" },
	{ ATCUplinkMsgElementId_PR_uM133NULL, "REPORT PRESENT LEVEL" },
	{ ATCUplinkMsgElementId_PR_uM134SpeedTypeSpeedTypeSpeedType, "REPORT [speedtype] [speedtype] [speedtype] SPEED" },
	{ ATCUplinkMsgElementId_PR_uM135NULL, "CONFIRM ASSIGNED LEVEL" },
	{ ATCUplinkMsgElementId_PR_uM136NULL, "CONFIRM ASSIGNED SPEED" },
	{ ATCUplinkMsgElementId_PR_uM137NULL, "CONFIRM ASSIGNED ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM138NULL, "CONFIRM TIME OVER REPORTED WAYPOINT" },
	{ ATCUplinkMsgElementId_PR_uM139NULL, "CONFIRM REPORTED WAYPOINT" },
	{ ATCUplinkMsgElementId_PR_uM140NULL, "CONFIRM NEXT WAYPOINT" },
	{ ATCUplinkMsgElementId_PR_uM141NULL, "CONFIRM NEXT WAYPOINT ETA" },
	{ ATCUplinkMsgElementId_PR_uM142NULL, "CONFIRM ENSUING WAYPOINT" },
	{ ATCUplinkMsgElementId_PR_uM143NULL, "CONFIRM REQUEST" },
	{ ATCUplinkMsgElementId_PR_uM144NULL, "CONFIRM SQUAWK" },
	{ ATCUplinkMsgElementId_PR_uM145NULL, "REPORT HEADING" },
	{ ATCUplinkMsgElementId_PR_uM146NULL, "REPORT GROUND TRACK" },
	{ ATCUplinkMsgElementId_PR_uM147NULL, "REQUEST POSITION REPORT" },
	{ ATCUplinkMsgElementId_PR_uM148Level, "WHEN CAN YOU ACCEPT [level]" },
	{ ATCUplinkMsgElementId_PR_uM149LevelPosition, "CAN YOU ACCEPT [level] AT [position]" },
	{ ATCUplinkMsgElementId_PR_uM150LevelTime, "CAN YOU ACCEPT [level] AT [time]" },
	{ ATCUplinkMsgElementId_PR_uM151Speed, "WHEN CAN YOU ACCEPT [speed]" },
	{ ATCUplinkMsgElementId_PR_uM152DistanceSpecifiedDirection, "WHEN CAN YOU ACCEPT [offset] [direction] OFFSET" },
	{ ATCUplinkMsgElementId_PR_uM153Altimeter, "ALTIMETER [altimeter]" },
	{ ATCUplinkMsgElementId_PR_uM154NULL, "RADAR SERVICE TERMINATED" },
	{ ATCUplinkMsgElementId_PR_uM155Position, "RADAR CONTACT [position]" },
	{ ATCUplinkMsgElementId_PR_uM156NULL, "RADAR CONTACT LOST" },
	{ ATCUplinkMsgElementId_PR_uM157Frequency, "CHECK STUCK MICROPHONE [frequency]" },
	{ ATCUplinkMsgElementId_PR_uM158AtisCode, "ATIS [atiscode]" },
	{ ATCUplinkMsgElementId_PR_uM159ErrorInformation, "ERROR" },
	{ ATCUplinkMsgElementId_PR_uM160Facility, "NEXT DATA AUTHORITY [facility]" },
	{ ATCUplinkMsgElementId_PR_uM161NULL, "END SERVICE" },
	{ ATCUplinkMsgElementId_PR_uM162NULL, "SERVICE UNAVAILABLE" },
	{ ATCUplinkMsgElementId_PR_uM163FacilityDesignation, "[facilitydesignation]" },
	{ ATCUplinkMsgElementId_PR_uM164NULL, "WHEN READY" },
	{ ATCUplinkMsgElementId_PR_uM165NULL, "THEN" },
	{ ATCUplinkMsgElementId_PR_uM166TrafficType, "DUE TO [traffictype]TRAFFIC" },
	{ ATCUplinkMsgElementId_PR_uM167NULL, "DUE TO AIRSPACE RESTRICTION" },
	{ ATCUplinkMsgElementId_PR_uM168NULL, "DISREGARD" },
	{ ATCUplinkMsgElementId_PR_uM169FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM170FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM171VerticalRate, "CLIMB AT [verticalRate] MINIMUM" },
	{ ATCUplinkMsgElementId_PR_uM172VerticalRate, "CLIMB AT [verticalRate] MAXIMUM" },
	{ ATCUplinkMsgElementId_PR_uM173VerticalRate, "DESCEND AT [verticalRate] MINIMUM" },
	{ ATCUplinkMsgElementId_PR_uM174VerticalRate, "DESCEND AT [verticalRate] MAXIMUM" },
	{ ATCUplinkMsgElementId_PR_uM175Level, "REPORT REACHING [level]" },
	{ ATCUplinkMsgElementId_PR_uM176NULL, "MAINTAIN OWN SEPARATION AND VMC" },
	{ ATCUplinkMsgElementId_PR_uM177NULL, "AT PILOTS DISCRETION" },
	{ ATCUplinkMsgElementId_PR_uM178NULL, "Reserved" },
	{ ATCUplinkMsgElementId_PR_uM179NULL, "SQUAWK IDENT" },
	{ ATCUplinkMsgElementId_PR_uM180LevelLevel, "REPORT REACHING BLOCK [level] TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM181ToFromPosition, "REPORT DISTANCE [tofrom] [position]" },
	{ ATCUplinkMsgElementId_PR_uM182NULL, "CONFIRM ATIS CODE" },
	{ ATCUplinkMsgElementId_PR_uM183FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM184TimeToFromPosition, "AT [time] REPORT DISTANCE [tofrom] [position]" },
	{ ATCUplinkMsgElementId_PR_uM185PositionLevel, "AFTER PASSING [position] CLIMB TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM186PositionLevel, "AFTER PASSING [position] DESCEND TO [level]" },
	{ ATCUplinkMsgElementId_PR_uM187FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM188PositionSpeed, "AFTER PASSING [position] MAINTAIN [speed]" },
	{ ATCUplinkMsgElementId_PR_uM189Speed, "ADJUST SPEED TO [speed]" },
	{ ATCUplinkMsgElementId_PR_uM190Degrees, "FLY HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM191NULL, "ALL ATS TERMINATED" },
	{ ATCUplinkMsgElementId_PR_uM192LevelTime, "REACH [level] BY [time]" },
	{ ATCUplinkMsgElementId_PR_uM193NULL, "IDENTIFICATION LOST" },
	{ ATCUplinkMsgElementId_PR_uM194FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM195FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM196FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM197FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM198FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM199FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM200NULL, "REPORT REACHING" },
	{ ATCUplinkMsgElementId_PR_uM201NULL, "Not Used" },
	{ ATCUplinkMsgElementId_PR_uM202NULL, "Not Used" },
	{ ATCUplinkMsgElementId_PR_uM203FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM204FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM205FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM206FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM207FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM208FreeText, "FREE TEXT" },
	{ ATCUplinkMsgElementId_PR_uM209LevelPosition, "REACH [level] BY [position]" },
	{ ATCUplinkMsgElementId_PR_uM210Position, "IDENTIFIED [position]" },
	{ ATCUplinkMsgElementId_PR_uM211NULL, "REQUEST FORWARDED" },
	{ ATCUplinkMsgElementId_PR_uM212FacilityDesignationATISCode, "[facilitydesignation] ATIS [atiscode] CURRENT" },
	{ ATCUplinkMsgElementId_PR_uM213FacilityDesignationAltimeter, "[facilitydesignation] ALTIMETER [altimeter]" },
	{ ATCUplinkMsgElementId_PR_uM214RunwayRVR, "RVR RUNWAY [runway] [rvr]" },
	{ ATCUplinkMsgElementId_PR_uM215DirectionDegrees, "TURN [direction] [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM216NULL, "REQUEST FLIGHT PLAN" },
	{ ATCUplinkMsgElementId_PR_uM217NULL, "REPORT ARRIVAL" },
	{ ATCUplinkMsgElementId_PR_uM218NULL, "REQUEST ALREADY RECEIVED" },
	{ ATCUplinkMsgElementId_PR_uM219Level, "STOP CLIMB AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM220Level, "STOP DESCENT AT [level]" },
	{ ATCUplinkMsgElementId_PR_uM221Degrees, "STOP TURN HEADING [degrees]" },
	{ ATCUplinkMsgElementId_PR_uM222NULL, "NO SPEED RESTRICTION" },
	{ ATCUplinkMsgElementId_PR_uM223NULL, "REDUCE TO MINIMUM APPROACH SPEED" },
	{ ATCUplinkMsgElementId_PR_uM224NULL, "NO DELAY EXPECTED" },
	{ ATCUplinkMsgElementId_PR_uM225NULL, "DELAY NOT DETERMINED" },
	{ ATCUplinkMsgElementId_PR_uM226Time, "EXPECTED APPROACH TIME [time]" },
	{ ATCUplinkMsgElementId_PR_uM227NULL, "LOGICAL ACKNOWLEDGMENT" },
	{ ATCUplinkMsgElementId_PR_uM228Position, "REPORT ETA [position]" },
	{ ATCUplinkMsgElementId_PR_uM229NULL, "REPORT ALTERNATE AERODROME" },
	{ ATCUplinkMsgElementId_PR_uM230NULL, "IMMEDIATELY" },
	{ ATCUplinkMsgElementId_PR_uM231NULL, "STATE PREFERRED LEVEL" },
	{ ATCUplinkMsgElementId_PR_uM232NULL, "STATE TOP OF DESCENT" },
	{ ATCUplinkMsgElementId_PR_uM233NULL, "USE OF LOGICAL ACKNOWLEDGMENT PROHIBITED" },
	{ ATCUplinkMsgElementId_PR_uM234NULL, "FLIGHT PLAN NOT HELD" },
	{ ATCUplinkMsgElementId_PR_uM235NULL, "ROGER 7500" },
	{ ATCUplinkMsgElementId_PR_uM236NULL, "LEAVE CONTROLLED AIRSPACE" },
	{ ATCUplinkMsgElementId_PR_uM237NULL, "REQUEST AGAIN WITH NEXT UNIT" },
	{ 0, NULL }
};

la_dict const VerticalType_bit_labels[] = {
	{ 0, "top of climb" },
	{ 1, "top of descent" },
	{ 2, "start of climb" },
	{ 3, "start of descent" },
	{ 4, "start of level" },
	{ 5, "start of speed change" },
	{ 6, "end of speed change" },
	{ 7, "speed limit" },
	{ 8, "cross over" },
	{ 0, NULL }
};

la_dict const ReportTypeNotSupported_bit_labels[] = {
	{ 0, "projected profile" },
	{ 1, "ground vector" },
	{ 2, "air vector" },
	{ 3, "meteo info" },
	{ 4, "extended projected profile" },
	{ 5, "ToA range" },
	{ 6, "speed schedule profile" },
	{ 7, "RNP profile" },
	{ 8, "planned final approach speed" },
	{ 0, NULL }
};

la_dict const EPPLimitations_bit_labels[] = {
	{ 0, "requested distance tolerance not supported" },
	{ 1, "requested level tolerance not supported" },
	{ 2, "requested time tolerance not supported" },
	{ 3, "requested speed tolerance not supported" },
	{ 0, NULL }
};

la_dict const EventTypeNotSupported_bit_labels[] = {
	{ 0, "lateral deviations" },
	{ 1, "vertical rate deviations" },
	{ 2, "level range deviations" },
	{ 3, "way point changes" },
	{ 4, "air speed changes" },
	{ 5, "ground speed changes" },
	{ 6, "EPP flight plan changes" },
	{ 7, "EPP next waypoint in horizon" },
	{ 8, "EPP tolerance changes" },
	{ 9, "RTA status changes" },
	{ 10, "FoM changes" },
	{ 11, "level changes" },
	{ 12, "vertical clearance deviations" },
	{ 13, "airspeed range deviations" },
	{ 14, "turbulence deviations" },
	{ 15, "RNP not met" },
	{ 16, "planned final approach speed changes" },
	{ 0, NULL }
};

la_dict const EmergencyUrgencyStatus_bit_labels[] = {
	{ 0, "emergency" },
	{ 1, "reserved0" },
	{ 2, "unlawful-interference" },
	{ 3, "reserved1" },
	{ 4, "reserved2" },
	{ 5, "emergency-cancelled" },
	{ 0, NULL }

};

la_dict const ATCDownlinkMsgElementId_labels[] = {
	{ ATCDownlinkMsgElementId_PR_dM0NULL, "WILCO" },
	{ ATCDownlinkMsgElementId_PR_dM1NULL, "UNABLE" },
	{ ATCDownlinkMsgElementId_PR_dM2NULL, "STANDBY" },
	{ ATCDownlinkMsgElementId_PR_dM3NULL, "ROGER" },
	{ ATCDownlinkMsgElementId_PR_dM4NULL, "AFFIRM" },
	{ ATCDownlinkMsgElementId_PR_dM5NULL, "NEGATIVE" },
	{ ATCDownlinkMsgElementId_PR_dM6Level, "REQUEST [level]" },
	{ ATCDownlinkMsgElementId_PR_dM7LevelLevel, "REQUEST BLOCK [level] TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM8Level, "REQUEST CRUISE CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM9Level, "REQUEST CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM10Level, "REQUEST DESCENT TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM11PositionLevel, "AT [position] REQUEST CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM12PositionLevel, "AT [position] REQUEST DESCENT TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM13TimeLevel, "AT [time] REQUEST CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM14TimeLevel, "AT [time] REQUEST DESCENT TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM15DistanceSpecifiedDirection, "REQUEST OFFSET [offset] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM16PositionDistanceSpecifiedDirection, "AT [position] REQUEST OFFSET [offset] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM17TimeDistanceSpecifiedDirection, "AT [time] REQUEST OFFSET [offset] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM18Speed, "REQUEST [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM19SpeedSpeed, "REQUEST [speed] TO [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM20NULL, "REQUEST VOICE CONTACT" },
	{ ATCDownlinkMsgElementId_PR_dM21Frequency, "REQUEST VOICE CONTACT [frequency]" },
	{ ATCDownlinkMsgElementId_PR_dM22Position, "REQUEST DIRECT TO [position]" },
	{ ATCDownlinkMsgElementId_PR_dM23ProcedureName, "REQUEST [procedureName]" },
	{ ATCDownlinkMsgElementId_PR_dM24RouteClearance, "REQUEST CLEARANCE [routeClearance]" },
	{ ATCDownlinkMsgElementId_PR_dM25ClearanceType, "REQUEST [clearanceType] CLEARANCE" },
	{ ATCDownlinkMsgElementId_PR_dM26PositionRouteClearance, "REQUEST WEATHER DEVIATION TO [position] VIA [routeClearance]" },
	{ ATCDownlinkMsgElementId_PR_dM27DistanceSpecifiedDirection, "REQUEST WEATHER DEVIATION UP TO [offset] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM28Level, "LEAVING [level]" },
	{ ATCDownlinkMsgElementId_PR_dM29Level, "CLIMBING TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM30Level, "DESCENDING TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM31Position, "PASSING [position]" },
	{ ATCDownlinkMsgElementId_PR_dM32Level, "PRESENT LEVEL [level]" },
	{ ATCDownlinkMsgElementId_PR_dM33Position, "PRESENT POSITION [position]" },
	{ ATCDownlinkMsgElementId_PR_dM34Speed, "PRESENT SPEED [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM35Degrees, "PRESENT HEADING [degrees]" },
	{ ATCDownlinkMsgElementId_PR_dM36Degrees, "PRESENT GROUND TRACK [degrees]" },
	{ ATCDownlinkMsgElementId_PR_dM37Level, "MAINTAINING [level]" },
	{ ATCDownlinkMsgElementId_PR_dM38Level, "ASSIGNED LEVEL [level]" },
	{ ATCDownlinkMsgElementId_PR_dM39Speed, "ASSIGNED SPEED [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM40RouteClearance, "ASSIGNED ROUTE [routeClearance]" },
	{ ATCDownlinkMsgElementId_PR_dM41NULL, "BACK ON ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM42Position, "NEXT WAYPOINT [position]" },
	{ ATCDownlinkMsgElementId_PR_dM43Time, "NEXT WAYPOINT ETA [time]" },
	{ ATCDownlinkMsgElementId_PR_dM44Position, "ENSUING WAYPOINT [position]" },
	{ ATCDownlinkMsgElementId_PR_dM45Position, "REPORTED WAYPOINT [position]" },
	{ ATCDownlinkMsgElementId_PR_dM46Time, "REPORTED WAYPOINT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM47Code, "SQUAWKING [code]" },
	{ ATCDownlinkMsgElementId_PR_dM48PositionReport, "POSITION REPORT [positionreport]" },
	{ ATCDownlinkMsgElementId_PR_dM49Speed, "WHEN CAN WE EXPECT [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM50SpeedSpeed, "WHEN CAN WE EXPECT [speed] TO [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM51NULL, "WHEN CAN WE EXPECT BACK ON ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM52NULL, "WHEN CAN WE EXPECT LOWER LEVEL" },
	{ ATCDownlinkMsgElementId_PR_dM53NULL, "WHEN CAN WE EXPECT HIGHER LEVEL" },
	{ ATCDownlinkMsgElementId_PR_dM54Level, "WHEN CAN WE EXPECT CRUISE CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM55NULL, "PAN PAN PAN" },
	{ ATCDownlinkMsgElementId_PR_dM56NULL, "MAYDAY MAYDAY MAYDAY" },
	{ ATCDownlinkMsgElementId_PR_dM57RemainingFuelPersonsOnBoard, "[remainingFuel] OF FUEL REMAINING AND [personsonboard] PERSONS ON BOARD" },
	{ ATCDownlinkMsgElementId_PR_dM58NULL, "CANCEL EMERGENCY" },
	{ ATCDownlinkMsgElementId_PR_dM59PositionRouteClearance, "DIVERTING TO [position] VIA [routeClearance]" },
	{ ATCDownlinkMsgElementId_PR_dM60DistanceSpecifiedDirection, "OFFSETTING [offset] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM61Level, "DESCENDING TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM62ErrorInformation, "ERROR" },
	{ ATCDownlinkMsgElementId_PR_dM63NULL, "NOT CURRENT DATA AUTHORITY" },
	{ ATCDownlinkMsgElementId_PR_dM64FacilityDesignation, "[facilitydesignation]" },
	{ ATCDownlinkMsgElementId_PR_dM65NULL, "DUE TO WEATHER" },
	{ ATCDownlinkMsgElementId_PR_dM66NULL, "DUE TO AIRCRAFT PERFORMANCE" },
	{ ATCDownlinkMsgElementId_PR_dM67FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM68FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM69NULL, "REQUEST VMC DESCENT" },
	{ ATCDownlinkMsgElementId_PR_dM70Degrees, "REQUEST HEADING [degrees]" },
	{ ATCDownlinkMsgElementId_PR_dM71Degrees, "REQUEST GROUND TRACK [degrees]" },
	{ ATCDownlinkMsgElementId_PR_dM72Level, "REACHING [level]" },
	{ ATCDownlinkMsgElementId_PR_dM73Versionnumber, "[versionnumber]" },
	{ ATCDownlinkMsgElementId_PR_dM74NULL, "REQUEST TO MAINTAIN OWN SEPARATION AND VMC" },
	{ ATCDownlinkMsgElementId_PR_dM75NULL, "AT PILOTS DISCRETION" },
	{ ATCDownlinkMsgElementId_PR_dM76LevelLevel, "REACHING BLOCK [level] TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM77LevelLevel, "ASSIGNED BLOCK [level] TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM78TimeDistanceToFromPosition, "AT [time] [distance] [tofrom] [position]" },
	{ ATCDownlinkMsgElementId_PR_dM79AtisCode, "ATIS [atiscode]" },
	{ ATCDownlinkMsgElementId_PR_dM80DistanceSpecifiedDirection, "DEVIATING UP TO [offset] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM81LevelTime, "WE CAN ACCEPT [level] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM82Level, "WE CANNOT ACCEPT [level]" },
	{ ATCDownlinkMsgElementId_PR_dM83SpeedTime, "WE CAN ACCEPT [speed] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM84Speed, "WE CANNOT ACCEPT [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM85DistanceSpecifiedDirectionTime, "WE CAN ACCEPT [offset] [direction] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM86DistanceSpecifiedDirection, "WE CANNOT ACCEPT [offset] [direction]" },
	{ ATCDownlinkMsgElementId_PR_dM87Level, "WHEN CAN WE EXPECT CLIMB TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM88Level, "WHEN CAN WE EXPECT DESCENT TO [level]" },
	{ ATCDownlinkMsgElementId_PR_dM89UnitnameFrequency, "MONITORING [unitname] [frequency]" },
	{ ATCDownlinkMsgElementId_PR_dM90FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM91FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM92FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM93FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM94FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM95FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM96FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM97FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM98FreeText, "FREE TEXT" },
	{ ATCDownlinkMsgElementId_PR_dM99NULL, "CURRENT DATA AUTHORITY" },
	{ ATCDownlinkMsgElementId_PR_dM100NULL, "LOGICAL ACKNOWLEDGMENT" },
	{ ATCDownlinkMsgElementId_PR_dM101NULL, "REQUEST END OF SERVICE" },
	{ ATCDownlinkMsgElementId_PR_dM102NULL, "LANDING REPORT" },
	{ ATCDownlinkMsgElementId_PR_dM103NULL, "CANCELLING IFR" },
	{ ATCDownlinkMsgElementId_PR_dM104PositionTime, "ETA [position] [time]" },
	{ ATCDownlinkMsgElementId_PR_dM105Airport, "ALTERNATE AERODROME [airport]" },
	{ ATCDownlinkMsgElementId_PR_dM106Level, "PREFERRED LEVEL [level]" },
	{ ATCDownlinkMsgElementId_PR_dM107NULL, "NOT AUTHORIZED NEXT DATA AUTHORITY" },
	{ ATCDownlinkMsgElementId_PR_dM108NULL, "DE-ICING COMPLETE" },
	{ ATCDownlinkMsgElementId_PR_dM109Time, "TOP OF DESCENT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM110Position, "TOP OF DESCENT [position]" },
	{ ATCDownlinkMsgElementId_PR_dM111TimePosition, "TOP OF DESCENT [time] [position]" },
	{ ATCDownlinkMsgElementId_PR_dM112NULL, "SQUAWKING 7500" },
	{ ATCDownlinkMsgElementId_PR_dM113SpeedTypeSpeedTypeSpeedTypeSpeed, "[speedType] [speedType] [speedType] SPEED [speed]" },
	{ 0, NULL }
};

/*************************************************
 * Helper functions used in ASN.1 type formatters
 *************************************************/

static GByteArray *_stringify_ShortTsap(GByteArray *array, ShortTsap_t const *tsap) {
	if(tsap->aRS != NULL) {
		array = g_byte_array_append(array, tsap->aRS->buf, tsap->aRS->size);
	}
	array = g_byte_array_append(array, tsap->locSysNselTsel.buf, tsap->locSysNselTsel.size);
	return array;
}

/************************
 * ASN.1 type formatters
 ************************/

LA_ASN1_FORMATTER_FUNC(asn1_output_acse_as_text) {
	la_asn1_output(p, asn1_acse_formatter_table_text, asn1_acse_formatter_table_text_len, true);
}

LA_ASN1_FORMATTER_FUNC(asn1_output_icao_as_text) {
	la_asn1_output(p, asn1_icao_formatter_table_text, asn1_icao_formatter_table_text_len, true);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SEQUENCE_acse_as_text) {
	la_format_SEQUENCE_as_text(p, asn1_output_acse_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_CHOICE_acse_as_text) {
	la_format_CHOICE_as_text(p, NULL, asn1_output_acse_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Associate_result_as_text) {
	la_format_INTEGER_as_ENUM_as_text(p, Associate_result_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Release_request_reason_as_text) {
	la_format_INTEGER_as_ENUM_as_text(p, Release_request_reason_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Release_response_reason_as_text) {
	la_format_INTEGER_as_ENUM_as_text(p, Release_response_reason_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ABRT_source_as_text) {
	la_format_INTEGER_as_ENUM_as_text(p, ABRT_source_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_CHOICE_icao_as_text) {
	la_format_CHOICE_as_text(p, NULL, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SEQUENCE_icao_as_text) {
	la_format_SEQUENCE_as_text(p, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SEQUENCE_OF_icao_as_text) {
	la_format_SEQUENCE_OF_as_text(p, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ATCDownlinkMsgElementId_as_text) {
	la_format_CHOICE_as_text(p, ATCDownlinkMsgElementId_labels, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ATCUplinkMsgElementId_as_text) {
	la_format_CHOICE_as_text(p, ATCUplinkMsgElementId_labels, asn1_output_icao_as_text);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Code_as_text) {
	Code_t const *code = p.sptr;
	long **cptr = code->list.array;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %ld%ld%ld%ld\n",
			p.label,
			*cptr[0],
			*cptr[1],
			*cptr[2],
			*cptr[3]
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DateTime_as_text) {
	DateTime_t const *dtg = p.sptr;
	Date_t const *d = &dtg->date;
	Time_t const *t = &dtg->time;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld\n", p.label,
			d->year, d->month, d->day,
			t->hours, t->minutes);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DateTimeGroup_as_text) {
	DateTimeGroup_t const *dtg = p.sptr;
	Date_t const *d = &dtg->date;
	Timehhmmss_t const *t = &dtg->timehhmmss;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n", p.label,
			d->year, d->month, d->day,
			t->hoursminutes.hours, t->hoursminutes.minutes, t->seconds);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Time_as_text) {
	Time_t const *t = p.sptr;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %02ld:%02ld\n", p.label, t->hours, t->minutes);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Latitude_as_text) {
	Latitude_t const *lat = p.sptr;
	long const ldir = lat->latitudeDirection;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LatitudeDirection, ldir);
	switch(lat->latitudeType.present) {
		case LatitudeType_PR_latitudeDegrees:
			LA_ISPRINTF(p.vstr, p.indent, "%s:   %02ld %s\n",
					p.label,
					lat->latitudeType.choice.latitudeDegrees,
					ldir_name
					);
			break;
		case LatitudeType_PR_latitudeDegreesMinutes:
			LA_ISPRINTF(p.vstr, p.indent, "%s:   %02ld %05.2f' %s\n",
					p.label,
					lat->latitudeType.choice.latitudeDegreesMinutes.latitudeWholeDegrees,
					lat->latitudeType.choice.latitudeDegreesMinutes.minutesLatLon / 100.0,
					ldir_name
					);
			break;
		case LatitudeType_PR_latitudeDMS:
			LA_ISPRINTF(p.vstr, p.indent, "%s:   %02ld %02ld' %02ld\" %s\n",
					p.label,
					lat->latitudeType.choice.latitudeDMS.latitudeWholeDegrees,
					lat->latitudeType.choice.latitudeDMS.latlonWholeMinutes,
					lat->latitudeType.choice.latitudeDMS.secondsLatLon,
					ldir_name
					);
			break;
		case LatitudeType_PR_NOTHING:
		default:
			LA_ISPRINTF(p.vstr, p.indent, "%s: none\n", p.label);
			break;
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Longitude_as_text) {
	Longitude_t const *lon = p.sptr;
	long const ldir = lon->longitudeDirection;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LongitudeDirection, ldir);
	switch(lon->longitudeType.present) {
		case LongitudeType_PR_longitudeDegrees:
			LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %s\n",
					p.label,
					lon->longitudeType.choice.longitudeDegrees,
					ldir_name
					);
			break;
		case LongitudeType_PR_longitudeDegreesMinutes:
			LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %05.2f' %s\n",
					p.label,
					lon->longitudeType.choice.longitudeDegreesMinutes.longitudeWholeDegrees,
					lon->longitudeType.choice.longitudeDegreesMinutes.minutesLatLon / 100.0,
					ldir_name
					);
			break;
		case LongitudeType_PR_longitudeDMS:
			LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %02ld' %02ld\" %s\n",
					p.label,
					lon->longitudeType.choice.longitudeDMS.longitudeWholeDegrees,
					lon->longitudeType.choice.longitudeDMS.latLonWholeMinutes,
					lon->longitudeType.choice.longitudeDMS.secondsLatLon,
					ldir_name
					);
			break;
		case LongitudeType_PR_NOTHING:
		default:
			LA_ISPRINTF(p.vstr, p.indent, "%s: none\n", p.label);
			break;
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_AltimeterEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " inHg", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_AltimeterMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " hPa", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Deg_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " deg", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DepartureMinimumInterval_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceKm_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km", 0.25, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceNm_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Humidity_as_text) {
	la_format_INTEGER_with_unit_as_text(p, "%%", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_DistanceMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Frequencyvhf_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " MHz", 0.005, 3);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Frequencyuhf_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " MHz", 0.025, 3);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Frequencyhf_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kHz", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LegTime_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LevelFeet_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " ft", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LevelFlightLevelMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Meters_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m", 1, 0);
}

// RejectDetails is a CHOICE whose all values are NULLs.  Aliasing them all to
// unique types just to print them with la_asn1_format_label_only_as_text would be an
// unnecessary overengineering.  Handling all values in a single routine is
// simpler, albeit less elegant at first glance.
static LA_ASN1_FORMATTER_FUNC(asn1_format_RejectDetails_as_text) {
	RejectDetails_t const *det = p.sptr;
	if(p.label != NULL) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: ", p.label);
	}
	switch(det->present) {
		case RejectDetails_PR_aDS_service_unavailable:
			la_vstring_append_sprintf(p.vstr, "ADS service unavailable\n");
			break;
		case RejectDetails_PR_undefined_reason:
			la_vstring_append_sprintf(p.vstr, "undefined reason\n");
			break;
		case RejectDetails_PR_maximum_capacity_exceeded:
			la_vstring_append_sprintf(p.vstr, "max. capacity exceeded\n");
			break;
		case RejectDetails_PR_reserved:
			la_vstring_append_sprintf(p.vstr, "(reserved)\n");
			break;
		case RejectDetails_PR_waypoint_in_request_not_on_the_route:
			la_vstring_append_sprintf(p.vstr, "requested waypoint not on the route\n");
			break;
		case RejectDetails_PR_aDS_contract_not_supported:
			la_vstring_append_sprintf(p.vstr, "ADS contract not supported\n");
			break;
		case RejectDetails_PR_noneOfReportTypesSupported:
			la_vstring_append_sprintf(p.vstr, "none of report types supported\n");
			break;
		case RejectDetails_PR_noneOfEventTypesSupported:
			la_vstring_append_sprintf(p.vstr, "none of event types supported\n");
			break;
		case RejectDetails_PR_NOTHING:
		default:
			la_vstring_append_sprintf(p.vstr, "none\n");
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ReportingRate_as_text) {
	ReportingRate_t const *rate = p.sptr;
	switch(rate->present) {
		case ReportingRate_PR_reporting_time_seconds_scale:
			p.sptr = &rate->choice.reporting_time_seconds_scale;
			la_format_INTEGER_with_unit_as_text(p, " sec", 1, 0);
			break;
		case ReportingRate_PR_reporting_time_minutes_scale:
			p.sptr = &rate->choice.reporting_time_minutes_scale;
			la_format_INTEGER_with_unit_as_text(p, " min", 1, 0);
			break;
		default:
			break;
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_RTASecTolerance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " sec", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_RTATolerance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Feet_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " ft", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km/h", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedIndicated_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_SpeedMach_as_text) {
	la_format_INTEGER_with_unit_as_text(p, "", 0.001, 3);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Temperature_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " C", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_VerticalRateEnglish_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " ft/min", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_VerticalRateMetric_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m/min", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LongTsap_as_text) {
	LongTsap_t const *tsap = p.sptr;
	GByteArray *tmparray = g_byte_array_new();
	tmparray = g_byte_array_append(tmparray, tsap->rDP.buf, tsap->rDP.size);
	tmparray = _stringify_ShortTsap(tmparray, &tsap->shortTsap);

	LA_ISPRINTF(p.vstr, p.indent, "%s: ", p.label);
	octet_string_with_ascii_format_text(p.vstr,
			&(octet_string_t){ .buf = tmparray->data, .len = tmparray->len },
			0);
	EOL(p.vstr);
	g_byte_array_free(tmparray, TRUE);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ShortTsap_as_text) {
	ShortTsap_t const *tsap = p.sptr;
	GByteArray *tmparray = g_byte_array_new();
	tmparray = _stringify_ShortTsap(tmparray, tsap);
	LA_ISPRINTF(p.vstr, p.indent, "%s: ", p.label);
	octet_string_with_ascii_format_text(p.vstr,
			&(octet_string_t){ .buf = tmparray->data, .len = tmparray->len },
			0);
	EOL(p.vstr);
	g_byte_array_free(tmparray, TRUE);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_UnitName_as_text) {
	UnitName_t const *un = p.sptr;
	char *fdes = XCALLOC(un->facilityDesignation.size + 1, sizeof(char));
	snprintf(fdes, un->facilityDesignation.size + 1, "%s", un->facilityDesignation.buf);
	char *fname = NULL;
	FacilityName_t *fn = un->facilityName;
	if(fn != NULL) {
		fname = XCALLOC(fn->size + 1, sizeof(char));
		snprintf(fname, fn->size + 1, "%s", fn->buf);
	}
	long const ffun = un->facilityFunction;
	char const *ffun_name = la_asn1_value2enum(&asn_DEF_FacilityFunction, ffun);
	LA_ISPRINTF(p.vstr, p.indent, "%s: %s, %s, %s\n", p.label, fdes, fname ? fname : "", ffun_name);
	XFREE(fdes);
	XFREE(fname);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2DateTimeGroup_as_text) {
	ADSv2DateTimeGroup_t const *dtg = p.sptr;
	Date_t const *d = &dtg->date;
	Timesec_t const *t = &dtg->time;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n", p.label,
			d->year, d->month, d->day,
			t->hours, t->minutes, t->seconds);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EstimatedPositionUncertainty_as_text) {
	EstimatedPositionUncertainty_t const *epu = p.sptr;
	if(*epu == 9900) {
		LA_ISPRINTF(p.vstr, p.indent, "%s: complete-loss\n", p.label);
	} else {
		la_format_INTEGER_with_unit_as_text(p, " nm", 0.01, 2);
	}
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2Latitude_as_text) {
	ADSv2Latitude_t const *lat = p.sptr;
	long const ldir = lat->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LatitudeDirection, ldir);
	LA_ISPRINTF(p.vstr, p.indent, "%s:  %02ld %02ld' %04.1f\" %s\n",
			p.label,
			lat->degrees,
			lat->minutes,
			lat->seconds / 10.0,
			ldir_name
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2Longitude_as_text) {
	ADSv2Longitude_t const *lon = p.sptr;
	long const ldir = lon->direction;
	char const *ldir_name = la_asn1_value2enum(&asn_DEF_LongitudeDirection, ldir);
	LA_ISPRINTF(p.vstr, p.indent, "%s: %03ld %02ld' %04.1f\" %s\n",
			p.label,
			lon->degrees,
			lon->minutes,
			lon->seconds / 10.0,
			ldir_name
			);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSAircraftPDUs_as_text) {
	ADSAircraftPDUs_t const *apdus = p.sptr;
	// Omit the timestamp for brevity, print the PDU only
	p.td = &asn_DEF_ADSAircraftPDU;
	p.sptr = &apdus->adsAircraftPdu;
	asn1_output_icao_as_text(p);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2Temperature_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " C", 0.25, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2WindSpeedKts_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSv2WindSpeedKmh_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " km/h", 2, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EmergencyUrgencyStatus_as_text) {
	la_format_BIT_STRING_as_text(p, EmergencyUrgencyStatus_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPPTimeInterval_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " minutes", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EventTypeNotSupported_as_text) {
	la_format_BIT_STRING_as_text(p, EventTypeNotSupported_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_GrossMass_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kg", 10, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ADSGroundPDUs_as_text) {
	ADSGroundPDUs_t const *apdus = p.sptr;
	// Omit the timestamp for brevity, print the PDU only
	p.td = &asn_DEF_ADSGroundPDU;
	p.sptr = &apdus->adsGroundPdu;
	asn1_output_icao_as_text(p);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPPLimitations_as_text) {
	la_format_BIT_STRING_as_text(p, EPPLimitations_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPPTolETA_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPPTolGCDistance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_EPUChangeTolerance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_GroundSpeed_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " kts", 0.5, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_GroundTrack_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " deg", 0.05, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_LateralDeviationThreshold_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_MachNumberTolerance_as_text) {
	la_format_INTEGER_with_unit_as_text(p, "", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Modulus_as_text) {
	long const *val = p.sptr;
	LA_ISPRINTF(p.vstr, p.indent, "%s: every %ld reports\n", p.label, *val);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_ReportTypeNotSupported_as_text) {
	la_format_BIT_STRING_as_text(p, ReportTypeNotSupported_bit_labels);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_RNPValue_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TurbulenceEDRValue_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " m^2/s^3", 0.01, 2);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TurbulenceMinutesInThePast_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 0.5, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TurbulenceObservationWindow_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " min", 1, 0);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_TurnRadius_as_text) {
	la_format_INTEGER_with_unit_as_text(p, " nm", 0.1, 1);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_Timesec_as_text) {
	Timesec_t const *t = p.sptr;
	LA_ISPRINTF(p.vstr, p.indent, "%s: %02ld:%02ld:%02ld\n", p.label, t->hours, t->minutes, t->seconds);
}

static LA_ASN1_FORMATTER_FUNC(asn1_format_VerticalType_as_text) {
	la_format_BIT_STRING_as_text(p, VerticalType_bit_labels);
}

la_asn1_formatter const asn1_icao_formatter_table_text[] = {
	// atn-b1_cpdlc-v1.asn1
	{ .type = &asn_DEF_AircraftAddress, .format = la_asn1_format_any_as_text, .label = "Aircraft address" },
	{ .type = &asn_DEF_AirInitiatedApplications, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Air-initiated applications" },
	{ .type = &asn_DEF_AirOnlyInitiatedApplications, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Air-only-initiated applications" },
	{ .type = &asn_DEF_Airport, .format = la_asn1_format_any_as_text, .label = "Airport" },
	{ .type = &asn_DEF_AirportDeparture, .format = la_asn1_format_any_as_text, .label = "Departure airport" },
	{ .type = &asn_DEF_AirportDestination, .format = la_asn1_format_any_as_text, .label = "Destination airport" },
	{ .type = &asn_DEF_Altimeter, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AltimeterEnglish, .format = asn1_format_AltimeterEnglish_as_text, .label = "Altimeter" },
	{ .type = &asn_DEF_AltimeterMetric, .format = asn1_format_AltimeterMetric_as_text, .label = "Altimeter" },
	{ .type = &asn_DEF_ATCDownlinkMessage, .format = asn1_format_SEQUENCE_icao_as_text, .label = "CPDLC Downlink Message" },
	{ .type = &asn_DEF_ATCDownlinkMessageData, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Message data" },
	{ .type = &asn_DEF_ATCDownlinkMsgElementId, .format = asn1_format_ATCDownlinkMsgElementId_as_text, .label = NULL },
	{ .type = &asn_DEF_ATCDownlinkMsgElementIdSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATCMessageHeader, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Header" },
	{ .type = &asn_DEF_ATCUplinkMessage, .format = asn1_format_SEQUENCE_icao_as_text, .label = "CPDLC Uplink Message" },
	{ .type = &asn_DEF_ATCUplinkMessageData, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Message data" },
	{ .type = &asn_DEF_ATCUplinkMsgElementId, .format = asn1_format_ATCUplinkMsgElementId_as_text, .label = NULL },
	{ .type = &asn_DEF_ATCUplinkMsgElementIdSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATISCode, .format = la_asn1_format_any_as_text, .label = "ATIS code" },
	{ .type = &asn_DEF_ATSRouteDesignator, .format = la_asn1_format_any_as_text, .label = "ATS route" },
	{ .type = &asn_DEF_ATWAlongTrackWaypoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATWAlongTrackWaypointSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Along-track waypoints" },
	{ .type = &asn_DEF_ATWDistance, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATWDistanceTolerance, .format = la_asn1_format_ENUM_as_text, .label = "ATW Distance Tolerance" },
	{ .type = &asn_DEF_ATWLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ATWLevelSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "ATW Levels" },
	{ .type = &asn_DEF_ATWLevelTolerance, .format = la_asn1_format_ENUM_as_text, .label = "ATW Level Tolerance" },
	{ .type = &asn_DEF_BlockLevel, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Block level" },
	{ .type = &asn_DEF_ClearanceType, .format = la_asn1_format_ENUM_as_text, .label = "Clearance type" },
	{ .type = &asn_DEF_Code, .format = asn1_format_Code_as_text, .label = "Code" },
	{ .type = &asn_DEF_ControlledTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DateTimeDepartureETD, .format = asn1_format_DateTime_as_text, .label = "Departure time" },
	{ .type = &asn_DEF_DateTimeGroup, .format = asn1_format_DateTimeGroup_as_text, .label = "Timestamp" },
	{ .type = &asn_DEF_DegreeIncrement, .format = asn1_format_Deg_as_text, .label = "Degree increment" },
	{ .type = &asn_DEF_Degrees, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DegreesMagnetic, .format = asn1_format_Deg_as_text, .label = "Degrees (magnetic)" },
	{ .type = &asn_DEF_DegreesTrue, .format = asn1_format_Deg_as_text, .label = "Degrees (true)" },
	{ .type = &asn_DEF_DepartureClearance, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DepartureMinimumInterval, .format = asn1_format_DepartureMinimumInterval_as_text, .label = "Minimum interval of departures" },
	{ .type = &asn_DEF_Direction, .format = la_asn1_format_ENUM_as_text, .label = "Direction" },
	{ .type = &asn_DEF_DirectionDegrees, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Distance, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceKm, .format = asn1_format_DistanceKm_as_text, .label = "Distance" },
	{ .type = &asn_DEF_DistanceNm, .format = asn1_format_DistanceNm_as_text, .label = "Distance" },
	{ .type = &asn_DEF_DistanceSpecified, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedDirection, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedDirectionTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedKm, .format = asn1_format_DistanceMetric_as_text, .label = "Offset" },
	{ .type = &asn_DEF_DistanceSpecifiedNm, .format = asn1_format_DistanceEnglish_as_text, .label = "Offset" },
	{ .type = &asn_DEF_DMVersionNumber, .format = la_asn1_format_any_as_text, .label = "Version number" },
	{ .type = &asn_DEF_ErrorInformation, .format = la_asn1_format_ENUM_as_text, .label = "Error information" },
	{ .type = &asn_DEF_Facility, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FacilityDesignation, .format = la_asn1_format_any_as_text, .label = "Facility designation" },
	{ .type = &asn_DEF_FacilityDesignationAltimeter, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FacilityDesignationATISCode, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FacilityName, .format = la_asn1_format_any_as_text, .label = "Facility name" },
	{ .type = &asn_DEF_Fix, .format = la_asn1_format_any_as_text, .label = "Fix" },
	{ .type = &asn_DEF_FixName, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FixNext, .format = asn1_format_CHOICE_icao_as_text, .label = "Next fix" },
	{ .type = &asn_DEF_FixNextPlusOne, .format = asn1_format_CHOICE_icao_as_text, .label = "Next+1 fix" },
	{ .type = &asn_DEF_FlightInformation, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_FreeText, .format = la_asn1_format_any_as_text, .label = NULL },
	{ .type = &asn_DEF_Frequency, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Frequencyhf, .format = asn1_format_Frequencyhf_as_text, .label = "HF" },
	{ .type = &asn_DEF_Frequencysatchannel, .format = la_asn1_format_any_as_text, .label = "Satcom channel" },
	{ .type = &asn_DEF_Frequencyuhf, .format = asn1_format_Frequencyuhf_as_text, .label = "UHF" },
	{ .type = &asn_DEF_Frequencyvhf, .format = asn1_format_Frequencyvhf_as_text, .label = "VHF" },
	{ .type = &asn_DEF_FurtherInstructions, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_GroundInitiatedApplications, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Ground-initiated applications" },
	{ .type = &asn_DEF_GroundOnlyInitiatedApplications, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Ground-only-initiated applications" },
	{ .type = &asn_DEF_Holdatwaypoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_HoldatwaypointSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Holding points" },
	{ .type = &asn_DEF_HoldatwaypointSpeedHigh, .format = asn1_format_CHOICE_icao_as_text, .label = "Max speed" },
	{ .type = &asn_DEF_HoldatwaypointSpeedLow, .format = asn1_format_CHOICE_icao_as_text, .label = "Min speed" },
	{ .type = &asn_DEF_HoldClearance, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Humidity, .format = asn1_format_Humidity_as_text, .label = "Humidity" },
	{ .type = &asn_DEF_Icing, .format = la_asn1_format_ENUM_as_text, .label = "Icing" },
	{ .type = &asn_DEF_InterceptCourseFrom, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_InterceptCourseFromSelection, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_InterceptCourseFromSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Intercept courses" },
	{ .type = &asn_DEF_Latitude, .format = asn1_format_Latitude_as_text, .label = "Latitude" },
	{ .type = &asn_DEF_LatitudeDirection, .format = la_asn1_format_ENUM_as_text, .label = "Direction" },
	{ .type = &asn_DEF_LatitudeLongitude, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LatitudeReportingPoints, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LatitudeType, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LatLonReportingPoints, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LegDistance, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LegDistanceEnglish, .format = asn1_format_DistanceEnglish_as_text, .label = "Leg distance" },
	{ .type = &asn_DEF_LegDistanceMetric, .format = asn1_format_DistanceMetric_as_text, .label = "Leg distance" },
	{ .type = &asn_DEF_LegTime, .format = asn1_format_LegTime_as_text, .label = "Leg time" },
	{ .type = &asn_DEF_LegType, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Level, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelFeet, .format = asn1_format_LevelFeet_as_text, .label = "Flight level" },
	{ .type = &asn_DEF_LevelFlightLevel, .format = la_asn1_format_any_as_text, .label = "Flight level" },
	{ .type = &asn_DEF_LevelFlightLevelMetric, .format = asn1_format_LevelFlightLevelMetric_as_text, .label = "Flight level" },
	{ .type = &asn_DEF_LevelLevel, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelMeters, .format = asn1_format_Meters_as_text, .label = "Flight level" },
	{ .type = &asn_DEF_LevelPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelProcedureName, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelsOfFlight, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelSpeedSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelType, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LogicalAck, .format = la_asn1_format_ENUM_as_text, .label = "Logical ACK" },
	{ .type = &asn_DEF_Longitude, .format = asn1_format_Longitude_as_text, .label = "Longitude" },
	{ .type = &asn_DEF_LongitudeDirection, .format = la_asn1_format_ENUM_as_text, .label = "Direction" },
	{ .type = &asn_DEF_LongitudeReportingPoints, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LongitudeType, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_MsgIdentificationNumber, .format = la_asn1_format_any_as_text, .label = "Msg ID" },
	{ .type = &asn_DEF_MsgReferenceNumber, .format = la_asn1_format_any_as_text, .label = "Msg Ref" },
	{ .type = &asn_DEF_Navaid, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_NavaidName, .format = la_asn1_format_any_as_text, .label = "Navaid" },
	{ .type = &asn_DEF_NULL, .format = NULL, .label = NULL },
	{ .type = &asn_DEF_PersonsOnBoard, .format = la_asn1_format_any_as_text, .label = "Persons on board" },
	{ .type = &asn_DEF_PlaceBearing, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PlaceBearingDistance, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PlaceBearingPlaceBearing, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PMCPDLCProviderAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "CPDLC Provider Abort Reason" },
	{ .type = &asn_DEF_PMCPDLCUserAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "CPDLC User Abort Reason" },
	{ .type = &asn_DEF_Position, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionDegrees, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionDistanceSpecifiedDirection, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionLevelLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionLevelSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionPosition, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionProcedureName, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionRouteClearanceIndex, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionSpeedSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionTimeLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionTimeTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PositionUnitNameFrequency, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PosReportTrackAngle, .format = asn1_format_CHOICE_icao_as_text, .label = "Track angle" },
	{ .type = &asn_DEF_PosReportHeading, .format = asn1_format_CHOICE_icao_as_text, .label = "Heading" },
	{ .type = &asn_DEF_Procedure, .format = la_asn1_format_any_as_text, .label = "Procedure" },
	{ .type = &asn_DEF_ProcedureApproach, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Approach procedure" },
	{ .type = &asn_DEF_ProcedureArrival, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Arrival procedure" },
	{ .type = &asn_DEF_ProcedureDeparture, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Departure procedure" },
	{ .type = &asn_DEF_ProcedureName, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ProcedureTransition, .format = la_asn1_format_any_as_text, .label = "Procedure transition" },
	{ .type = &asn_DEF_ProcedureType, .format = la_asn1_format_ENUM_as_text, .label = "Procedure type" },
	{ .type = &asn_DEF_ProtectedAircraftPDUs, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ProtectedGroundPDUs, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_PublishedIdentifier, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RemainingFuel, .format = asn1_format_Time_as_text, .label = "Remaining fuel" },
	{ .type = &asn_DEF_RemainingFuelPersonsOnBoard, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ReportedWaypointLevel, .format = asn1_format_CHOICE_icao_as_text, .label = "Reported waypoint level" },
	{ .type = &asn_DEF_ReportedWaypointPosition, .format = asn1_format_CHOICE_icao_as_text, .label = "Reported waypoint position" },
	{ .type = &asn_DEF_ReportedWaypointTime, .format = asn1_format_Time_as_text, .label = "Reported waypoint time" },
	{ .type = &asn_DEF_ReportingPoints, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RevisionNumber, .format = la_asn1_format_any_as_text, .label = "Revision number" },
	{ .type = &asn_DEF_RouteAndLevels, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RouteClearance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Route clearance" },
	{ .type = &asn_DEF_RouteClearanceIndex, .format = la_asn1_format_any_as_text, .label = "Route clearance index" },
	{ .type = &asn_DEF_RouteClearanceConstrainedData, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RouteClearanceSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RouteInformation, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RouteInformationAdditional, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Additional route information" },
	{ .type = &asn_DEF_RouteInformationSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Route" },
	{ .type = &asn_DEF_RTARequiredTimeArrival, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RTARequiredTimeArrivalSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Required arrival times" },
	{ .type = &asn_DEF_RTATime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RTATolerance, .format = asn1_format_RTATolerance_as_text, .label = "RTA Tolerance" },
	{ .type = &asn_DEF_Runway, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RunwayArrival, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Arrival runway" },
	{ .type = &asn_DEF_RunwayConfiguration, .format = la_asn1_format_ENUM_as_text, .label = "Runway configuration" },
	{ .type = &asn_DEF_RunwayDeparture, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Departure runway" },
	{ .type = &asn_DEF_RunwayDirection, .format = la_asn1_format_any_as_text, .label = "Runway direction" },
	{ .type = &asn_DEF_RunwayRVR, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RVR, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_RVRFeet, .format = asn1_format_Feet_as_text, .label = "RVR" },
	{ .type = &asn_DEF_RVRMeters, .format = asn1_format_Meters_as_text, .label = "RVR" },
	{ .type = &asn_DEF_Speed, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedGround, .format = asn1_format_SpeedEnglish_as_text, .label = "Ground speed" },
	{ .type = &asn_DEF_SpeedGroundMetric, .format = asn1_format_SpeedMetric_as_text, .label = "Ground speed" },
	{ .type = &asn_DEF_SpeedIndicated, .format = asn1_format_SpeedIndicated_as_text, .label = "Indicated airspeed" },
	{ .type = &asn_DEF_SpeedIndicatedMetric, .format = asn1_format_SpeedMetric_as_text, .label = "Indicated airspeed" },
	{ .type = &asn_DEF_SpeedMach, .format = asn1_format_SpeedMach_as_text, .label = "Mach" },
	{ .type = &asn_DEF_SpeedSpeed, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedTime, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedTrue, .format = asn1_format_SpeedEnglish_as_text, .label = "True airspeed" },
	{ .type = &asn_DEF_SpeedTrueMetric, .format = asn1_format_SpeedMetric_as_text, .label = "True airspeed" },
	{ .type = &asn_DEF_SpeedType, .format = la_asn1_format_ENUM_as_text, .label = "Speed type" },
	{ .type = &asn_DEF_SpeedTypeSpeedTypeSpeedType, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedTypeSpeedTypeSpeedTypeSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_Temperature, .format = asn1_format_Temperature_as_text, .label = "Temperature" },
	{ .type = &asn_DEF_Time, .format = asn1_format_Time_as_text, .label = "Time" },
	{ .type = &asn_DEF_TimeDepAllocated, .format = asn1_format_Time_as_text, .label = "Allocated departure time" },
	{ .type = &asn_DEF_TimeDepClearanceExpected, .format = asn1_format_Time_as_text, .label = "Expected departure clearance time" },
	{ .type = &asn_DEF_TimeDeparture, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeDistanceSpecifiedDirection, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeDistanceToFromPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeETAatFixNext, .format = asn1_format_Time_as_text, .label = "ETA at next fix" },
	{ .type = &asn_DEF_TimeETAatDest, .format = asn1_format_Time_as_text, .label = "ETA at destination" },
	{ .type = &asn_DEF_Timehhmmss, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimePosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimePositionLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimePositionLevelSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeSpeedSpeed, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeTime, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeToFromPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TimeTolerance, .format = la_asn1_format_ENUM_as_text, .label = "Time tolerance" },
	{ .type = &asn_DEF_TimeUnitNameFrequency, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ToFrom, .format = la_asn1_format_ENUM_as_text, .label = "To/From" },
	{ .type = &asn_DEF_ToFromPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_TrafficType, .format = la_asn1_format_ENUM_as_text, .label = "Traffic type" },
	{ .type = &asn_DEF_Turbulence, .format = la_asn1_format_ENUM_as_text, .label = "Turbulence" },
	{ .type = &asn_DEF_UnitName, .format = asn1_format_UnitName_as_text, .label = "Unit name" },
	{ .type = &asn_DEF_UnitNameFrequency, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_VerticalChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_VerticalDirection, .format = la_asn1_format_ENUM_as_text, .label = "Vertical direction" },
	{ .type = &asn_DEF_VerticalRate, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_VerticalRateEnglish, .format = asn1_format_VerticalRateEnglish_as_text, .label = "Vertical rate" },
	{ .type = &asn_DEF_VerticalRateMetric, .format = asn1_format_VerticalRateMetric_as_text, .label = "Vertical rate" },
	{ .type = &asn_DEF_WaypointSpeedLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_WaypointSpeedLevelSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Waypoints, speeds and levels" },
	{ .type = &asn_DEF_WindDirection, .format = asn1_format_Deg_as_text, .label = "Wind direction" },
	{ .type = &asn_DEF_Winds, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_WindSpeed, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_WindSpeedEnglish, .format = asn1_format_SpeedEnglish_as_text, .label = "Wind speed" },
	{ .type = &asn_DEF_WindSpeedMetric, .format = asn1_format_SpeedMetric_as_text, .label = "Wind speed" },
	// atn-b1_cm.asn1
	{ .type = &asn_DEF_APAddress, .format = asn1_format_CHOICE_icao_as_text, .label = "AP Address" },
	{ .type = &asn_DEF_AEQualifier, .format = la_asn1_format_any_as_text, .label = "Application Entity Qualifier" },
	{ .type = &asn_DEF_AEQualifierVersion, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_AEQualifierVersionAddress, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ARS, .format = la_asn1_format_any_as_text, .label = "ARS" },
	{ .type = &asn_DEF_AircraftFlightIdentification, .format = la_asn1_format_any_as_text, .label = "Flight ID" },
	{ .type = &asn_DEF_CMAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "ATN Context Management - Abort Reason" },
	{ .type = &asn_DEF_CMAircraftMessage, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_CMGroundMessage, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_CMContactRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Contact Request" },
	{ .type = &asn_DEF_CMContactResponse, .format = la_asn1_format_ENUM_as_text, .label = "ATN Context Management - Contact Response" },
	{ .type = &asn_DEF_CMForwardRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Forward Request" },
	{ .type = &asn_DEF_CMForwardResponse, .format = la_asn1_format_ENUM_as_text, .label = "ATN Context Management - Forward Response" },
	{ .type = &asn_DEF_CMLogonRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Logon Request" },
	{ .type = &asn_DEF_CMLogonResponse, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Logon Response" },
	{ .type = &asn_DEF_CMUpdate, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ATN Context Management - Update" },
	// Handled by asn_DEF_ShortTsap formatter
	//{ .type = &asn_DEF_LocSysNselTsel, .format = la_asn1_format_any_as_text, .label = "LOC/SYS/NSEL/TSEL" },
	{ .type = &asn_DEF_LongTsap, .format = asn1_format_LongTsap_as_text, .label = "Long TSAP" },
	{ .type = &asn_DEF_OCTET_STRING, .format = la_asn1_format_any_as_text, .label = NULL },
	{ .type = &asn_DEF_RDP, .format = la_asn1_format_any_as_text, .label = "RDP" },
	{ .type = &asn_DEF_ShortTsap, .format = asn1_format_ShortTsap_as_text, .label = "Short TSAP" },
	{ .type = &asn_DEF_VersionNumber, .format = la_asn1_format_any_as_text, .label = "Version number" },
	// atn-b1_pmadsc.asn1
	{ .type = &asn_DEF_ADSAircraftPDU, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSAircraftPDUs, .format = asn1_format_ADSAircraftPDUs_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSGroundPDU, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSGroundPDUs, .format = asn1_format_ADSGroundPDUs_as_text, .label = NULL },
	{ .type = &asn_DEF_CancelAllContracts, .format = la_asn1_format_label_only_as_text, .label = "ADS-C v2 Cancel All Contracts" },
	{ .type = &asn_DEF_CancelContract, .format = asn1_format_CHOICE_icao_as_text, .label = "ADS-C v2 Cancel Contract" },
	{ .type = &asn_DEF_CancelPositiveAcknowledgement, .format = la_asn1_format_ENUM_as_text, .label = "ADS-C v2 Cancel ACK" },
	{ .type = &asn_DEF_CancelRejectReason, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Cancel NAK" },
	{ .type = &asn_DEF_ProviderAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "ADS-C v2 Provider Abort" },
	{ .type = &asn_DEF_RejectReason, .format = la_asn1_format_ENUM_as_text, .label = "Reject reason" },
	{ .type = &asn_DEF_RequestType, .format = la_asn1_format_ENUM_as_text, .label = "Request type" },
	{ .type = &asn_DEF_UserAbortReason, .format = la_asn1_format_ENUM_as_text, .label = "ADS-C v2 User Abort" },
	// atn-b2_adsc_v2.asn1
	{ .type = &asn_DEF_AAISAvailability, .format = la_asn1_format_any_as_text, .label = "AAIS available" },
	{ .type = &asn_DEF_ADSAccept, .format = asn1_format_CHOICE_icao_as_text, .label = "ADS-C v2 Contract Request Accept" },
	{ .type = &asn_DEF_ADSDataReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report data" },
	{ .type = &asn_DEF_ADSEmergencyUrgencyStatus, .format = asn1_format_EmergencyUrgencyStatus_as_text, .label = "Emergency/urgency status" },
	{ .type = &asn_DEF_ADSNonCompliance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Non-Compliance Notification" },
	{ .type = &asn_DEF_ADSPositiveAcknowledgement, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 ACK" },
	{ .type = &asn_DEF_ADSReject, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Reject" },
	{ .type = &asn_DEF_ADSReport, .format = asn1_format_CHOICE_icao_as_text, .label = "ADS-C v2 Report" },
	{ .type = &asn_DEF_ADSRequestContract, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSv2DateTimeGroup, .format = asn1_format_ADSv2DateTimeGroup_as_text, .label = "Timestamp" },
	{ .type = &asn_DEF_ADSv2Latitude, .format = asn1_format_ADSv2Latitude_as_text, .label = "Lat" },
	{ .type = &asn_DEF_ADSv2LatitudeLongitude, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSv2Level, .format = asn1_format_LevelFeet_as_text, .label = "Alt" },
	{ .type = &asn_DEF_ADSv2Longitude, .format = asn1_format_ADSv2Longitude_as_text, .label = "Lon" },
	{ .type = &asn_DEF_ADSv2RequestType, .format = la_asn1_format_ENUM_as_text, .label = "Request type" },
	{ .type = &asn_DEF_ADSv2Temperature, .format = asn1_format_ADSv2Temperature_as_text, .label = "Temperature" },
	{ .type = &asn_DEF_ADSv2Turbulence, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Turbulence" },
	{ .type = &asn_DEF_ADSv2VerticalRate, .format = asn1_format_VerticalRateEnglish_as_text, .label = "Vertical rate" },
	{ .type = &asn_DEF_ADSv2WindSpeed, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ADSv2WindSpeedKmh, .format = asn1_format_ADSv2WindSpeedKmh_as_text, .label = "Wind speed" },
	{ .type = &asn_DEF_ADSv2WindSpeedKts, .format = asn1_format_ADSv2WindSpeedKts_as_text, .label = "Wind speed" },
	{ .type = &asn_DEF_ATSUListHiPrio, .format = la_asn1_format_any_as_text, .label = "High priority" },
	{ .type = &asn_DEF_ATSUListMedPrio, .format = la_asn1_format_any_as_text, .label = "Medium priority" },
	{ .type = &asn_DEF_ATSUListLoPrio, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Low priority" },
	{ .type = &asn_DEF_AirVector, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Air vector" },
	{ .type = &asn_DEF_AirVectorModulus, .format = asn1_format_Modulus_as_text, .label = "Report air vector" },
	{ .type = &asn_DEF_Airspeed, .format = asn1_format_CHOICE_icao_as_text, .label = "Airspeed" },
	{ .type = &asn_DEF_AirspeedChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report airspeed changes" },
	{ .type = &asn_DEF_AirspeedChangeTolerance, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Airspeed" },
	{ .type = &asn_DEF_AirspeedRangeChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report airspeed range changes" },
	{ .type = &asn_DEF_ClimbSpeed, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Climb speed" },
	{ .type = &asn_DEF_ConnectedATSUList, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Connected ATSU list" },
	{ .type = &asn_DEF_ContractDetailsNotSupporting, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ContractNumber, .format = la_asn1_format_any_as_text, .label = "Contract number" },
	{ .type = &asn_DEF_DCRAirVector, .format = la_asn1_format_label_only_as_text, .label = "Report air vector" },
	{ .type = &asn_DEF_DCRGroundVector, .format = la_asn1_format_label_only_as_text, .label = "Report ground vector" },
	{ .type = &asn_DEF_DCRPlannedFinalApproachSpeed, .format = la_asn1_format_label_only_as_text, .label = "Report planned final approach speed" },
	{ .type = &asn_DEF_DCRProjectedProfile, .format = la_asn1_format_label_only_as_text, .label = "Report projected profile" },
	{ .type = &asn_DEF_DCRRNPProfile, .format = la_asn1_format_label_only_as_text, .label = "Report RNP profile" },
	{ .type = &asn_DEF_DCRSpeedScheduleProfile, .format = la_asn1_format_label_only_as_text, .label = "Report speed schedule profile" },
	{ .type = &asn_DEF_DemandContractRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Demand Contract Request" },
	{ .type = &asn_DEF_DemandReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = "On-demand Report" },
	{ .type = &asn_DEF_ECRRNPNotMet, .format = la_asn1_format_label_only_as_text, .label = "Report when RNP not met" },
	{ .type = &asn_DEF_ECRRTAStatusChange, .format = la_asn1_format_label_only_as_text, .label = "Report RTA status changes" },
	{ .type = &asn_DEF_ECRWaypointChange, .format = la_asn1_format_label_only_as_text, .label = "Report waypoint changes" },
	{ .type = &asn_DEF_DescentSpeed, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Descent speed" },
	{ .type = &asn_DEF_EPPEventChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report EPP changes" },
	{ .type = &asn_DEF_EPPFlightPlanChangeRequest, .format = la_asn1_format_label_only_as_text, .label = "Report EPP flight plan changes" },
	{ .type = &asn_DEF_EPPLevel, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_EPPLimitations, .format = asn1_format_EPPLimitations_as_text, .label = "EPP limitations" },
	{ .type = &asn_DEF_EPPNextWptInHorizonRequest, .format = la_asn1_format_label_only_as_text, .label = "Report next waypoint in horizon" },
	{ .type = &asn_DEF_EPPTolGCDistance, .format = asn1_format_EPPTolGCDistance_as_text, .label = "Great circle distance" },
	{ .type = &asn_DEF_EPPTolLevel, .format = asn1_format_LevelFeet_as_text, .label = "Altitude" },
	{ .type = &asn_DEF_EPPTolETA, .format = asn1_format_EPPTolETA_as_text, .label = "ETA" },
	{ .type = &asn_DEF_EPPToleranceChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report EPP tolerance changes" },
	{ .type = &asn_DEF_EPPTolerancesValues, .format = asn1_format_SEQUENCE_icao_as_text, .label = "EPP tolerances" },
	{ .type = &asn_DEF_EPPNumWaypoints, .format = la_asn1_format_any_as_text, .label = "Number of waypoints" },
	{ .type = &asn_DEF_EPPTimeInterval, .format = asn1_format_EPPTimeInterval_as_text, .label = "Time interval" },
	{ .type = &asn_DEF_EPPRequest, .format = asn1_format_CHOICE_icao_as_text, .label = "Report extended projected profile" },
	{ .type = &asn_DEF_EPPWindow, .format = asn1_format_CHOICE_icao_as_text, .label = "EPP window" },
	{ .type = &asn_DEF_EPUChangeTolerance, .format = asn1_format_EPUChangeTolerance_as_text, .label = "Report FoM changes exceeding" },
	{ .type = &asn_DEF_ETA, .format = asn1_format_Timesec_as_text, .label = "ETA" },
	{ .type = &asn_DEF_EstimatedPositionUncertainty, .format = asn1_format_EstimatedPositionUncertainty_as_text, .label = "Estimated position uncertainty" },
	{ .type = &asn_DEF_EventContractRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Event Contract Request" },
	{ .type = &asn_DEF_EventReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Event Report" },
	{ .type = &asn_DEF_EventTypeNotSupported, .format = asn1_format_EventTypeNotSupported_as_text, .label = "Unsupported events" },
	{ .type = &asn_DEF_EventTypeReported, .format = la_asn1_format_ENUM_as_text, .label = "Reported event" },
	{ .type = &asn_DEF_ExtendedProjectedProfile, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Extended projected profile" },
	{ .type = &asn_DEF_ExtendedProjectedProfileModulus, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report extended projected profile" },
	{ .type = &asn_DEF_ExtendedWayPointSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Waypoint sequence" },
	{ .type = &asn_DEF_ExtendedWayPointSequenceElement, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint data" },
	{ .type = &asn_DEF_FigureOfMerit, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Figure of merit" },
	{ .type = &asn_DEF_FinalApproachSpeedChange, .format = asn1_format_SpeedIndicated_as_text, .label = "Report planned final approach speed changes" },
	{ .type = &asn_DEF_FinalCruiseSpeedAtToD, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Final cruise speed at top of descent" },
	{ .type = &asn_DEF_GrossMass, .format = asn1_format_GrossMass_as_text, .label = "Gross mass" },
	{ .type = &asn_DEF_GroundSpeed, .format = asn1_format_GroundSpeed_as_text, .label = "Ground speed" },
	{ .type = &asn_DEF_GroundSpeedChange, .format = asn1_format_SpeedIndicated_as_text, .label = "Report ground speed changes" },
	{ .type = &asn_DEF_GroundTrack, .format = asn1_format_GroundTrack_as_text, .label = "Ground track" },
	{ .type = &asn_DEF_GroundVector, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Ground vector" },
	{ .type = &asn_DEF_GroundVectorModulus, .format = asn1_format_Modulus_as_text, .label = "Report ground vector" },
	{ .type = &asn_DEF_Heading, .format = asn1_format_GroundTrack_as_text, .label = "Heading" },
	{ .type = &asn_DEF_Ias, .format = asn1_format_SpeedIndicated_as_text, .label = "IAS" },
	{ .type = &asn_DEF_IasTolerance, .format = asn1_format_SpeedIndicated_as_text, .label = "IAS" },
	{ .type = &asn_DEF_IasChange, .format = asn1_format_SpeedIndicated_as_text, .label = "IAS change" },
	{ .type = &asn_DEF_InitialCruiseSpeedAtToC, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Initial cruise speed at top of climb" },
	{ .type = &asn_DEF_LateralFlightManaged, .format = la_asn1_format_any_as_text, .label = "Lateral flight managed" },
	{ .type = &asn_DEF_LateralDeviationChange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report lateral deviation changes" },
	{ .type = &asn_DEF_LateralDeviationOffsetTag, .format = la_asn1_format_label_only_as_text, .label = "Offset tag" /* ? */ },
	{ .type = &asn_DEF_LateralDeviationThresholdLeft, .format = asn1_format_LateralDeviationThreshold_as_text, .label = "Left threshold" },
	{ .type = &asn_DEF_LateralDeviationThresholdRight, .format = asn1_format_LateralDeviationThreshold_as_text, .label = "Right threshold" },
	{ .type = &asn_DEF_LateralType, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Lateral type" },
	{ .type = &asn_DEF_LateralTypeFlyby, .format = asn1_format_CHOICE_icao_as_text, .label = "Fly-by" },
	{ .type = &asn_DEF_LateralTypeFixedRadiusTransition, .format = asn1_format_CHOICE_icao_as_text, .label = "Fixed radius transition" },
	{ .type = &asn_DEF_LateralTypeOffsetStart, .format = la_asn1_format_label_only_as_text, .label = "Offset start" },
	{ .type = &asn_DEF_LateralTypeOffsetReached, .format = la_asn1_format_label_only_as_text, .label = "Offset reached" },
	{ .type = &asn_DEF_LateralTypeReturnToParentPathInitiation, .format = la_asn1_format_label_only_as_text, .label = "Return to parent path initiation" },
	{ .type = &asn_DEF_LateralTypeOffsetEnd, .format = la_asn1_format_label_only_as_text, .label = "Offset end" },
	{ .type = &asn_DEF_LateralTypeOffset, .format = la_asn1_format_label_only_as_text, .label = "Offset" },
	{ .type = &asn_DEF_LateralTypeOverfly, .format = la_asn1_format_label_only_as_text, .label = "Overfly" },
	{ .type = &asn_DEF_LateralTypeFlightPlanWayPoint, .format = la_asn1_format_label_only_as_text, .label = "Flight plan waypoint" },
	{ .type = &asn_DEF_LateralTypeFollowedByDisco, .format = la_asn1_format_label_only_as_text, .label = "Followed by discontinuity" },
	{ .type = &asn_DEF_LevelChange, .format = asn1_format_LevelFeet_as_text, .label = "Report level changes exceeding" },
	{ .type = &asn_DEF_LevelConstraint, .format = asn1_format_CHOICE_icao_as_text, .label = "Level constraint" },
	{ .type = &asn_DEF_LevelConstraintQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Level constraint type" },
	{ .type = &asn_DEF_LevelRangeDeviation, .format = asn1_format_CHOICE_icao_as_text, .label = "Report level range deviation" },
	{ .type = &asn_DEF_LevelRangeDeviationBoth, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_LevelRangeDeviationCeiling, .format = asn1_format_LevelFeet_as_text, .label = "Upper limit" },
	{ .type = &asn_DEF_LevelRangeDeviationFloor, .format = asn1_format_LevelFeet_as_text, .label = "Lower limit" },
	{ .type = &asn_DEF_MachAndIas, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_MachNumberChange, .format = asn1_format_SpeedMach_as_text, .label = "Mach number change" },
	{ .type = &asn_DEF_MachNumberTolerance, .format = asn1_format_MachNumberTolerance_as_text, .label = "Mach number" },
	{ .type = &asn_DEF_MetInfo, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Meteo data" },
	{ .type = &asn_DEF_MinMaxIAS, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Min/max IAS" },
	{ .type = &asn_DEF_MinMaxMach, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Min/max Mach" },
	{ .type = &asn_DEF_MinMaxSpeed, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Min/max speed" },
	{ .type = &asn_DEF_MetInfoModulus, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report meteo info" },
	{ .type = &asn_DEF_MetInfoRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report meteo info" },
	{ .type = &asn_DEF_Modulus, .format = asn1_format_Modulus_as_text, .label = "Reporting frequency" },
	{ .type = &asn_DEF_MSLAltitude, .format = asn1_format_LevelFeet_as_text, .label = "Alt (MSL)" },
	{ .type = &asn_DEF_MultipleNavigationalUnitsOperating, .format = la_asn1_format_any_as_text, .label = "Multiple NAV units operating" },
	{ .type = &asn_DEF_NominalSpeed, .format = asn1_format_CHOICE_icao_as_text, .label = "Nominal speed" },
	{ .type = &asn_DEF_PeriodicContractRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ADS-C v2 Periodic Contract Request" },
	{ .type = &asn_DEF_PeriodicReport, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Periodic Report" },
	{ .type = &asn_DEF_PlannedFinalAppSpeedModulus, .format = asn1_format_Modulus_as_text, .label = "Report planned final approach speed" },
	{ .type = &asn_DEF_PredictedGrossMassAtToD, .format = asn1_format_GrossMass_as_text, .label = "Predicted gross mass at top of descent" },
	{ .type = &asn_DEF_ProjectedProfile, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Projected profile" },
	{ .type = &asn_DEF_ProjectedProfileModulus, .format = asn1_format_Modulus_as_text, .label = "Report projected profile" },
	{ .type = &asn_DEF_QNEAltitude, .format = asn1_format_LevelFeet_as_text, .label = "Alt (QNE)" },
	{ .type = &asn_DEF_QNHAltitude, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Alt (QNH)" },
	{ .type = &asn_DEF_RejectDetails, .format = asn1_format_RejectDetails_as_text, .label = "Reject reason" },
	{ .type = &asn_DEF_RNPProfile, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "RNP profile" },
	{ .type = &asn_DEF_RNPProfileModulus, .format = asn1_format_Modulus_as_text, .label = "Report RNP profile" },
	{ .type = &asn_DEF_RNPSegment, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RNP segment" },
	{ .type = &asn_DEF_RNPSegmentEndPoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "End" },
	{ .type = &asn_DEF_RNPSegmentStartPoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Start" },
	{ .type = &asn_DEF_RNPValue, .format = asn1_format_RNPValue_as_text, .label = "RNP value" },
	{ .type = &asn_DEF_RTA, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RTA" },
	{ .type = &asn_DEF_RTASecTolerance, .format = asn1_format_RTASecTolerance_as_text, .label = "Tolerance" },
	{ .type = &asn_DEF_RTAStatus, .format = la_asn1_format_ENUM_as_text, .label = "Status" },
	{ .type = &asn_DEF_RTAStatusData, .format = asn1_format_SEQUENCE_icao_as_text, .label = "RTA status data" },
	{ .type = &asn_DEF_RTAType, .format = la_asn1_format_ENUM_as_text, .label = "Type" },
	{ .type = &asn_DEF_ReportTypeAndPeriodNotSupported, .format = asn1_format_SEQUENCE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_ReportTypeNotSupported, .format = asn1_format_ReportTypeNotSupported_as_text, .label = "Unsupported reports" },
	{ .type = &asn_DEF_ReportingRate, .format = asn1_format_ReportingRate_as_text, .label = "Reporting rate" },
	{ .type = &asn_DEF_SingleLevel, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Single level" },
	{ .type = &asn_DEF_SingleLevelSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Single level sequence" },
	{ .type = &asn_DEF_SpeedConstraint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Speed constraint" },
	{ .type = &asn_DEF_SpeedIASMach, .format = asn1_format_CHOICE_icao_as_text, .label = NULL },
	{ .type = &asn_DEF_SpeedManaged, .format = la_asn1_format_any_as_text, .label = "Speed managed" },
	{ .type = &asn_DEF_SpeedQualifier, .format = la_asn1_format_ENUM_as_text, .label = "Type" },
	{ .type = &asn_DEF_SpeedScheduleBlock, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Block speed schedule" },
	{ .type = &asn_DEF_SpeedScheduleProfile, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Speed schedule profile" },
	{ .type = &asn_DEF_SpeedScheduleProfileModulus, .format = asn1_format_Modulus_as_text, .label = "Report speed schedule profile" },
	{ .type = &asn_DEF_SpeedScheduleSingle, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Single speed schedule" },
	{ .type = &asn_DEF_TimeManaged, .format = la_asn1_format_any_as_text, .label = "Time managed" },
	{ .type = &asn_DEF_TOAComputationTime, .format = asn1_format_Timesec_as_text, .label = "Computation time" },
	{ .type = &asn_DEF_TOARange, .format = asn1_format_SEQUENCE_icao_as_text, .label = "ToA range" },
	{ .type = &asn_DEF_TOARangeEarliestETA, .format = asn1_format_Timesec_as_text, .label = "ETA (earliest)" },
	{ .type = &asn_DEF_TOARangeLatestETA, .format = asn1_format_Timesec_as_text, .label = "ETA (latest)" },
	{ .type = &asn_DEF_TOARangeRequest, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report ToA range" },
	{ .type = &asn_DEF_TOARangeRequestModulus, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report ToA range" },
	{ .type = &asn_DEF_ThreeDPosition, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Position" },
	{ .type = &asn_DEF_Timesec, .format = asn1_format_Timesec_as_text, .label = "Time" },
	{ .type = &asn_DEF_TrajectoryIntentStatus, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Trajectory intent status" },
	{ .type = &asn_DEF_TurbulenceDeviation, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report turbulence deviation" },
	{ .type = &asn_DEF_TurbulenceEDRAverage, .format = asn1_format_TurbulenceEDRValue_as_text, .label = "Average EDR value" },
	{ .type = &asn_DEF_TurbulenceEDRPeak, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Peak EDR value" },
	{ .type = &asn_DEF_TurbulenceEDRValue, .format = asn1_format_TurbulenceEDRValue_as_text, .label = "EDR value" },
	{ .type = &asn_DEF_TurbulenceMinutesInPast, .format = asn1_format_TurbulenceMinutesInThePast_as_text, .label = "Time ago" },
	{ .type = &asn_DEF_TurbulenceObservationWindow, .format = asn1_format_TurbulenceObservationWindow_as_text, .label = "Observation window" },
	{ .type = &asn_DEF_TurbulencePeakThreshold, .format = asn1_format_TurbulenceEDRValue_as_text, .label = "Peak EDR threshold" },
	{ .type = &asn_DEF_TurnRadius, .format = asn1_format_TurnRadius_as_text, .label = "Turn radius" },
	{ .type = &asn_DEF_TurnRadiusNotAvailable, .format = la_asn1_format_label_only_as_text, .label = "Turn radius not available" },
	{ .type = &asn_DEF_VerticalClearanceDeviation, .format = asn1_format_LevelFeet_as_text, .label = "Report vertical clearance deviation exceeding" },
	{ .type = &asn_DEF_VerticalFlightManaged, .format = la_asn1_format_any_as_text, .label = "Vertical flight managed" },
	{ .type = &asn_DEF_VerticalRateDeviation, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Report vertical rate deviation" },
	{ .type = &asn_DEF_VerticalRateDeviationLower, .format = asn1_format_VerticalRateEnglish_as_text, .label = "Lower limit" },
	{ .type = &asn_DEF_VerticalRateDeviationUpper, .format = asn1_format_VerticalRateEnglish_as_text, .label = "Upper limit" },
	{ .type = &asn_DEF_VerticalType, .format = asn1_format_VerticalType_as_text, .label = "Vertical type" },
	{ .type = &asn_DEF_Waypoint, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint" },
	{ .type = &asn_DEF_WaypointName, .format = la_asn1_format_any_as_text, .label = "Wpt name" },
	{ .type = &asn_DEF_WayPointSequence, .format = asn1_format_SEQUENCE_OF_icao_as_text, .label = "Waypoint sequence" },
	{ .type = &asn_DEF_WayPointSequenceElement, .format = asn1_format_SEQUENCE_icao_as_text, .label = "Waypoint data" },
	{ .type = &asn_DEF_WindErrorModelUsed, .format = la_asn1_format_ENUM_as_text, .label = "" },
	{ .type = &asn_DEF_WindQualityFlag, .format = la_asn1_format_ENUM_as_text, .label = "Wind quality flag" },
};

size_t asn1_icao_formatter_table_text_len = sizeof(asn1_icao_formatter_table_text) / sizeof(la_asn1_formatter);

la_asn1_formatter const asn1_acse_formatter_table_text[] = {
	{ .type = &asn_DEF_AARE_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Associate Response" },
	{ .type = &asn_DEF_AARQ_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Associate Request" },
	{ .type = &asn_DEF_ABRT_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Abort" },
	{ .type = &asn_DEF_ABRT_diagnostic, .format = la_asn1_format_ENUM_as_text, .label = "Cause" },
	{ .type = &asn_DEF_ABRT_source  , .format = asn1_format_ABRT_source_as_text, .label = "Source" },
	{ .type = &asn_DEF_ACSE_apdu, .format = asn1_format_CHOICE_acse_as_text, .label = NULL },
	{ .type = &asn_DEF_AE_qualifier, .format = asn1_format_CHOICE_acse_as_text, .label = NULL },
	{ .type = &asn_DEF_AE_qualifier_form2, .format = la_asn1_format_any_as_text, .label = "AE qualifier" },
	{ .type = &asn_DEF_AP_title, .format = asn1_format_CHOICE_acse_as_text, .label = NULL },
	{ .type = &asn_DEF_AP_title_form2, .format = la_asn1_format_any_as_text, .label = "AP title" },
	{ .type = &asn_DEF_Application_context_name, .format = la_asn1_format_any_as_text, .label = "Application context name" },
	{ .type = &asn_DEF_Associate_result , .format = asn1_format_Associate_result_as_text, .label = "Associate result" },
	{ .type = &asn_DEF_Release_request_reason , .format = asn1_format_Release_request_reason_as_text, .label = "Reason" },
	{ .type = &asn_DEF_Release_response_reason , .format = asn1_format_Release_response_reason_as_text, .label = "Reason" },
	{ .type = &asn_DEF_RLRE_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Release Response" },
	{ .type = &asn_DEF_RLRQ_apdu, .format = asn1_format_SEQUENCE_acse_as_text, .label = "X.227 ACSE Release Request" },
	// Supported in ATN ULCS, but not included in text output
	{ .type = &asn_DEF_ACSE_requirements  , .format = NULL, .label = NULL },
	{ .type = &asn_DEF_Associate_source_diagnostic , .format = NULL, .label = NULL },
	{ .type = &asn_DEF_Association_information, .format = NULL, .label = NULL },
	{ .type = &asn_DEF_Authentication_value , .format = NULL, .label = NULL }
	// Not supported in ATN ULCS
	// { .type = &asn_DEF_AE_invocation_identifier, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_AE_qualifier_form1, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_AP_invocation_identifier, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_AP_title_form1, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_Application_context_name_list, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_AttributeTypeAndValue, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_EXTERNALt, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_Implementation_data, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_Mechanism_name, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_Name, .format = NULL, .label = NULL },
	// { .type = &asn_DEF_RDNSequence , .format = NULL, .label = NULL },
	// { .type = &asn_DEF_RelativeDistinguishedName, .format = NULL, .label = NULL },
};

size_t asn1_acse_formatter_table_text_len = sizeof(asn1_acse_formatter_table_text) / sizeof(la_asn1_formatter);
