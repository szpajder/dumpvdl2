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
#include "asn1-util.h"                          // asn_formatter_t, asn1_output()
#include "asn1-format-common.h"                 // common formatters and helper functions

// forward declarations
void asn1_output_icao_as_text(la_vstring *vstr, asn_TYPE_descriptor_t *td, const void *sptr, int indent);
void asn1_output_acse_as_text(la_vstring *vstr, asn_TYPE_descriptor_t *td, const void *sptr, int indent);

static dict const Associate_result_labels[] = {
	{ .id = Associate_result_accepted, .val = "accept" },
	{ .id = Associate_result_rejected_permanent, .val = "reject (permanent)" },
	{ .id = Associate_result_rejected_transient, .val = "reject (transient)" },
	{ .id = 0, .val = NULL }
};

static dict const Release_request_reason_labels[] = {
	{ .id = Release_request_reason_normal, .val = "normal" },
	{ .id = Release_request_reason_urgent, .val = "urgent" },
	{ .id = Release_request_reason_user_defined, .val = "user defined" },
	{ .id = 0, .val = NULL }
};

static dict const Release_response_reason_labels[] = {
	{ .id = Release_response_reason_normal, .val = "normal" },
	{ .id = Release_response_reason_not_finished, .val = "not finished" },
	{ .id = Release_response_reason_user_defined, .val = "user defined" },
	{ .id = 0, .val = NULL }
};

static dict const ABRT_source_labels[] = {
	{ .id = ABRT_source_acse_service_user, .val = "user" },
	{ .id = ABRT_source_acse_service_provider, .val = "provider" },
	{ .id = 0, .val = NULL }
};

static dict const ATCUplinkMsgElementId_labels[] = {
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

static dict const VerticalType_bit_labels[] = {
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

static dict const ReportTypeNotSupported_bit_labels[] = {
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

static dict const EPPLimitations_bit_labels[] = {
	{ 0, "requested distance tolerance not supported" },
	{ 1, "requested level tolerance not supported" },
	{ 2, "requested time tolerance not supported" },
	{ 3, "requested speed tolerance not supported" },
	{ 0, NULL }
};

static dict const EventTypeNotSupported_bit_labels[] = {
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

static dict const EmergencyUrgencyStatus_bit_labels[] = {
	{ 0, "emergency" },
	{ 1, "reserved0" },
	{ 2, "unlawful-interference" },
	{ 3, "reserved1" },
	{ 4, "reserved2" },
	{ 5, "emergency-cancelled" },
	{ 0, NULL }

};
static dict const ATCDownlinkMsgElementId_labels[] = {
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

static GByteArray *_stringify_ShortTsap(GByteArray *array, ShortTsap_t *tsap) {
	if(tsap->aRS != NULL) {
		array = g_byte_array_append(array, tsap->aRS->buf, tsap->aRS->size);
	}
	array = g_byte_array_append(array, tsap->locSysNselTsel.buf, tsap->locSysNselTsel.size);
	return array;
}

/************************
 * ASN.1 type formatters
 ************************/

static ASN1_FORMATTER_PROTOTYPE(asn1_format_SEQUENCE_acse) {
	_format_SEQUENCE(vstr, label, &asn1_output_acse_as_text, td, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_CHOICE_acse) {
	_format_CHOICE(vstr, label, NULL, &asn1_output_acse_as_text, td, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Associate_result) {
	UNUSED(td);
	_format_INTEGER_as_ENUM(vstr, label, Associate_result_labels, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Release_request_reason) {
	UNUSED(td);
	_format_INTEGER_as_ENUM(vstr, label, Release_request_reason_labels, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Release_response_reason) {
	UNUSED(td);
	_format_INTEGER_as_ENUM(vstr, label, Release_response_reason_labels, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ABRT_source) {
	UNUSED(td);
	_format_INTEGER_as_ENUM(vstr, label, ABRT_source_labels, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_CHOICE_icao) {
	_format_CHOICE(vstr, label, NULL, &asn1_output_icao_as_text, td, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_SEQUENCE_icao) {
	_format_SEQUENCE(vstr, label, &asn1_output_icao_as_text, td, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_SEQUENCE_OF_icao) {
	_format_SEQUENCE_OF(vstr, label, &asn1_output_icao_as_text, td, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ATCDownlinkMsgElementId) {
	_format_CHOICE(vstr, label, ATCDownlinkMsgElementId_labels, &asn1_output_icao_as_text, td, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ATCUplinkMsgElementId) {
	_format_CHOICE(vstr, label, ATCUplinkMsgElementId_labels, &asn1_output_icao_as_text, td, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Code) {
	UNUSED(td);
	CAST_PTR(code, Code_t *, sptr);
	long **cptr = code->list.array;
	LA_ISPRINTF(vstr, indent, "%s: %ld%ld%ld%ld\n",
			label,
			*cptr[0],
			*cptr[1],
			*cptr[2],
			*cptr[3]
			);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_DateTime) {
	UNUSED(td);
	CAST_PTR(dtg, DateTime_t *, sptr);
	Date_t *d = &dtg->date;
	Time_t *t = &dtg->time;
	LA_ISPRINTF(vstr, indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld\n", label,
			d->year, d->month, d->day,
			t->hours, t->minutes);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_DateTimeGroup) {
	UNUSED(td);
	CAST_PTR(dtg, DateTimeGroup_t *, sptr);
	Date_t *d = &dtg->date;
	Timehhmmss_t *t = &dtg->timehhmmss;
	LA_ISPRINTF(vstr, indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n", label,
			d->year, d->month, d->day,
			t->hoursminutes.hours, t->hoursminutes.minutes, t->seconds);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Time) {
	UNUSED(td);
	CAST_PTR(t, Time_t *, sptr);
	LA_ISPRINTF(vstr, indent, "%s: %02ld:%02ld\n", label, t->hours, t->minutes);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Latitude) {
	UNUSED(td);
	CAST_PTR(lat, Latitude_t *, sptr);
	long const ldir = lat->latitudeDirection;
	char const *ldir_name = value2enum(&asn_DEF_LatitudeDirection, ldir);
	switch(lat->latitudeType.present) {
		case LatitudeType_PR_latitudeDegrees:
			LA_ISPRINTF(vstr, indent, "%s:   %02ld %s\n",
					label,
					lat->latitudeType.choice.latitudeDegrees,
					ldir_name
					);
			break;
		case LatitudeType_PR_latitudeDegreesMinutes:
			LA_ISPRINTF(vstr, indent, "%s:   %02ld %05.2f' %s\n",
					label,
					lat->latitudeType.choice.latitudeDegreesMinutes.latitudeWholeDegrees,
					lat->latitudeType.choice.latitudeDegreesMinutes.minutesLatLon / 100.0,
					ldir_name
					);
			break;
		case LatitudeType_PR_latitudeDMS:
			LA_ISPRINTF(vstr, indent, "%s:   %02ld %02ld' %02ld\" %s\n",
					label,
					lat->latitudeType.choice.latitudeDMS.latitudeWholeDegrees,
					lat->latitudeType.choice.latitudeDMS.latlonWholeMinutes,
					lat->latitudeType.choice.latitudeDMS.secondsLatLon,
					ldir_name
					);
			break;
		case LatitudeType_PR_NOTHING:
		default:
			LA_ISPRINTF(vstr, indent, "%s: none\n", label);
			break;
	}
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Longitude) {
	UNUSED(td);
	CAST_PTR(lon, Longitude_t *, sptr);
	long const ldir = lon->longitudeDirection;
	char const *ldir_name = value2enum(&asn_DEF_LongitudeDirection, ldir);
	switch(lon->longitudeType.present) {
		case LongitudeType_PR_longitudeDegrees:
			LA_ISPRINTF(vstr, indent, "%s: %03ld %s\n",
					label,
					lon->longitudeType.choice.longitudeDegrees,
					ldir_name
					);
			break;
		case LongitudeType_PR_longitudeDegreesMinutes:
			LA_ISPRINTF(vstr, indent, "%s: %03ld %05.2f' %s\n",
					label,
					lon->longitudeType.choice.longitudeDegreesMinutes.longitudeWholeDegrees,
					lon->longitudeType.choice.longitudeDegreesMinutes.minutesLatLon / 100.0,
					ldir_name
					);
			break;
		case LongitudeType_PR_longitudeDMS:
			LA_ISPRINTF(vstr, indent, "%s: %03ld %02ld' %02ld\" %s\n",
					label,
					lon->longitudeType.choice.longitudeDMS.longitudeWholeDegrees,
					lon->longitudeType.choice.longitudeDMS.latLonWholeMinutes,
					lon->longitudeType.choice.longitudeDMS.secondsLatLon,
					ldir_name
					);
			break;
		case LongitudeType_PR_NOTHING:
		default:
			LA_ISPRINTF(vstr, indent, "%s: none\n", label);
			break;
	}
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_AltimeterEnglish) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " inHg", 0.01, 2);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_AltimeterMetric) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " hPa", 0.1, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_DepartureMinimumInterval) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " min", 0.1, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_DistanceKm) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " km", 0.25, 2);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_DistanceNm) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " nm", 0.1, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Humidity) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, "%%", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_DistanceEnglish) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " nm", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_DistanceMetric) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " km", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Frequencyvhf) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " MHz", 0.005, 3);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Frequencyuhf) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " MHz", 0.025, 3);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Frequencyhf) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " kHz", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_LegTime) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " min", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_LevelFeet) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " ft", 10, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_LevelFlightLevelMetric) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " m", 10, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Meters) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " m", 1, 0);
}

