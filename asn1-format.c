/*
 *  This file is a part of dumpvdl2
 *
 *  Copyright (c) 2017-2018 Tomasz Lemiech <szpajder@gmail.com>
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

#include <search.h>				// lfind()
#include "asn1/ATCDownlinkMessage.h"		// ATCDownlinkMessage_t and dependencies
#include "asn1/ATCUplinkMessage.h"		// ATCUplinkMessage_t and dependencies
#include "asn1/CMAircraftMessage.h"		// AircraftMessge_t and dependencies
#include "asn1/CMGroundMessage.h"		// CMGroundMessage_t and dependencies
#include "asn1/ProtectedGroundPDUs.h"		// ProtectedGroundPDUs_t
#include "asn1/constr_CHOICE.h"			// _fetch_present_idx()
#include "asn1/asn_SET_OF.h"			// _A_CSET_FROM_VOID()
#include "tlv.h"				// dict_search()
#include "dumpvdl2.h"				// XCALLOC
#include "asn1-format.h"

// FIXME: bsearch
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
	{ ATCUplinkMsgElementId_PR_uM64DistanceSpecifiedDirection, "OFFSET [specifiedDistance] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM65PositionDistanceSpecifiedDirection, "AT [position] OFFSET [specifiedDistance] [direction] OF ROUTE" },
	{ ATCUplinkMsgElementId_PR_uM66TimeDistanceSpecifiedDirection, "AT [time] OFFSET [specifiedDistance] [direction] OF ROUTE" },
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
	{ ATCUplinkMsgElementId_PR_uM82DistanceSpecifiedDirection, "CLEARED TO DEVIATE UP TO [specifiedDistance] [direction] OF ROUTE" },
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
	{ ATCUplinkMsgElementId_PR_uM152DistanceSpecifiedDirection, "WHEN CAN YOU ACCEPT [specifiedDistance] [direction] OFFSET" },
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
	{ ATCDownlinkMsgElementId_PR_dM15DistanceSpecifiedDirection, "REQUEST OFFSET [specifiedDistance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM16PositionDistanceSpecifiedDirection, "AT [position] REQUEST OFFSET [specifiedDistance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM17TimeDistanceSpecifiedDirection, "AT [time] REQUEST OFFSET [specifiedDistance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM18Speed, "REQUEST [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM19SpeedSpeed, "REQUEST [speed] TO [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM20NULL, "REQUEST VOICE CONTACT" },
	{ ATCDownlinkMsgElementId_PR_dM21Frequency, "REQUEST VOICE CONTACT [frequency]" },
	{ ATCDownlinkMsgElementId_PR_dM22Position, "REQUEST DIRECT TO [position]" },
	{ ATCDownlinkMsgElementId_PR_dM23ProcedureName, "REQUEST [procedureName]" },
	{ ATCDownlinkMsgElementId_PR_dM24RouteClearance, "REQUEST CLEARANCE [routeClearance]" },
	{ ATCDownlinkMsgElementId_PR_dM25ClearanceType, "REQUEST [clearanceType] CLEARANCE" },
	{ ATCDownlinkMsgElementId_PR_dM26PositionRouteClearance, "REQUEST WEATHER DEVIATION TO [position] VIA [routeClearance]" },
	{ ATCDownlinkMsgElementId_PR_dM27DistanceSpecifiedDirection, "REQUEST WEATHER DEVIATION UP TO [specifiedDistance] [direction] OF ROUTE" },
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
	{ ATCDownlinkMsgElementId_PR_dM60DistanceSpecifiedDirection, "OFFSETTING [specifiedDistance] [direction] OF ROUTE" },
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
	{ ATCDownlinkMsgElementId_PR_dM80DistanceSpecifiedDirection, "DEVIATING UP TO [specifiedDistance] [direction] OF ROUTE" },
	{ ATCDownlinkMsgElementId_PR_dM81LevelTime, "WE CAN ACCEPT [level] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM82Level, "WE CANNOT ACCEPT [level]" },
	{ ATCDownlinkMsgElementId_PR_dM83SpeedTime, "WE CAN ACCEPT [speed] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM84Speed, "WE CANNOT ACCEPT [speed]" },
	{ ATCDownlinkMsgElementId_PR_dM85DistanceSpecifiedDirectionTime, "WE CAN ACCEPT [specifiedDistance] [direction] AT [time]" },
	{ ATCDownlinkMsgElementId_PR_dM86DistanceSpecifiedDirection, "WE CANNOT ACCEPT [specifiedDistance] [direction]" },
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

/*******************
 * Helper functions
 *******************/