// RejectDetails is a CHOICE whose all values are NULLs.  Aliasing them all to
// unique types just to print them with asn1_format_label_only would be an
// unnecessary overengineering.  Handling all values in a single routine is
// simpler, albeit less elegant at first glance.
static ASN1_FORMATTER_PROTOTYPE(asn1_format_RejectDetails) {
	UNUSED(td);
	CAST_PTR(det, RejectDetails_t *, sptr);
	if(label != NULL) {
		LA_ISPRINTF(vstr, indent, "%s: ", label);
	}
	switch(det->present) {
		case RejectDetails_PR_aDS_service_unavailable:
			la_vstring_append_sprintf(vstr, "ADS service unavailable\n");
			break;
		case RejectDetails_PR_undefined_reason:
			la_vstring_append_sprintf(vstr, "undefined reason\n");
			break;
		case RejectDetails_PR_maximum_capacity_exceeded:
			la_vstring_append_sprintf(vstr, "max. capacity exceeded\n");
			break;
		case RejectDetails_PR_reserved:
			la_vstring_append_sprintf(vstr, "(reserved)\n");
			break;
		case RejectDetails_PR_waypoint_in_request_not_on_the_route:
			la_vstring_append_sprintf(vstr, "requested waypoint not on the route\n");
			break;
		case RejectDetails_PR_aDS_contract_not_supported:
			la_vstring_append_sprintf(vstr, "ADS contract not supported\n");
			break;
		case RejectDetails_PR_noneOfReportTypesSupported:
			la_vstring_append_sprintf(vstr, "none of report types supported\n");
			break;
		case RejectDetails_PR_noneOfEventTypesSupported:
			la_vstring_append_sprintf(vstr, "none of event types supported\n");
			break;
		case RejectDetails_PR_NOTHING:
		default:
			la_vstring_append_sprintf(vstr, "none\n");
	}
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ReportingRate) {
	UNUSED(td);
	CAST_PTR(rate, ReportingRate_t *, sptr);
	switch(rate->present) {
		case ReportingRate_PR_reporting_time_seconds_scale:
			_format_INTEGER_with_unit(vstr, label, td,
					&rate->choice.reporting_time_seconds_scale, indent, " sec", 1, 0);
			break;
		case ReportingRate_PR_reporting_time_minutes_scale:
			_format_INTEGER_with_unit(vstr, label, td,
					&rate->choice.reporting_time_minutes_scale, indent, " min", 1, 0);
			break;
		default:
			break;
	}
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_RTASecTolerance) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " sec", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_RTATolerance) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " min", 0.1, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Feet) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " ft", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_SpeedMetric) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " km/h", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_SpeedEnglish) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " kts", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_SpeedIndicated) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " kts", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_SpeedMach) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, "", 0.001, 3);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Temperature) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " C", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_VerticalRateEnglish) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " ft/min", 10, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_VerticalRateMetric) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " m/min", 10, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_LongTsap) {
	UNUSED(td);
	CAST_PTR(tsap, LongTsap_t *, sptr);
	GByteArray *tmparray = g_byte_array_new();
	tmparray = g_byte_array_append(tmparray, tsap->rDP.buf, tsap->rDP.size);
	tmparray = _stringify_ShortTsap(tmparray, &tsap->shortTsap);

	LA_ISPRINTF(vstr, indent, "%s: ", label);
	octet_string_with_ascii_format_text(vstr,
			&(octet_string_t){ .buf = tmparray->data, .len = tmparray->len },
			0);
	EOL(vstr);
	g_byte_array_free(tmparray, TRUE);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ShortTsap) {
	UNUSED(td);
	CAST_PTR(tsap, ShortTsap_t *, sptr);
	GByteArray *tmparray = g_byte_array_new();
	tmparray = _stringify_ShortTsap(tmparray, tsap);
	LA_ISPRINTF(vstr, indent, "%s: ", label);
	octet_string_with_ascii_format_text(vstr,
			&(octet_string_t){ .buf = tmparray->data, .len = tmparray->len },
			0);
	EOL(vstr);
	g_byte_array_free(tmparray, TRUE);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_UnitName) {
	UNUSED(td);
	CAST_PTR(un, UnitName_t *, sptr);
	char *fdes = XCALLOC(un->facilityDesignation.size + 1, sizeof(char));
	snprintf(fdes, un->facilityDesignation.size + 1, "%s", un->facilityDesignation.buf);
	char *fname = NULL;
	FacilityName_t *fn = un->facilityName;
	if(fn != NULL) {
		fname = XCALLOC(fn->size + 1, sizeof(char));
		snprintf(fname, fn->size + 1, "%s", fn->buf);
	}
	long const ffun = un->facilityFunction;
	char const *ffun_name = value2enum(&asn_DEF_FacilityFunction, ffun);
	LA_ISPRINTF(vstr, indent, "%s: %s, %s, %s\n", label, fdes, fname ? fname : "", ffun_name);
	XFREE(fdes);
	XFREE(fname);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ADSv2DateTimeGroup) {
	UNUSED(td);
	CAST_PTR(dtg, ADSv2DateTimeGroup_t *, sptr);
	Date_t *d = &dtg->date;
	Timesec_t *t = &dtg->time;
	LA_ISPRINTF(vstr, indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n", label,
			d->year, d->month, d->day,
			t->hours, t->minutes, t->seconds);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_EstimatedPositionUncertainty) {
	CAST_PTR(epu, EstimatedPositionUncertainty_t *, sptr);
	if(*epu == 9900) {
		LA_ISPRINTF(vstr, indent, "%s: complete-loss\n", label);
	} else {
		_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " nm", 0.01, 2);
	}
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ADSv2Latitude) {
	UNUSED(td);
	CAST_PTR(lat, ADSv2Latitude_t *, sptr);
	long const ldir = lat->direction;
	char const *ldir_name = value2enum(&asn_DEF_LatitudeDirection, ldir);
	LA_ISPRINTF(vstr, indent, "%s:  %02ld %02ld' %04.1f\" %s\n",
			label,
			lat->degrees,
			lat->minutes,
			lat->seconds / 10.0,
			ldir_name
			);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ADSv2Longitude) {
	UNUSED(td);
	CAST_PTR(lon, ADSv2Longitude_t *, sptr);
	long const ldir = lon->direction;
	char const *ldir_name = value2enum(&asn_DEF_LongitudeDirection, ldir);
	LA_ISPRINTF(vstr, indent, "%s: %03ld %02ld' %04.1f\" %s\n",
			label,
			lon->degrees,
			lon->minutes,
			lon->seconds / 10.0,
			ldir_name
			);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ADSAircraftPDUs) {
	UNUSED(td);
	UNUSED(label);
	CAST_PTR(apdus, ADSAircraftPDUs_t *, sptr);
	// Omit the timestamp for brevity, print the PDU only
	asn1_output_icao_as_text(vstr, &asn_DEF_ADSAircraftPDU, &apdus->adsAircraftPdu, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ADSv2Temperature) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " C", 0.25, 2);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ADSv2WindSpeedKts) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " kts", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ADSv2WindSpeedKmh) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " km/h", 2, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_EmergencyUrgencyStatus) {
	UNUSED(td);
	_format_BIT_STRING(vstr, label, EmergencyUrgencyStatus_bit_labels, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_EPPTimeInterval) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " minutes", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_EventTypeNotSupported) {
	UNUSED(td);
	_format_BIT_STRING(vstr, label, EventTypeNotSupported_bit_labels, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_GrossMass) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " kg", 10, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ADSGroundPDUs) {
	UNUSED(td);
	UNUSED(label);
	CAST_PTR(apdus, ADSGroundPDUs_t *, sptr);
	// Omit the timestamp for brevity, print the PDU only
	asn1_output_icao_as_text(vstr, &asn_DEF_ADSGroundPDU, &apdus->adsGroundPdu, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_EPPLimitations) {
	UNUSED(td);
	_format_BIT_STRING(vstr, label, EPPLimitations_bit_labels, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_EPPTolETA) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " min", 0.1, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_EPPTolGCDistance) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " nm", 0.01, 2);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_EPUChangeTolerance) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " nm", 0.01, 2);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_GroundSpeed) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " kts", 0.5, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_GroundTrack) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " deg", 0.05, 2);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_LateralDeviationThreshold) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " nm", 0.1, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_MachNumberTolerance) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, "", 0.01, 2);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Modulus) {
	UNUSED(td);
	CAST_PTR(val, long *, sptr);
	LA_ISPRINTF(vstr, indent, "%s: every %ld reports\n", label, *val);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_ReportTypeNotSupported) {
	UNUSED(td);
	_format_BIT_STRING(vstr, label, ReportTypeNotSupported_bit_labels, sptr, indent);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_RNPValue) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " nm", 0.1, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_TurbulenceEDRValue) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " m^2/s^3", 0.01, 2);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_TurbulenceMinutesInThePast) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " min", 0.5, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_TurbulenceObservationWindow) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " min", 1, 0);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_TurnRadius) {
	_format_INTEGER_with_unit(vstr, label, td, sptr, indent, " nm", 0.1, 1);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_Timesec) {
	UNUSED(td);
	CAST_PTR(t, Timesec_t *, sptr);
	LA_ISPRINTF(vstr, indent, "%s: %02ld:%02ld:%02ld\n", label, t->hours, t->minutes, t->seconds);
}

static ASN1_FORMATTER_PROTOTYPE(asn1_format_VerticalType) {
	UNUSED(td);
	_format_BIT_STRING(vstr, label, VerticalType_bit_labels, sptr, indent);
}

asn_formatter_t const asn1_icao_formatter_table[] = {
	// atn-b1_cpdlc-v1.asn1
	{ .type = &asn_DEF_AircraftAddress, .format = &asn1_format_any, .label = "Aircraft address" },
	{ .type = &asn_DEF_AirInitiatedApplications, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Air-initiated applications" },
	{ .type = &asn_DEF_AirOnlyInitiatedApplications, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Air-only-initiated applications" },
	{ .type = &asn_DEF_Airport, .format = &asn1_format_any, .label = "Airport" },
	{ .type = &asn_DEF_AirportDeparture, .format = &asn1_format_any, .label = "Departure airport" },
	{ .type = &asn_DEF_AirportDestination, .format = &asn1_format_any, .label = "Destination airport" },
	{ .type = &asn_DEF_Altimeter, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_AltimeterEnglish, .format = &asn1_format_AltimeterEnglish, .label = "Altimeter" },
	{ .type = &asn_DEF_AltimeterMetric, .format = &asn1_format_AltimeterMetric, .label = "Altimeter" },
	{ .type = &asn_DEF_ATCDownlinkMessage, .format = &asn1_format_SEQUENCE_icao, .label = "CPDLC Downlink Message" },
	{ .type = &asn_DEF_ATCDownlinkMessageData, .format = &asn1_format_SEQUENCE_icao, .label = "Message data" },
	{ .type = &asn_DEF_ATCDownlinkMsgElementId, .format = &asn1_format_ATCDownlinkMsgElementId, .label = NULL },
	{ .type = &asn_DEF_ATCDownlinkMsgElementIdSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_ATCMessageHeader, .format = &asn1_format_SEQUENCE_icao, .label = "Header" },
	{ .type = &asn_DEF_ATCUplinkMessage, .format = &asn1_format_SEQUENCE_icao, .label = "CPDLC Uplink Message" },
	{ .type = &asn_DEF_ATCUplinkMessageData, .format = &asn1_format_SEQUENCE_icao, .label = "Message data" },
	{ .type = &asn_DEF_ATCUplinkMsgElementId, .format = &asn1_format_ATCUplinkMsgElementId, .label = NULL },
	{ .type = &asn_DEF_ATCUplinkMsgElementIdSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_ATISCode, .format = &asn1_format_any, .label = "ATIS code" },
	{ .type = &asn_DEF_ATSRouteDesignator, .format = &asn1_format_any, .label = "ATS route" },
	{ .type = &asn_DEF_ATWAlongTrackWaypoint, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ATWAlongTrackWaypointSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Along-track waypoints" },
	{ .type = &asn_DEF_ATWDistance, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ATWDistanceTolerance, .format = &asn1_format_ENUM, .label = "ATW Distance Tolerance" },
	{ .type = &asn_DEF_ATWLevel, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ATWLevelSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "ATW Levels" },
	{ .type = &asn_DEF_ATWLevelTolerance, .format = &asn1_format_ENUM, .label = "ATW Level Tolerance" },
	{ .type = &asn_DEF_BlockLevel, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Block level" },
	{ .type = &asn_DEF_ClearanceType, .format = &asn1_format_ENUM, .label = "Clearance type" },
	{ .type = &asn_DEF_Code, .format = &asn1_format_Code, .label = "Code" },
	{ .type = &asn_DEF_ControlledTime, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_DateTimeDepartureETD, .format = &asn1_format_DateTime, .label = "Departure time" },
	{ .type = &asn_DEF_DateTimeGroup, .format = &asn1_format_DateTimeGroup, .label = "Timestamp" },
	{ .type = &asn_DEF_DegreeIncrement, .format = &asn1_format_Deg, .label = "Degree increment" },
	{ .type = &asn_DEF_Degrees, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_DegreesMagnetic, .format = &asn1_format_Deg, .label = "Degrees (magnetic)" },
	{ .type = &asn_DEF_DegreesTrue, .format = &asn1_format_Deg, .label = "Degrees (true)" },
	{ .type = &asn_DEF_DepartureClearance, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_DepartureMinimumInterval, .format = &asn1_format_DepartureMinimumInterval, .label = "Minimum interval of departures" },
	{ .type = &asn_DEF_Direction, .format = &asn1_format_ENUM, .label = "Direction" },
	{ .type = &asn_DEF_DirectionDegrees, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_Distance, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_DistanceKm, .format = &asn1_format_DistanceKm, .label = "Distance" },
	{ .type = &asn_DEF_DistanceNm, .format = &asn1_format_DistanceNm, .label = "Distance" },
	{ .type = &asn_DEF_DistanceSpecified, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedDirection, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedDirectionTime, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_DistanceSpecifiedKm, .format = &asn1_format_DistanceMetric, .label = "Offset" },
	{ .type = &asn_DEF_DistanceSpecifiedNm, .format = &asn1_format_DistanceEnglish, .label = "Offset" },
	{ .type = &asn_DEF_DMVersionNumber, .format = &asn1_format_any, .label = "Version number" },
	{ .type = &asn_DEF_ErrorInformation, .format = &asn1_format_ENUM, .label = "Error information" },
	{ .type = &asn_DEF_Facility, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_FacilityDesignation, .format = &asn1_format_any, .label = "Facility designation" },
	{ .type = &asn_DEF_FacilityDesignationAltimeter, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_FacilityDesignationATISCode, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_FacilityName, .format = &asn1_format_any, .label = "Facility name" },
	{ .type = &asn_DEF_Fix, .format = &asn1_format_any, .label = "Fix" },
	{ .type = &asn_DEF_FixName, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_FlightInformation, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_FreeText, .format = &asn1_format_any, .label = NULL },
	{ .type = &asn_DEF_Frequency, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_Frequencyhf, .format = &asn1_format_Frequencyhf, .label = "HF" },
	{ .type = &asn_DEF_Frequencysatchannel, .format = &asn1_format_any, .label = "Satcom channel" },
	{ .type = &asn_DEF_Frequencyuhf, .format = &asn1_format_Frequencyuhf, .label = "UHF" },
	{ .type = &asn_DEF_Frequencyvhf, .format = &asn1_format_Frequencyvhf, .label = "VHF" },
	{ .type = &asn_DEF_FurtherInstructions, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_GroundInitiatedApplications, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Ground-initiated applications" },
	{ .type = &asn_DEF_GroundOnlyInitiatedApplications, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Ground-only-initiated applications" },
	{ .type = &asn_DEF_Holdatwaypoint, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_HoldatwaypointSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Holding points" },
	{ .type = &asn_DEF_HoldClearance, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_Humidity, .format = &asn1_format_Humidity, .label = "Humidity" },
	{ .type = &asn_DEF_Icing, .format = &asn1_format_ENUM, .label = "Icing" },
	{ .type = &asn_DEF_InterceptCourseFrom, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_InterceptCourseFromSelection, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_InterceptCourseFromSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Intercept courses" },
	{ .type = &asn_DEF_Latitude, .format = &asn1_format_Latitude, .label = "Latitude" },
	{ .type = &asn_DEF_LatitudeDirection, .format = &asn1_format_ENUM, .label = "Direction" },
	{ .type = &asn_DEF_LatitudeLongitude, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LatitudeReportingPoints, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LatitudeType, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_LatLonReportingPoints, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_LegDistance, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_LegDistanceEnglish, .format = &asn1_format_DistanceEnglish, .label = "Leg distance" },
	{ .type = &asn_DEF_LegDistanceMetric, .format = &asn1_format_DistanceMetric, .label = "Leg distance" },
	{ .type = &asn_DEF_LegTime, .format = &asn1_format_LegTime, .label = "Leg time" },
	{ .type = &asn_DEF_LegType, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_Level, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_LevelFeet, .format = &asn1_format_LevelFeet, .label = "Flight level" },
	{ .type = &asn_DEF_LevelFlightLevel, .format = &asn1_format_any, .label = "Flight level" },
	{ .type = &asn_DEF_LevelFlightLevelMetric, .format = &asn1_format_LevelFlightLevelMetric, .label = "Flight level" },
	{ .type = &asn_DEF_LevelLevel, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_LevelMeters, .format = &asn1_format_Meters, .label = "Flight level" },
	{ .type = &asn_DEF_LevelPosition, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LevelProcedureName, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LevelsOfFlight, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_LevelSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LevelSpeedSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LevelTime, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LevelType, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_LogicalAck, .format = &asn1_format_ENUM, .label = "Logical ACK" },
	{ .type = &asn_DEF_Longitude, .format = &asn1_format_Longitude, .label = "Longitude" },
	{ .type = &asn_DEF_LongitudeDirection, .format = &asn1_format_ENUM, .label = "Direction" },
	{ .type = &asn_DEF_LongitudeReportingPoints, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LongitudeType, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_MsgIdentificationNumber, .format = &asn1_format_any, .label = "Msg ID" },
	{ .type = &asn_DEF_MsgReferenceNumber, .format = &asn1_format_any, .label = "Msg Ref" },
	{ .type = &asn_DEF_Navaid, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_NavaidName, .format = &asn1_format_any, .label = "Navaid" },
	{ .type = &asn_DEF_NULL, .format = &asn1_format_NULL, .label = NULL },
	{ .type = &asn_DEF_PersonsOnBoard, .format = &asn1_format_any, .label = "Persons on board" },
	{ .type = &asn_DEF_PlaceBearing, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PlaceBearingDistance, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PlaceBearingPlaceBearing, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_PMCPDLCProviderAbortReason, .format = &asn1_format_ENUM, .label = "CPDLC Provider Abort Reason" },
	{ .type = &asn_DEF_PMCPDLCUserAbortReason, .format = &asn1_format_ENUM, .label = "CPDLC User Abort Reason" },
	{ .type = &asn_DEF_Position, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionDegrees, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionDistanceSpecifiedDirection, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionLevel, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionLevelLevel, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionLevelSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionPosition, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_PositionProcedureName, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionReport, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionRouteClearanceIndex, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionSpeedSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionTime, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionTimeLevel, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionTimeTime, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_PositionUnitNameFrequency, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_Procedure, .format = &asn1_format_any, .label = "Procedure" },
	{ .type = &asn_DEF_ProcedureApproach, .format = &asn1_format_SEQUENCE_icao, .label = "Approach procedure" },
	{ .type = &asn_DEF_ProcedureArrival, .format = &asn1_format_SEQUENCE_icao, .label = "Arrival procedure" },
	{ .type = &asn_DEF_ProcedureDeparture, .format = &asn1_format_SEQUENCE_icao, .label = "Departure procedure" },
	{ .type = &asn_DEF_ProcedureName, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ProcedureTransition, .format = &asn1_format_any, .label = "Procedure transition" },
	{ .type = &asn_DEF_ProcedureType, .format = &asn1_format_ENUM, .label = "Procedure type" },
	{ .type = &asn_DEF_ProtectedAircraftPDUs, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_ProtectedGroundPDUs, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_PublishedIdentifier, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_RemainingFuel, .format = &asn1_format_Time, .label = "Remaining fuel" },
	{ .type = &asn_DEF_RemainingFuelPersonsOnBoard, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ReportingPoints, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_RevisionNumber, .format = &asn1_format_any, .label = "Revision number" },
	{ .type = &asn_DEF_RouteAndLevels, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_RouteClearance, .format = &asn1_format_SEQUENCE_icao, .label = "Route clearance" },
	{ .type = &asn_DEF_RouteClearanceIndex, .format = &asn1_format_any, .label = "Route clearance index" },
	{ .type = &asn_DEF_RouteClearanceSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_RouteInformation, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_RouteInformationAdditional, .format = &asn1_format_SEQUENCE_icao, .label = "Additional route information" },
	{ .type = &asn_DEF_RouteInformationSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Route" },
	{ .type = &asn_DEF_RTARequiredTimeArrival, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_RTARequiredTimeArrivalSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Required arrival times" },
	{ .type = &asn_DEF_RTATime, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_RTATolerance, .format = &asn1_format_RTATolerance, .label = "RTA Tolerance" },
	{ .type = &asn_DEF_Runway, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_RunwayArrival, .format = &asn1_format_SEQUENCE_icao, .label = "Arrival runway" },
	{ .type = &asn_DEF_RunwayConfiguration, .format = &asn1_format_ENUM, .label = "Runway configuration" },
	{ .type = &asn_DEF_RunwayDeparture, .format = &asn1_format_SEQUENCE_icao, .label = "Departure runway" },
	{ .type = &asn_DEF_RunwayDirection, .format = &asn1_format_any, .label = "Runway direction" },
	{ .type = &asn_DEF_RunwayRVR, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_RVR, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_RVRFeet, .format = &asn1_format_Feet, .label = "RVR" },
	{ .type = &asn_DEF_RVRMeters, .format = &asn1_format_Meters, .label = "RVR" },
	{ .type = &asn_DEF_Speed, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_SpeedGround, .format = &asn1_format_SpeedEnglish, .label = "Ground speed" },
	{ .type = &asn_DEF_SpeedGroundMetric, .format = &asn1_format_SpeedMetric, .label = "Ground speed" },
	{ .type = &asn_DEF_SpeedIndicated, .format = &asn1_format_SpeedIndicated, .label = "Indicated airspeed" },
	{ .type = &asn_DEF_SpeedIndicatedMetric, .format = &asn1_format_SpeedMetric, .label = "Indicated airspeed" },
	{ .type = &asn_DEF_SpeedMach, .format = &asn1_format_SpeedMach, .label = "Mach" },
	{ .type = &asn_DEF_SpeedSpeed, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_SpeedTime, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_SpeedTrue, .format = &asn1_format_SpeedEnglish, .label = "True airspeed" },
	{ .type = &asn_DEF_SpeedTrueMetric, .format = &asn1_format_SpeedMetric, .label = "True airspeed" },
	{ .type = &asn_DEF_SpeedType, .format = &asn1_format_ENUM, .label = "Speed type" },
	{ .type = &asn_DEF_SpeedTypeSpeedTypeSpeedType, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_SpeedTypeSpeedTypeSpeedTypeSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_Temperature, .format = &asn1_format_Temperature, .label = "Temperature" },
	{ .type = &asn_DEF_Time, .format = &asn1_format_Time, .label = "Time" },
	{ .type = &asn_DEF_TimeDeparture, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimeDistanceSpecifiedDirection, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimeDistanceToFromPosition, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_Timehhmmss, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimeLevel, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimePosition, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimePositionLevel, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimePositionLevelSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimeSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimeSpeedSpeed, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimeTime, .format = &asn1_format_SEQUENCE_OF_icao, .label = NULL },
	{ .type = &asn_DEF_TimeToFromPosition, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TimeTolerance, .format = &asn1_format_ENUM, .label = "Time tolerance" },
	{ .type = &asn_DEF_TimeUnitNameFrequency, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ToFrom, .format = &asn1_format_ENUM, .label = "To/From" },
	{ .type = &asn_DEF_ToFromPosition, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_TrafficType, .format = &asn1_format_ENUM, .label = "Traffic type" },
	{ .type = &asn_DEF_Turbulence, .format = &asn1_format_ENUM, .label = "Turbulence" },
	{ .type = &asn_DEF_UnitName, .format = &asn1_format_UnitName, .label = "Unit name" },
	{ .type = &asn_DEF_UnitNameFrequency, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_VerticalChange, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_VerticalDirection, .format = &asn1_format_ENUM, .label = "Vertical direction" },
	{ .type = &asn_DEF_VerticalRate, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_VerticalRateEnglish, .format = &asn1_format_VerticalRateEnglish, .label = "Vertical rate" },
	{ .type = &asn_DEF_VerticalRateMetric, .format = &asn1_format_VerticalRateMetric, .label = "Vertical rate" },
	{ .type = &asn_DEF_WaypointSpeedLevel, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_WaypointSpeedLevelSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Waypoints, speeds and levels" },
	{ .type = &asn_DEF_WindDirection, .format = &asn1_format_Deg, .label = "Wind direction" },
	{ .type = &asn_DEF_Winds, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_WindSpeed, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_WindSpeedEnglish, .format = &asn1_format_SpeedEnglish, .label = "Wind speed" },
	{ .type = &asn_DEF_WindSpeedMetric, .format = &asn1_format_SpeedMetric, .label = "Wind speed" },
	// atn-b1_cm.asn1
	{ .type = &asn_DEF_APAddress, .format = &asn1_format_CHOICE_icao, .label = "AP Address" },
	{ .type = &asn_DEF_AEQualifier, .format = &asn1_format_any, .label = "Application Entity Qualifier" },
	{ .type = &asn_DEF_AEQualifierVersion, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_AEQualifierVersionAddress, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ARS, .format = &asn1_format_any, .label = "ARS" },
	{ .type = &asn_DEF_AircraftFlightIdentification, .format = &asn1_format_any, .label = "Flight ID" },
	{ .type = &asn_DEF_CMAbortReason, .format = &asn1_format_ENUM, .label = "ATN Context Management - Abort Reason" },
	{ .type = &asn_DEF_CMAircraftMessage, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_CMGroundMessage, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_CMContactRequest, .format = &asn1_format_SEQUENCE_icao, .label = "ATN Context Management - Contact Request" },
	{ .type = &asn_DEF_CMContactResponse, .format = &asn1_format_ENUM, .label = "ATN Context Management - Contact Response" },
	{ .type = &asn_DEF_CMForwardRequest, .format = &asn1_format_SEQUENCE_icao, .label = "ATN Context Management - Forward Request" },
	{ .type = &asn_DEF_CMForwardResponse, .format = &asn1_format_ENUM, .label = "ATN Context Management - Forward Response" },
	{ .type = &asn_DEF_CMLogonRequest, .format = &asn1_format_SEQUENCE_icao, .label = "ATN Context Management - Logon Request" },
	{ .type = &asn_DEF_CMLogonResponse, .format = &asn1_format_SEQUENCE_icao, .label = "ATN Context Management - Logon Response" },
	{ .type = &asn_DEF_CMUpdate, .format = &asn1_format_SEQUENCE_icao, .label = "ATN Context Management - Update" },
	{ .type = &asn_DEF_LocSysNselTsel, .format = &asn1_format_any, .label = "LOC/SYS/NSEL/TSEL" },
	{ .type = &asn_DEF_LongTsap, .format = &asn1_format_LongTsap, .label = "Long TSAP" },
	{ .type = &asn_DEF_OCTET_STRING, .format = &asn1_format_any, .label = NULL },
	{ .type = &asn_DEF_RDP, .format = &asn1_format_any, .label = "RDP" },
	{ .type = &asn_DEF_ShortTsap, .format = &asn1_format_ShortTsap, .label = "Short TSAP" },
	{ .type = &asn_DEF_VersionNumber, .format = &asn1_format_any, .label = "Version number" },
	// atn-b1_pmadsc.asn1
	{ .type = &asn_DEF_ADSAircraftPDU, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_ADSAircraftPDUs, .format = &asn1_format_ADSAircraftPDUs, .label = NULL },
	{ .type = &asn_DEF_ADSGroundPDU, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_ADSGroundPDUs, .format = &asn1_format_ADSGroundPDUs, .label = NULL },
	{ .type = &asn_DEF_CancelAllContracts, .format = &asn1_format_label_only, .label = "ADS-C v2 Cancel All Contracts" },
	{ .type = &asn_DEF_CancelContract, .format = &asn1_format_CHOICE_icao, .label = "ADS-C v2 Cancel Contract" },
	{ .type = &asn_DEF_CancelPositiveAcknowledgement, .format = &asn1_format_ENUM, .label = "ADS-C v2 Cancel ACK" },
	{ .type = &asn_DEF_CancelRejectReason, .format = &asn1_format_SEQUENCE_icao, .label = "ADS-C v2 Cancel NAK" },
	{ .type = &asn_DEF_ProviderAbortReason, .format = &asn1_format_ENUM, .label = "ADS-C v2 Provider Abort" },
	{ .type = &asn_DEF_RejectReason, .format = &asn1_format_ENUM, .label = "Reject reason" },
	{ .type = &asn_DEF_RequestType, .format = &asn1_format_ENUM, .label = "Request type" },
	{ .type = &asn_DEF_UserAbortReason, .format = &asn1_format_ENUM, .label = "ADS-C v2 User Abort" },
	// atn-b2_adsc_v2.asn1
	{ .type = &asn_DEF_AAISAvailability, .format = &asn1_format_any, .label = "AAIS available" },
	{ .type = &asn_DEF_ADSAccept, .format = &asn1_format_CHOICE_icao, .label = "ADS-C v2 Contract Request Accept" },
	{ .type = &asn_DEF_ADSDataReport, .format = &asn1_format_SEQUENCE_icao, .label = "Report data" },
	{ .type = &asn_DEF_ADSEmergencyUrgencyStatus, .format = &asn1_format_EmergencyUrgencyStatus, .label = "Emergency/urgency status" },
	{ .type = &asn_DEF_ADSNonCompliance, .format = &asn1_format_SEQUENCE_icao, .label = "ADS-C v2 Non-Compliance Notification" },
	{ .type = &asn_DEF_ADSPositiveAcknowledgement, .format = &asn1_format_SEQUENCE_icao, .label = "ADS-C v2 ACK" },
	{ .type = &asn_DEF_ADSReject, .format = &asn1_format_SEQUENCE_icao, .label = "ADS-C v2 Reject" },
	{ .type = &asn_DEF_ADSReport, .format = &asn1_format_CHOICE_icao, .label = "ADS-C v2 Report" },
	{ .type = &asn_DEF_ADSRequestContract, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_ADSv2DateTimeGroup, .format = &asn1_format_ADSv2DateTimeGroup, .label = "Timestamp" },
	{ .type = &asn_DEF_ADSv2Latitude, .format = &asn1_format_ADSv2Latitude, .label = "Lat" },
	{ .type = &asn_DEF_ADSv2LatitudeLongitude, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ADSv2Level, .format = &asn1_format_LevelFeet, .label = "Alt" },
	{ .type = &asn_DEF_ADSv2Longitude, .format = &asn1_format_ADSv2Longitude, .label = "Lon" },
	{ .type = &asn_DEF_ADSv2RequestType, .format = &asn1_format_ENUM, .label = "Request type" },
	{ .type = &asn_DEF_ADSv2Temperature, .format = &asn1_format_ADSv2Temperature, .label = "Temperature" },
	{ .type = &asn_DEF_ADSv2Turbulence, .format = &asn1_format_SEQUENCE_icao, .label = "Turbulence" },
	{ .type = &asn_DEF_ADSv2VerticalRate, .format = &asn1_format_VerticalRateEnglish, .label = "Vertical rate" },
	{ .type = &asn_DEF_ADSv2WindSpeed, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_ADSv2WindSpeedKmh, .format = &asn1_format_ADSv2WindSpeedKmh, .label = "Wind speed" },
	{ .type = &asn_DEF_ADSv2WindSpeedKts, .format = &asn1_format_ADSv2WindSpeedKts, .label = "Wind speed" },
	{ .type = &asn_DEF_ATSUListHiPrio, .format = &asn1_format_any, .label = "High priority" },
	{ .type = &asn_DEF_ATSUListMedPrio, .format = &asn1_format_any, .label = "Medium priority" },
	{ .type = &asn_DEF_ATSUListLoPrio, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Low priority" },
	{ .type = &asn_DEF_AirVector, .format = &asn1_format_SEQUENCE_icao, .label = "Air vector" },
	{ .type = &asn_DEF_AirVectorModulus, .format = &asn1_format_Modulus, .label = "Report air vector" },
	{ .type = &asn_DEF_Airspeed, .format = &asn1_format_CHOICE_icao, .label = "Airspeed" },
	{ .type = &asn_DEF_AirspeedChange, .format = &asn1_format_SEQUENCE_icao, .label = "Report airspeed changes" },
	{ .type = &asn_DEF_AirspeedChangeTolerance, .format = &asn1_format_SEQUENCE_icao, .label = "Airspeed" },
	{ .type = &asn_DEF_AirspeedRangeChange, .format = &asn1_format_SEQUENCE_icao, .label = "Report airspeed range changes" },
	{ .type = &asn_DEF_ClimbSpeed, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Climb speed" },
	{ .type = &asn_DEF_ConnectedATSUList, .format = &asn1_format_SEQUENCE_icao, .label = "Connected ATSU list" },
	{ .type = &asn_DEF_ContractDetailsNotSupporting, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_ContractNumber, .format = &asn1_format_any, .label = "Contract number" },
	{ .type = &asn_DEF_DCRAirVector, .format = &asn1_format_label_only, .label = "Report air vector" },
	{ .type = &asn_DEF_DCRGroundVector, .format = &asn1_format_label_only, .label = "Report ground vector" },
	{ .type = &asn_DEF_DCRPlannedFinalApproachSpeed, .format = &asn1_format_label_only, .label = "Report planned final approach speed" },
	{ .type = &asn_DEF_DCRProjectedProfile, .format = &asn1_format_label_only, .label = "Report projected profile" },
	{ .type = &asn_DEF_DCRRNPProfile, .format = &asn1_format_label_only, .label = "Report RNP profile" },
	{ .type = &asn_DEF_DCRSpeedScheduleProfile, .format = &asn1_format_label_only, .label = "Report speed schedule profile" },
	{ .type = &asn_DEF_DemandContractRequest, .format = &asn1_format_SEQUENCE_icao, .label = "ADS-C v2 Demand Contract Request" },
	{ .type = &asn_DEF_DemandReport, .format = &asn1_format_SEQUENCE_icao, .label = "On-demand Report" },
	{ .type = &asn_DEF_ECRRNPNotMet, .format = &asn1_format_label_only, .label = "Report when RNP not met" },
	{ .type = &asn_DEF_ECRRTAStatusChange, .format = &asn1_format_label_only, .label = "Report RTA status changes" },
	{ .type = &asn_DEF_ECRWaypointChange, .format = &asn1_format_label_only, .label = "Report waypoint changes" },
	{ .type = &asn_DEF_DescentSpeed, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Descent speed" },
	{ .type = &asn_DEF_EPPEventChange, .format = &asn1_format_SEQUENCE_icao, .label = "Report EPP changes" },
	{ .type = &asn_DEF_EPPFlightPlanChangeRequest, .format = &asn1_format_label_only, .label = "Report EPP flight plan changes" },
	{ .type = &asn_DEF_EPPLevel, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_EPPLimitations, .format = &asn1_format_EPPLimitations, .label = "EPP limitations" },
	{ .type = &asn_DEF_EPPNextWptInHorizonRequest, .format = &asn1_format_label_only, .label = "Report next waypoint in horizon" },
	{ .type = &asn_DEF_EPPTolGCDistance, .format = &asn1_format_EPPTolGCDistance, .label = "Great circle distance" },
	{ .type = &asn_DEF_EPPTolLevel, .format = &asn1_format_LevelFeet, .label = "Altitude" },
	{ .type = &asn_DEF_EPPTolETA, .format = &asn1_format_EPPTolETA, .label = "ETA" },
	{ .type = &asn_DEF_EPPToleranceChange, .format = &asn1_format_SEQUENCE_icao, .label = "Report EPP tolerance changes" },
	{ .type = &asn_DEF_EPPTolerancesValues, .format = &asn1_format_SEQUENCE_icao, .label = "EPP tolerances" },
	{ .type = &asn_DEF_EPPNumWaypoints, .format = &asn1_format_any, .label = "Number of waypoints" },
	{ .type = &asn_DEF_EPPTimeInterval, .format = &asn1_format_EPPTimeInterval, .label = "Time interval" },
	{ .type = &asn_DEF_EPPRequest, .format = &asn1_format_CHOICE_icao, .label = "Report extended projected profile" },
	{ .type = &asn_DEF_EPPWindow, .format = &asn1_format_CHOICE_icao, .label = "EPP window" },
	{ .type = &asn_DEF_EPUChangeTolerance, .format = &asn1_format_EPUChangeTolerance, .label = "Report FoM changes exceeding" },
	{ .type = &asn_DEF_ETA, .format = &asn1_format_Timesec, .label = "ETA" },
	{ .type = &asn_DEF_EstimatedPositionUncertainty, .format = &asn1_format_EstimatedPositionUncertainty, .label = "Estimated position uncertainty" },
	{ .type = &asn_DEF_EventContractRequest, .format = &asn1_format_SEQUENCE_icao, .label = "ADS-C v2 Event Contract Request" },
	{ .type = &asn_DEF_EventReport, .format = &asn1_format_SEQUENCE_icao, .label = "Event Report" },
	{ .type = &asn_DEF_EventTypeNotSupported, .format = &asn1_format_EventTypeNotSupported, .label = "Unsupported events" },
	{ .type = &asn_DEF_EventTypeReported, .format = &asn1_format_ENUM, .label = "Reported event" },
	{ .type = &asn_DEF_ExtendedProjectedProfile, .format = &asn1_format_SEQUENCE_icao, .label = "Extended projected profile" },
	{ .type = &asn_DEF_ExtendedProjectedProfileModulus, .format = &asn1_format_SEQUENCE_icao, .label = "Report extended projected profile" },
	{ .type = &asn_DEF_ExtendedWayPointSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Waypoint sequence" },
	{ .type = &asn_DEF_ExtendedWayPointSequenceElement, .format = &asn1_format_SEQUENCE_icao, .label = "Waypoint data" },
	{ .type = &asn_DEF_FigureOfMerit, .format = &asn1_format_SEQUENCE_icao, .label = "Figure of merit" },
	{ .type = &asn_DEF_FinalApproachSpeedChange, .format = &asn1_format_SpeedIndicated, .label = "Report planned final approach speed changes" },
	{ .type = &asn_DEF_FinalCruiseSpeedAtToD, .format = &asn1_format_SEQUENCE_icao, .label = "Final cruise speed at top of descent" },
	{ .type = &asn_DEF_GrossMass, .format = &asn1_format_GrossMass, .label = "Gross mass" },
	{ .type = &asn_DEF_GroundSpeed, .format = &asn1_format_GroundSpeed, .label = "Ground speed" },
	{ .type = &asn_DEF_GroundSpeedChange, .format = &asn1_format_SpeedIndicated, .label = "Report ground speed changes" },
	{ .type = &asn_DEF_GroundTrack, .format = &asn1_format_GroundTrack, .label = "Ground track" },
	{ .type = &asn_DEF_GroundVector, .format = &asn1_format_SEQUENCE_icao, .label = "Ground vector" },
	{ .type = &asn_DEF_GroundVectorModulus, .format = &asn1_format_Modulus, .label = "Report ground vector" },
	{ .type = &asn_DEF_Heading, .format = &asn1_format_GroundTrack, .label = "Heading" },
	{ .type = &asn_DEF_Ias, .format = &asn1_format_SpeedIndicated, .label = "IAS" },
	{ .type = &asn_DEF_IasTolerance, .format = &asn1_format_SpeedIndicated, .label = "IAS" },
	{ .type = &asn_DEF_IasChange, .format = &asn1_format_SpeedIndicated, .label = "IAS change" },
	{ .type = &asn_DEF_InitialCruiseSpeedAtToC, .format = &asn1_format_SEQUENCE_icao, .label = "Initial cruise speed at top of climb" },
	{ .type = &asn_DEF_LateralFlightManaged, .format = &asn1_format_any, .label = "Lateral flight managed" },
	{ .type = &asn_DEF_LateralDeviationChange, .format = &asn1_format_SEQUENCE_icao, .label = "Report lateral deviation changes" },
	{ .type = &asn_DEF_LateralDeviationOffsetTag, .format = &asn1_format_label_only, .label = "Offset tag" /* ? */ },
	{ .type = &asn_DEF_LateralDeviationThresholdLeft, .format = &asn1_format_LateralDeviationThreshold, .label = "Left threshold" },
	{ .type = &asn_DEF_LateralDeviationThresholdRight, .format = &asn1_format_LateralDeviationThreshold, .label = "Right threshold" },
	{ .type = &asn_DEF_LateralType, .format = &asn1_format_SEQUENCE_icao, .label = "Lateral type" },
	{ .type = &asn_DEF_LateralTypeFlyby, .format = &asn1_format_CHOICE_icao, .label = "Fly-by" },
	{ .type = &asn_DEF_LateralTypeFixedRadiusTransition, .format = &asn1_format_CHOICE_icao, .label = "Fixed radius transition" },
	{ .type = &asn_DEF_LateralTypeOffsetStart, .format = &asn1_format_label_only, .label = "Offset start" },
	{ .type = &asn_DEF_LateralTypeOffsetReached, .format = &asn1_format_label_only, .label = "Offset reached" },
	{ .type = &asn_DEF_LateralTypeReturnToParentPathInitiation, .format = &asn1_format_label_only, .label = "Return to parent path initiation" },
	{ .type = &asn_DEF_LateralTypeOffsetEnd, .format = &asn1_format_label_only, .label = "Offset end" },
	{ .type = &asn_DEF_LateralTypeOffset, .format = &asn1_format_label_only, .label = "Offset" },
	{ .type = &asn_DEF_LateralTypeOverfly, .format = &asn1_format_label_only, .label = "Overfly" },
	{ .type = &asn_DEF_LateralTypeFlightPlanWayPoint, .format = &asn1_format_label_only, .label = "Flight plan waypoint" },
	{ .type = &asn_DEF_LateralTypeFollowedByDisco, .format = &asn1_format_label_only, .label = "Followed by discontinuity" },
	{ .type = &asn_DEF_LevelChange, .format = &asn1_format_LevelFeet, .label = "Report level changes exceeding" },
	{ .type = &asn_DEF_LevelConstraint, .format = &asn1_format_CHOICE_icao, .label = "Level constraint" },
	{ .type = &asn_DEF_LevelConstraintQualifier, .format = &asn1_format_ENUM, .label = "Level constraint type" },
	{ .type = &asn_DEF_LevelRangeDeviation, .format = &asn1_format_CHOICE_icao, .label = "Report level range deviation" },
	{ .type = &asn_DEF_LevelRangeDeviationBoth, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_LevelRangeDeviationCeiling, .format = &asn1_format_LevelFeet, .label = "Upper limit" },
	{ .type = &asn_DEF_LevelRangeDeviationFloor, .format = &asn1_format_LevelFeet, .label = "Lower limit" },
	{ .type = &asn_DEF_MachAndIas, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_MachNumberChange, .format = &asn1_format_SpeedMach, .label = "Mach number change" },
	{ .type = &asn_DEF_MachNumberTolerance, .format = &asn1_format_MachNumberTolerance, .label = "Mach number" },
	{ .type = &asn_DEF_MetInfo, .format = &asn1_format_SEQUENCE_icao, .label = "Meteo data" },
	{ .type = &asn_DEF_MinMaxIAS, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Min/max IAS" },
	{ .type = &asn_DEF_MinMaxMach, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Min/max Mach" },
	{ .type = &asn_DEF_MinMaxSpeed, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Min/max speed" },
	{ .type = &asn_DEF_MetInfoModulus, .format = &asn1_format_SEQUENCE_icao, .label = "Report meteo info" },
	{ .type = &asn_DEF_MetInfoRequest, .format = &asn1_format_SEQUENCE_icao, .label = "Report meteo info" },
	{ .type = &asn_DEF_Modulus, .format = &asn1_format_Modulus, .label = "Reporting frequency" },
	{ .type = &asn_DEF_MSLAltitude, .format = &asn1_format_LevelFeet, .label = "Alt (MSL)" },
	{ .type = &asn_DEF_MultipleNavigationalUnitsOperating, .format = &asn1_format_any, .label = "Multiple NAV units operating" },
	{ .type = &asn_DEF_NominalSpeed, .format = &asn1_format_CHOICE_icao, .label = "Nominal speed" },
	{ .type = &asn_DEF_PeriodicContractRequest, .format = &asn1_format_SEQUENCE_icao, .label = "ADS-C v2 Periodic Contract Request" },
	{ .type = &asn_DEF_PeriodicReport, .format = &asn1_format_SEQUENCE_icao, .label = "Periodic Report" },
	{ .type = &asn_DEF_PlannedFinalAppSpeedModulus, .format = &asn1_format_Modulus, .label = "Report planned final approach speed" },
	{ .type = &asn_DEF_PredictedGrossMassAtToD, .format = &asn1_format_GrossMass, .label = "Predicted gross mass at top of descent" },
	{ .type = &asn_DEF_ProjectedProfile, .format = &asn1_format_SEQUENCE_icao, .label = "Projected profile" },
	{ .type = &asn_DEF_ProjectedProfileModulus, .format = &asn1_format_Modulus, .label = "Report projected profile" },
	{ .type = &asn_DEF_QNEAltitude, .format = &asn1_format_LevelFeet, .label = "Alt (QNE)" },
	{ .type = &asn_DEF_QNHAltitude, .format = &asn1_format_SEQUENCE_icao, .label = "Alt (QNH)" },
	{ .type = &asn_DEF_RejectDetails, .format = &asn1_format_RejectDetails, .label = "Reject reason" },
	{ .type = &asn_DEF_RNPProfile, .format = &asn1_format_SEQUENCE_OF_icao, .label = "RNP profile" },
	{ .type = &asn_DEF_RNPProfileModulus, .format = &asn1_format_Modulus, .label = "Report RNP profile" },
	{ .type = &asn_DEF_RNPSegment, .format = &asn1_format_SEQUENCE_icao, .label = "RNP segment" },
	{ .type = &asn_DEF_RNPSegmentEndPoint, .format = &asn1_format_SEQUENCE_icao, .label = "End" },
	{ .type = &asn_DEF_RNPSegmentStartPoint, .format = &asn1_format_SEQUENCE_icao, .label = "Start" },
	{ .type = &asn_DEF_RNPValue, .format = &asn1_format_RNPValue, .label = "RNP value" },
	{ .type = &asn_DEF_RTA, .format = &asn1_format_SEQUENCE_icao, .label = "RTA" },
	{ .type = &asn_DEF_RTASecTolerance, .format = &asn1_format_RTASecTolerance, .label = "Tolerance" },
	{ .type = &asn_DEF_RTAStatus, .format = &asn1_format_ENUM, .label = "Status" },
	{ .type = &asn_DEF_RTAStatusData, .format = &asn1_format_SEQUENCE_icao, .label = "RTA status data" },
	{ .type = &asn_DEF_RTAType, .format = &asn1_format_ENUM, .label = "Type" },
	{ .type = &asn_DEF_ReportTypeAndPeriodNotSupported, .format = &asn1_format_SEQUENCE_icao, .label = NULL },
	{ .type = &asn_DEF_ReportTypeNotSupported, .format = &asn1_format_ReportTypeNotSupported, .label = "Unsupported reports" },
	{ .type = &asn_DEF_ReportingRate, .format = &asn1_format_ReportingRate, .label = "Reporting rate" },
	{ .type = &asn_DEF_SingleLevel, .format = &asn1_format_SEQUENCE_icao, .label = "Single level" },
	{ .type = &asn_DEF_SingleLevelSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Single level sequence" },
	{ .type = &asn_DEF_SpeedConstraint, .format = &asn1_format_SEQUENCE_icao, .label = "Speed constraint" },
	{ .type = &asn_DEF_SpeedIASMach, .format = &asn1_format_CHOICE_icao, .label = NULL },
	{ .type = &asn_DEF_SpeedManaged, .format = &asn1_format_any, .label = "Speed managed" },
	{ .type = &asn_DEF_SpeedQualifier, .format = &asn1_format_ENUM, .label = "Type" },
	{ .type = &asn_DEF_SpeedScheduleBlock, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Block speed schedule" },
	{ .type = &asn_DEF_SpeedScheduleProfile, .format = &asn1_format_SEQUENCE_icao, .label = "Speed schedule profile" },
	{ .type = &asn_DEF_SpeedScheduleProfileModulus, .format = &asn1_format_Modulus, .label = "Report speed schedule profile" },
	{ .type = &asn_DEF_SpeedScheduleSingle, .format = &asn1_format_SEQUENCE_icao, .label = "Single speed schedule" },
	{ .type = &asn_DEF_TimeManaged, .format = &asn1_format_any, .label = "Time managed" },
	{ .type = &asn_DEF_TOAComputationTime, .format = &asn1_format_Timesec, .label = "Computation time" },
	{ .type = &asn_DEF_TOARange, .format = &asn1_format_SEQUENCE_icao, .label = "ToA range" },
	{ .type = &asn_DEF_TOARangeEarliestETA, .format = &asn1_format_Timesec, .label = "ETA (earliest)" },
	{ .type = &asn_DEF_TOARangeLatestETA, .format = &asn1_format_Timesec, .label = "ETA (latest)" },
	{ .type = &asn_DEF_TOARangeRequest, .format = &asn1_format_SEQUENCE_icao, .label = "Report ToA range" },
	{ .type = &asn_DEF_TOARangeRequestModulus, .format = &asn1_format_SEQUENCE_icao, .label = "Report ToA range" },
	{ .type = &asn_DEF_ThreeDPosition, .format = &asn1_format_SEQUENCE_icao, .label = "Position" },
	{ .type = &asn_DEF_Timesec, .format = &asn1_format_Timesec, .label = "Time" },
	{ .type = &asn_DEF_TrajectoryIntentStatus, .format = &asn1_format_SEQUENCE_icao, .label = "Trajectory intent status" },
	{ .type = &asn_DEF_TurbulenceDeviation, .format = &asn1_format_SEQUENCE_icao, .label = "Report turbulence deviation" },
	{ .type = &asn_DEF_TurbulenceEDRAverage, .format = &asn1_format_TurbulenceEDRValue, .label = "Average EDR value" },
	{ .type = &asn_DEF_TurbulenceEDRPeak, .format = &asn1_format_SEQUENCE_icao, .label = "Peak EDR value" },
	{ .type = &asn_DEF_TurbulenceEDRValue, .format = &asn1_format_TurbulenceEDRValue, .label = "EDR value" },
	{ .type = &asn_DEF_TurbulenceMinutesInPast, .format = &asn1_format_TurbulenceMinutesInThePast, .label = "Time ago" },
	{ .type = &asn_DEF_TurbulenceObservationWindow, .format = &asn1_format_TurbulenceObservationWindow, .label = "Observation window" },
	{ .type = &asn_DEF_TurbulencePeakThreshold, .format = &asn1_format_TurbulenceEDRValue, .label = "Peak EDR threshold" },
	{ .type = &asn_DEF_TurnRadius, .format = &asn1_format_TurnRadius, .label = "Turn radius" },
	{ .type = &asn_DEF_TurnRadiusNotAvailable, .format = &asn1_format_label_only, .label = "Turn radius not available" },
	{ .type = &asn_DEF_VerticalClearanceDeviation, .format = &asn1_format_LevelFeet, .label = "Report vertical clearance deviation exceeding" },
	{ .type = &asn_DEF_VerticalFlightManaged, .format = &asn1_format_any, .label = "Vertical flight managed" },
	{ .type = &asn_DEF_VerticalRateDeviation, .format = &asn1_format_SEQUENCE_icao, .label = "Report vertical rate deviation" },
	{ .type = &asn_DEF_VerticalRateDeviationLower, .format = &asn1_format_VerticalRateEnglish, .label = "Lower limit" },
	{ .type = &asn_DEF_VerticalRateDeviationUpper, .format = &asn1_format_VerticalRateEnglish, .label = "Upper limit" },
	{ .type = &asn_DEF_VerticalType, .format = &asn1_format_VerticalType, .label = "Vertical type" },
	{ .type = &asn_DEF_Waypoint, .format = &asn1_format_SEQUENCE_icao, .label = "Waypoint" },
	{ .type = &asn_DEF_WaypointName, .format = &asn1_format_any, .label = "Wpt name" },
	{ .type = &asn_DEF_WayPointSequence, .format = &asn1_format_SEQUENCE_OF_icao, .label = "Waypoint sequence" },
	{ .type = &asn_DEF_WayPointSequenceElement, .format = &asn1_format_SEQUENCE_icao, .label = "Waypoint data" },
	{ .type = &asn_DEF_WindErrorModelUsed, .format = &asn1_format_ENUM, .label = "" },
	{ .type = &asn_DEF_WindQualityFlag, .format = &asn1_format_ENUM, .label = "Wind quality flag" },
};

size_t asn1_icao_formatter_table_len = sizeof(asn1_icao_formatter_table) / sizeof(asn_formatter_t);

void asn1_output_icao_as_text(la_vstring *vstr, asn_TYPE_descriptor_t *td, const void *sptr, int indent) {
	asn1_output(vstr, asn1_icao_formatter_table, asn1_icao_formatter_table_len, td, sptr, indent);
}

asn_formatter_t const asn1_acse_formatter_table[] = {
	{ .type = &asn_DEF_AARE_apdu, .format = asn1_format_SEQUENCE_acse, .label = "X.227 ACSE Associate Response" },
	{ .type = &asn_DEF_AARQ_apdu, .format = asn1_format_SEQUENCE_acse, .label = "X.227 ACSE Associate Request" },
	{ .type = &asn_DEF_ABRT_apdu, .format = asn1_format_SEQUENCE_acse, .label = "X.227 ACSE Abort" },
	{ .type = &asn_DEF_ABRT_diagnostic, .format = asn1_format_ENUM, .label = "Cause" },
	{ .type = &asn_DEF_ABRT_source  , .format = asn1_format_ABRT_source, .label = "Source" },
	{ .type = &asn_DEF_ACSE_apdu, .format = asn1_format_CHOICE_acse, .label = NULL },
	{ .type = &asn_DEF_AE_qualifier, .format = asn1_format_CHOICE_acse, .label = NULL },
	{ .type = &asn_DEF_AE_qualifier_form2, .format = asn1_format_any, .label = "AE qualifier" },
	{ .type = &asn_DEF_AP_title, .format = asn1_format_CHOICE_acse, .label = NULL },
	{ .type = &asn_DEF_AP_title_form2, .format = asn1_format_any, .label = "AP title" },
	{ .type = &asn_DEF_Application_context_name, .format = asn1_format_any, .label = "Application context name" },
	{ .type = &asn_DEF_Associate_result , .format = asn1_format_Associate_result, .label = "Associate result" },
	{ .type = &asn_DEF_Release_request_reason , .format = asn1_format_Release_request_reason, .label = "Reason" },
	{ .type = &asn_DEF_Release_response_reason , .format = asn1_format_Release_response_reason, .label = "Reason" },
	{ .type = &asn_DEF_RLRE_apdu, .format = asn1_format_SEQUENCE_acse, .label = "X.227 ACSE Release Response" },
	{ .type = &asn_DEF_RLRQ_apdu, .format = asn1_format_SEQUENCE_acse, .label = "X.227 ACSE Release Request" },
	// Supported in ATN ULCS, but not included in text output
	{ .type = &asn_DEF_ACSE_requirements  , .format = asn1_format_NULL, .label = NULL },
	{ .type = &asn_DEF_Associate_source_diagnostic , .format = asn1_format_NULL, .label = NULL },
	{ .type = &asn_DEF_Association_information, .format = asn1_format_NULL, .label = NULL },
	{ .type = &asn_DEF_Authentication_value , .format = asn1_format_NULL, .label = NULL }
	// Not supported in ATN ULCS
	// { .type = &asn_DEF_AE_invocation_identifier, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_AE_qualifier_form1, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_AP_invocation_identifier, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_AP_title_form1, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_Application_context_name_list, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_AttributeTypeAndValue, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_EXTERNALt, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_Implementation_data, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_Mechanism_name, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_Name, .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_RDNSequence , .format = asn1_format_NULL, .label = NULL },
	// { .type = &asn_DEF_RelativeDistinguishedName, .format = asn1_format_NULL, .label = NULL },
};

size_t asn1_acse_formatter_table_len = sizeof(asn1_acse_formatter_table) / sizeof(asn_formatter_t);

void asn1_output_acse_as_text(la_vstring *vstr, asn_TYPE_descriptor_t *td, const void *sptr, int indent) {
	asn1_output(vstr, asn1_acse_formatter_table, asn1_acse_formatter_table_len, td, sptr, indent);
}