static char const *value2enum(asn_TYPE_descriptor_t *td, long const value) {
	if(td == NULL) return NULL;
	asn_INTEGER_enum_map_t const *enum_map = INTEGER_map_value2enum(td->specifics, value);
	if(enum_map == NULL) return NULL;
	return enum_map->enum_name;
}

static void _format_CHOICE(FILE *stream, dict const * const choice_labels, asn_TYPE_descriptor_t *td, void const *sptr, int indent) {
	asn_CHOICE_specifics_t *specs = (asn_CHOICE_specifics_t *)td->specifics;
	int present = _fetch_present_idx(sptr, specs->pres_offset, specs->pres_size);
	if(choice_labels != NULL) {
		char *descr = dict_search(choice_labels, present);
		if(descr != NULL) {
			IFPRINTF(stream, indent, "%s\n", descr);
		} else {
			IFPRINTF(stream, indent, "<no description for CHOICE value %d>\n", present);
		}
		indent++;
	}
	if(present > 0 && present <= td->elements_count) {
		asn_TYPE_member_t *elm = &td->elements[present-1];
		void const *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				IFPRINTF(stream, indent, "%s: <not present>\n", elm->name);
				return;
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}

		output_asn1(stream, elm->type, memb_ptr, indent);
	} else {
		IFPRINTF(stream, indent, "-- %s: value %d out of range\n", td->name, present);
	}
}

static void _format_SEQUENCE_OF(FILE *stream, asn_TYPE_descriptor_t *td, void const *sptr, int indent) {
	const asn_anonymous_set_ *list = _A_CSET_FROM_VOID(sptr);
	for(int i = 0; i < list->count; i++) {
		const void *element = list->array[i];
		if(element == NULL) {
			continue;
		}
		output_asn1(stream, td, element, indent);
	}
}

static void _format_INTEGER_with_unit(FILE *stream, char const * const label, asn_TYPE_descriptor_t *td,
	void const *sptr, int indent, char const * const unit, double multiplier, int decimal_places) {
	CAST_PTR(val, long *, sptr);
	IFPRINTF(stream, indent, "%s: %.*f%s\n", label, decimal_places, (double)(*val) * multiplier, unit);
}

/************************
 * ASN.1 type formatters
 ************************/

ASN1_FORMATTER_PROTOTYPE(asn1_format_any) {
	if(label != NULL) {
		IFPRINTF(stream, indent, "%s: ", label);
	} else {
		IFPRINTF(stream, indent, "%s", "");
	}
	asn_fprint(stream, td, sptr, 1);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_NULL) {
	// NOOP
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_CHOICE) {
// If there is a label - print it and indent the contents by one level.
// If there is no label, then treat this as an anonymous CHOICE and do
// not indent the contents. This makes the CHOICE invisible, and the output
// less cluttered.
	if(label != NULL) {
		IFPRINTF(stream, indent, "%s:\n", label);
		indent++;
	}
	_format_CHOICE(stream, NULL, td, sptr, indent);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_ENUM) {
	long const value = *(long const *)sptr;
	char const *s = value2enum(td, value);
	if(s != NULL) {
		IFPRINTF(stream, indent, "%s: %s\n", label, s);
	} else {
		IFPRINTF(stream, indent, "%s: %ld\n", label, value);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_SEQUENCE) {
	if(label != NULL) {
		IFPRINTF(stream, indent, "%s:\n", label);
		indent++;
	}
	for(int edx = 0; edx < td->elements_count; edx++) {
		asn_TYPE_member_t *elm = &td->elements[edx];
		const void *memb_ptr;

		if(elm->flags & ATF_POINTER) {
			memb_ptr = *(const void * const *)((const char *)sptr + elm->memb_offset);
			if(!memb_ptr) {
				continue;
			}
		} else {
			memb_ptr = (const void *)((const char *)sptr + elm->memb_offset);
		}
		output_asn1(stream, elm->type, memb_ptr, indent);
	}
}

// TODO: _format_SEQUENCE_OF
ASN1_FORMATTER_PROTOTYPE(asn1_format_ATCDownlinkMessageData) {
	CAST_PTR(dmd, ATCDownlinkMessageData_t *, sptr);
	IFPRINTF(stream, indent, "%s:\n", label);
	indent++;
	for(int i = 0; i < dmd->elementIds.list.count; i++) {
		const void *element = dmd->elementIds.list.array[i];
		if(element == NULL) {
			continue;
		}
		output_asn1(stream, &asn_DEF_ATCDownlinkMsgElementId, element, indent);
	}
	if(dmd->constrainedData != NULL) {
		output_asn1(stream, &asn_DEF_RouteClearance, dmd->constrainedData, indent);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_ATCDownlinkMsgElementId) {
	_format_CHOICE(stream, ATCDownlinkMsgElementId_labels, td, sptr, indent);
}

// TODO: _format_SEQUENCE_OF
ASN1_FORMATTER_PROTOTYPE(asn1_format_ATCUplinkMessageData) {
	CAST_PTR(umd, ATCUplinkMessageData_t *, sptr);
	IFPRINTF(stream, indent, "%s:\n", label);
	indent++;
	for(int i = 0; i < umd->elementIds.list.count; i++) {
		const void *element = umd->elementIds.list.array[i];
		if(element == NULL) {
			continue;
		}
		output_asn1(stream, &asn_DEF_ATCUplinkMsgElementId, element, indent);
	}
	if(umd->constrainedData != NULL) {
		output_asn1(stream, &asn_DEF_RouteClearance, umd->constrainedData, indent);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_ATCUplinkMsgElementId) {
	_format_CHOICE(stream, ATCUplinkMsgElementId_labels, td, sptr, indent);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Code) {
	CAST_PTR(code, Code_t *, sptr);
	long **cptr = code->list.array;
	IFPRINTF(stream, indent, "%s: %ld%ld%ld%ld\n",
		label,
		*cptr[0],
		*cptr[1],
		*cptr[2],
		*cptr[3]
	);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_DateTime) {
	CAST_PTR(dtg, DateTime_t *, sptr);
	Date_t *d = &dtg->date;
	Time_t *t = &dtg->time;
	IFPRINTF(stream, indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld\n", label,
		d->year, d->month, d->day,
		t->hours, t->minutes);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_DateTimeGroup) {
	CAST_PTR(dtg, DateTimeGroup_t *, sptr);
	Date_t *d = &dtg->date;
	Timehhmmss_t *t = &dtg->timehhmmss;
	IFPRINTF(stream, indent, "%s: %04ld-%02ld-%02ld %02ld:%02ld:%02ld\n", label,
		d->year, d->month, d->day,
		t->hoursminutes.hours, t->hoursminutes.minutes, t->seconds);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Deg) {
	_format_INTEGER_with_unit(stream, label, td, sptr, indent, " deg", 1, 0);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Frequencyvhf) {
	_format_INTEGER_with_unit(stream, label, td, sptr, indent, " MHz", 0.005, 3);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Latitude) {
	CAST_PTR(lat, Latitude_t *, sptr);
	long const ldir = lat->latitudeDirection;
	char const *ldir_name = value2enum(&asn_DEF_LatitudeDirection, ldir);
	switch(lat->latitudeType.present) {
	case LatitudeType_PR_latitudeDegrees:
		IFPRINTF(stream, indent, "%s:   %02ld %s\n",
			label,
			lat->latitudeType.choice.latitudeDegrees,
			ldir_name
		);
		break;
	case LatitudeType_PR_latitudeDegreesMinutes:
		IFPRINTF(stream, indent, "%s:   %02ld %02ld' %s\n",
			label,
			lat->latitudeType.choice.latitudeDegreesMinutes.latitudeWholeDegrees,
			lat->latitudeType.choice.latitudeDegreesMinutes.minutesLatLon,
			ldir_name
		);
		break;
	case LatitudeType_PR_latitudeDMS:
		IFPRINTF(stream, indent, "%s:   %02ld %02ld'%02ld\" %s\n",
			label,
			lat->latitudeType.choice.latitudeDMS.latitudeWholeDegrees,
			lat->latitudeType.choice.latitudeDMS.latlonWholeMinutes,
			lat->latitudeType.choice.latitudeDMS.secondsLatLon,
			ldir_name
		);
		break;
	case LatitudeType_PR_NOTHING:
	default:
		IFPRINTF(stream, indent, "%s: none\n", label);
		break;
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Longitude) {
	CAST_PTR(lon, Longitude_t *, sptr);
	long const ldir = lon->longitudeDirection;
	char const *ldir_name = value2enum(&asn_DEF_LongitudeDirection, ldir);
	switch(lon->longitudeType.present) {
	case LongitudeType_PR_longitudeDegrees:
		IFPRINTF(stream, indent, "%s: %03ld %s\n",
			label,
			lon->longitudeType.choice.longitudeDegrees,
			ldir_name
		);
		break;
	case LongitudeType_PR_longitudeDegreesMinutes:
		IFPRINTF(stream, indent, "%s: %03ld %02ld' %s\n",
			label,
			lon->longitudeType.choice.longitudeDegreesMinutes.longitudeWholeDegrees,
			lon->longitudeType.choice.longitudeDegreesMinutes.minutesLatLon,
			ldir_name
		);
		break;
	case LongitudeType_PR_longitudeDMS:
		IFPRINTF(stream, indent, "%s: %03ld %02ld'%02ld\" %s\n",
			label,
			lon->longitudeType.choice.longitudeDMS.longitudeWholeDegrees,
			lon->longitudeType.choice.longitudeDMS.latLonWholeMinutes,
			lon->longitudeType.choice.longitudeDMS.secondsLatLon,
			ldir_name
		);
		break;
	case LongitudeType_PR_NOTHING:
	default:
		IFPRINTF(stream, indent, "%s: none\n", label);
		break;
	}
}

// FIXME: change rDP type to something unique - then we'll be able to replace
// this routine with asn1_format_SEQUENCE
// FIXME: replace these cryptic labels with something human-readable
ASN1_FORMATTER_PROTOTYPE(asn1_format_LongTsap) {
	CAST_PTR(tsap, LongTsap_t *, sptr);
	IFPRINTF(stream, indent, "%s:\n", label);
	indent++;
	asn1_format_any(stream, "RDP", &asn_DEF_OCTET_STRING, &tsap->rDP, indent);
	output_asn1(stream, &asn_DEF_ShortTsap, &tsap->shortTsap, indent);
}

// FIXME: change aRS type to something unique - then we'll be able to replace
// this routine with asn1_format_SEQUENCE
// FIXME: replace these cryptic labels with something human-readable
ASN1_FORMATTER_PROTOTYPE(asn1_format_ShortTsap) {
	CAST_PTR(tsap, ShortTsap_t *, sptr);
	IFPRINTF(stream, indent, "%s:\n", label);
	indent++;
	if(tsap->aRS != NULL) {
		asn1_format_any(stream, "ARS", &asn_DEF_OCTET_STRING, tsap->aRS, indent);
	}
	asn1_format_any(stream, "locSysNselTsel", &asn_DEF_OCTET_STRING, &tsap->locSysNselTsel, indent);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_SpeedIndicated) {
	_format_INTEGER_with_unit(stream, label, td, sptr, indent, " kts", 1, 0);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_SpeedMach) {
	_format_INTEGER_with_unit(stream, label, td, sptr, indent, "", 0.001, 2);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_Time) {
	CAST_PTR(t, Time_t *, sptr);
	IFPRINTF(stream, indent, "%s: %02ld:%02ld\n", label, t->hours, t->minutes);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_UnitName) {
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
	IFPRINTF(stream, indent, "%s: %s, %s, %s\n", label, fdes, fname ? fname : "", ffun_name);
	XFREE(fdes);
	XFREE(fname);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_VerticalRateEnglish) {
	_format_INTEGER_with_unit(stream, label, td, sptr, indent, " ft/min", 10, 0);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_VerticalRateMetric) {
	_format_INTEGER_with_unit(stream, label, td, sptr, indent, " m/min", 10, 0);
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_CMLogonRequest) {
	CAST_PTR(cmlr, CMLogonRequest_t *, sptr);
	IFPRINTF(stream, indent, "%s:\n", label);
	indent++;
	output_asn1(stream, &asn_DEF_AircraftFlightIdentification, &cmlr->aircraftFlightIdentification, indent);
	output_asn1(stream, &asn_DEF_LongTsap, &cmlr->cMLongTSAP, indent);
	if(cmlr->groundInitiatedApplications != NULL) {
		IFPRINTF(stream, indent, "%s:\n", "Ground-initiated applications");
		_format_SEQUENCE_OF(stream, &asn_DEF_AEQualifierVersionAddress, cmlr->groundInitiatedApplications, indent+1);
	}
	if(cmlr->airOnlyInitiatedApplications != NULL) {
		IFPRINTF(stream, indent, "%s:\n", "Air-initiated applications");
		_format_SEQUENCE_OF(stream, &asn_DEF_AEQualifierVersion, cmlr->airOnlyInitiatedApplications, indent+1);
	}
	if(cmlr->facilityDesignation != NULL) {
		output_asn1(stream, &asn_DEF_FacilityDesignation, cmlr->facilityDesignation, indent+1);
	}
// Can't use output_asn1 here - we have two different labels for the same type.
	if(cmlr->airportDeparture != NULL) {
		asn1_format_any(stream, "Departure airport", &asn_DEF_Airport, cmlr->airportDeparture, indent);
	}
	if(cmlr->airportDestination != NULL) {
		asn1_format_any(stream, "Destination airport", &asn_DEF_Airport, cmlr->airportDestination, indent);
	}
	if(cmlr->dateTimeDepartureETD != NULL) {
		asn1_format_DateTime(stream, "Departure time", &asn_DEF_DateTime, cmlr->dateTimeDepartureETD, indent);
	}
}

ASN1_FORMATTER_PROTOTYPE(asn1_format_CMLogonResponse) {
	CAST_PTR(cmlr, CMLogonResponse_t *, sptr);
	IFPRINTF(stream, indent, "%s:\n", label);
	indent++;
	if(cmlr->airInitiatedApplications != NULL) {
		IFPRINTF(stream, indent, "%s:\n", "Air-initiated applications");
		_format_SEQUENCE_OF(stream, &asn_DEF_AEQualifierVersionAddress, cmlr->airInitiatedApplications, indent+1);
	}
	if(cmlr->groundOnlyInitiatedApplications != NULL) {
		IFPRINTF(stream, indent, "%s:\n", "Ground-initiated applications");
		_format_SEQUENCE_OF(stream, &asn_DEF_AEQualifierVersion, cmlr->groundOnlyInitiatedApplications, indent+1);
	}
}


static asn_formatter_t const asn1_formatter_table[] = {
// atn-cpdlc.asn1
	{ .type = &asn_DEF_ATCDownlinkMessage, .format = &asn1_format_SEQUENCE, .label = "CPDLC Downlink Message" },
	{ .type = &asn_DEF_ATCMessageHeader, .format = &asn1_format_SEQUENCE, .label = "Header" },
	{ .type = &asn_DEF_ATCDownlinkMessageData, .format = &asn1_format_ATCDownlinkMessageData, .label = "Message data" },
	{ .type = &asn_DEF_ATCDownlinkMsgElementId, .format = &asn1_format_ATCDownlinkMsgElementId, .label = NULL },
	{ .type = &asn_DEF_ATCUplinkMessage, .format = &asn1_format_SEQUENCE, .label = "CPDLC Uplink Message" },
	{ .type = &asn_DEF_ATCUplinkMessageData, .format = &asn1_format_ATCUplinkMessageData, .label = "Message data" },
	{ .type = &asn_DEF_ATCUplinkMsgElementId, .format = &asn1_format_ATCUplinkMsgElementId, .label = NULL },
	{ .type = &asn_DEF_Code, .format = &asn1_format_Code, .label = "Code" },
	{ .type = &asn_DEF_DateTimeGroup, .format = &asn1_format_DateTimeGroup, .label = "Timestamp" },
	{ .type = &asn_DEF_Degrees, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_DegreesMagnetic, .format = &asn1_format_Deg, .label = "Degrees (magnetic)" },
	{ .type = &asn_DEF_DegreesTrue, .format = &asn1_format_Deg, .label = "Degrees (true)" },
	{ .type = &asn_DEF_Direction, .format = &asn1_format_ENUM, .label = "Direction" },
	{ .type = &asn_DEF_DirectionDegrees, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_ErrorInformation, .format = &asn1_format_ENUM, .label = "Error information" },
	{ .type = &asn_DEF_Facility, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_FacilityDesignation, .format = &asn1_format_any, .label = "Facility designation" },
	{ .type = &asn_DEF_Fix, .format = &asn1_format_any, .label = "Fix" },
	{ .type = &asn_DEF_FixName, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_FreeText, .format = &asn1_format_any, .label = NULL },
	{ .type = &asn_DEF_Frequency, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_Frequencyvhf, .format = &asn1_format_Frequencyvhf, .label = "VHF" },
	{ .type = &asn_DEF_Latitude, .format = &asn1_format_Latitude, .label = "Latitude" },
	{ .type = &asn_DEF_LatitudeDegreesMinutesSeconds, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_LatitudeDirection, .format = &asn1_format_ENUM, .label = "Direction" },
	{ .type = &asn_DEF_LatitudeType, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_LatitudeLongitude, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_Level, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_LevelFlightLevel, .format = &asn1_format_any, .label = "Flight level" },
	{ .type = &asn_DEF_LevelPosition, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_LevelTime, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_LevelType, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_LogicalAck, .format = &asn1_format_ENUM, .label = "Logical ACK" },
	{ .type = &asn_DEF_Longitude, .format = &asn1_format_Longitude, .label = "Longitude" },
	{ .type = &asn_DEF_LongitudeDegreesMinutesSeconds, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_LongitudeDirection, .format = &asn1_format_ENUM, .label = "Direction" },
	{ .type = &asn_DEF_LongitudeType, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_MsgIdentificationNumber, .format = &asn1_format_any, .label = "Msg ID" },
	{ .type = &asn_DEF_MsgReferenceNumber, .format = &asn1_format_any, .label = "Msg Ref" },
	{ .type = &asn_DEF_NULL, .format = &asn1_format_NULL, .label = NULL },
	{ .type = &asn_DEF_Navaid, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_NavaidName, .format = &asn1_format_any, .label = "Navaid" },
	{ .type = &asn_DEF_PMCPDLCProviderAbortReason, .format = &asn1_format_ENUM, .label = "CPDLC Provider Abort Reason" },
	{ .type = &asn_DEF_PMCPDLCUserAbortReason, .format = &asn1_format_ENUM, .label = "CPDLC User Abort Reason" },
	{ .type = &asn_DEF_Position, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_PositionLevel, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_ProtectedGroundPDUs, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_Speed, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_SpeedIndicated, .format = &asn1_format_SpeedIndicated, .label = "Indicated airspeed" },
	{ .type = &asn_DEF_SpeedMach, .format = &asn1_format_SpeedMach, .label = "Mach speed" },
	{ .type = &asn_DEF_Time, .format = &asn1_format_Time, .label = "Time" },
	{ .type = &asn_DEF_UnitNameFrequency, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_UnitName, .format = &asn1_format_UnitName, .label = "Unit name" },
	{ .type = &asn_DEF_VerticalRate, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_VerticalRateEnglish, .format = &asn1_format_VerticalRateEnglish, .label = "Vertical rate" },
	{ .type = &asn_DEF_VerticalRateMetric, .format = &asn1_format_VerticalRateMetric, .label = "Vertical rate" },
// atn-cm.asn1
	{ .type = &asn_DEF_APAddress, .format = &asn1_format_CHOICE, .label = "AP Address" },
	{ .type = &asn_DEF_AEQualifier, .format = &asn1_format_any, .label = "Application Entity Qualifier" },
	{ .type = &asn_DEF_AEQualifierVersion, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_AEQualifierVersionAddress, .format = &asn1_format_SEQUENCE, .label = NULL },
	{ .type = &asn_DEF_AircraftFlightIdentification, .format = &asn1_format_any, .label = "Flight ID" },
	{ .type = &asn_DEF_CMAircraftMessage, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_CMGroundMessage, .format = &asn1_format_CHOICE, .label = NULL },
	{ .type = &asn_DEF_CMLogonRequest, .format = &asn1_format_CMLogonRequest, .label = "Context Management - Logon Request" },
	{ .type = &asn_DEF_CMLogonResponse, .format = &asn1_format_CMLogonResponse, .label = "Context Management - Logon Response" },
	{ .type = &asn_DEF_LongTsap, .format = &asn1_format_LongTsap, .label = "Long TSAP" },
	{ .type = &asn_DEF_OCTET_STRING, .format = &asn1_format_any, .label = NULL },
	{ .type = &asn_DEF_ShortTsap, .format = &asn1_format_ShortTsap, .label = "Short TSAP" },
	{ .type = &asn_DEF_VersionNumber, .format = &asn1_format_any, .label = "Version number" },
};

static size_t asn1_formatter_table_len = sizeof(asn1_formatter_table) / sizeof(asn_formatter_t);

static int compare_fmtr(const void *k, const void *m) {
	asn_formatter_t *memb = (asn_formatter_t *)m;
	return(k == memb->type ? 0 : 1);
}

void output_asn1(FILE *stream, asn_TYPE_descriptor_t *td, const void *sptr, int indent) {
	if(td == NULL || sptr == NULL) return;
	asn_formatter_t *formatter = lfind(td, asn1_formatter_table, &asn1_formatter_table_len,
		sizeof(asn_formatter_t), &compare_fmtr);
	if(formatter != NULL) {
		(*formatter->format)(stream, formatter->label, td, sptr, indent);
	} else {
		IFPRINTF(stream, indent, "-- Formatter for type %s not found, ASN.1 dump follows:\n", td->name);
		if(indent > 0) {
			IFPRINTF(stream, indent * 4, "%s", "");	// asn_fprint does not indent the first line
		}
		asn_fprint(stream, td, sptr, indent+1);
		IFPRINTF(stream, indent, "%s", "-- ASN.1 dump end\n");
	}
}


