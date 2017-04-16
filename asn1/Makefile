export DEBUG ?= 0
CC = gcc
AR = ar
RANLIB = ranlib
CFLAGS = -std=c99 -g -Wall -O3 -DEMIT_ASN_DEBUG=$(DEBUG) -I. -D_XOPEN_SOURCE=500
ASN_MODULE_SOURCES=	\
	Fully-encoded-data.c	\
	PDV-list.c	\
	Transfer-syntax-name.c	\
	Presentation-context-identifier.c	\
	EXTERNALt.c	\
	ACSE-apdu.c	\
	AARQ-apdu.c	\
	AARE-apdu.c	\
	RLRQ-apdu.c	\
	RLRE-apdu.c	\
	ABRT-apdu.c	\
	ABRT-diagnostic.c	\
	ABRT-source.c	\
	ACSE-requirements.c	\
	Application-context-name-list.c	\
	Application-context-name.c	\
	AP-title.c	\
	AE-qualifier.c	\
	AP-title-form1.c	\
	AE-qualifier-form1.c	\
	AP-title-form2.c	\
	AE-qualifier-form2.c	\
	AE-invocation-identifier.c	\
	AP-invocation-identifier.c	\
	Associate-result.c	\
	Associate-source-diagnostic.c	\
	Association-information.c	\
	Implementation-data.c	\
	Mechanism-name.c	\
	Release-request-reason.c	\
	Release-response-reason.c	\
	Authentication-value.c	\
	Name.c	\
	RDNSequence.c	\
	RelativeDistinguishedName.c	\
	AttributeTypeAndValue.c	\
	CMAircraftMessage.c	\
	CMGroundMessage.c	\
	AircraftFlightIdentification.c	\
	Airport.c	\
	APAddress.c	\
	AEQualifier.c	\
	AEQualifierVersion.c	\
	AEQualifierVersionAddress.c	\
	CMAbortReason.c	\
	CMContactRequest.c	\
	CMContactResponse.c	\
	CMForwardRequest.c	\
	CMForwardResponse.c	\
	CMLogonRequest.c	\
	CMLogonResponse.c	\
	CMUpdate.c	\
	Date.c	\
	DateTime.c	\
	Day.c	\
	FacilityDesignation.c	\
	LongTsap.c	\
	Month.c	\
	Response.c	\
	ShortTsap.c	\
	Time.c	\
	Timehours.c	\
	Timeminutes.c	\
	VersionNumber.c	\
	Year.c	\
	GroundPDUs.c	\
	UplinkMessage.c	\
	AircraftPDUs.c	\
	StartDownMessage.c	\
	Mode.c	\
	DownlinkMessage.c	\
	CPDLCUserAbortReason.c	\
	CPDLCProviderAbortReason.c	\
	ProtectedGroundPDUs.c	\
	ProtectedUplinkMessage.c	\
	ATCForwardMessage.c	\
	ForwardHeader.c	\
	ForwardMessage.c	\
	ATCForwardResponse.c	\
	ProtectedAircraftPDUs.c	\
	ProtectedStartDownMessage.c	\
	ProtectedMode.c	\
	ProtectedDownlinkMessage.c	\
	AlgorithmIdentifier.c	\
	CPDLCMessage.c	\
	PMCPDLCUserAbortReason.c	\
	PMCPDLCProviderAbortReason.c	\
	ATCUplinkMessage.c	\
	ATCUplinkMessageData.c	\
	ATCDownlinkMessage.c	\
	ATCDownlinkMessageData.c	\
	ATCMessageHeader.c	\
	MsgIdentificationNumber.c	\
	MsgReferenceNumber.c	\
	LogicalAck.c	\
	ATCUplinkMsgElementId.c	\
	ATCDownlinkMsgElementId.c	\
	AircraftAddress.c	\
	Altimeter.c	\
	AltimeterEnglish.c	\
	AltimeterMetric.c	\
	ATISCode.c	\
	ATSRouteDesignator.c	\
	ATWAlongTrackWaypoint.c	\
	ATWLevel.c	\
	ATWLevelSequence.c	\
	ATWLevelTolerance.c	\
	ATWDistance.c	\
	ATWDistanceTolerance.c	\
	ClearanceType.c	\
	Code.c	\
	CodeOctalDigit.c	\
	ControlledTime.c	\
	DateTimeGroup.c	\
	DegreeIncrement.c	\
	Degrees.c	\
	DegreesMagnetic.c	\
	DegreesTrue.c	\
	DepartureClearance.c	\
	DepartureMinimumInterval.c	\
	Direction.c	\
	DirectionDegrees.c	\
	Distance.c	\
	DistanceKm.c	\
	DistanceNm.c	\
	DistanceSpecified.c	\
	DistanceSpecifiedDirection.c	\
	DistanceSpecifiedDirectionTime.c	\
	DistanceSpecifiedKm.c	\
	DistanceSpecifiedNm.c	\
	ErrorInformation.c	\
	Facility.c	\
	FacilityFunction.c	\
	FacilityDesignationAltimeter.c	\
	FacilityDesignationATISCode.c	\
	FacilityName.c	\
	Fix.c	\
	FixName.c	\
	FlightInformation.c	\
	FreeText.c	\
	Frequency.c	\
	Frequencyhf.c	\
	Frequencysatchannel.c	\
	Frequencyuhf.c	\
	Frequencyvhf.c	\
	FurtherInstructions.c	\
	Holdatwaypoint.c	\
	HoldClearance.c	\
	Humidity.c	\
	InterceptCourseFrom.c	\
	InterceptCourseFromSelection.c	\
	Icing.c	\
	Latitude.c	\
	LatitudeDegrees.c	\
	LatitudeDegreesMinutes.c	\
	LatitudeDegreesMinutesSeconds.c	\
	LatitudeDirection.c	\
	LatitudeWholeDegrees.c	\
	LatitudeLongitude.c	\
	LatitudeReportingPoints.c	\
	LatitudeType.c	\
	LatLonWholeMinutes.c	\
	LatLonReportingPoints.c	\
	LegDistance.c	\
	LegDistanceEnglish.c	\
	LegDistanceMetric.c	\
	LegTime.c	\
	LegType.c	\
	Level.c	\
	LevelFeet.c	\
	LevelFlightLevel.c	\
	LevelFlightLevelMetric.c	\
	LevelLevel.c	\
	LevelMeters.c	\
	LevelPosition.c	\
	LevelProcedureName.c	\
	LevelsOfFlight.c	\
	LevelSpeed.c	\
	LevelSpeedSpeed.c	\
	LevelTime.c	\
	LevelType.c	\
	Longitude.c	\
	LongitudeDegrees.c	\
	LongitudeDegreesMinutes.c	\
	LongitudeDegreesMinutesSeconds.c	\
	LongitudeDirection.c	\
	LongitudeWholeDegrees.c	\
	LongitudeReportingPoints.c	\
	LongitudeType.c	\
	MinutesLatLon.c	\
	Navaid.c	\
	NavaidName.c	\
	PersonsOnBoard.c	\
	PlaceBearing.c	\
	PlaceBearingDistance.c	\
	PlaceBearingPlaceBearing.c	\
	Position.c	\
	PositionDegrees.c	\
	PositionDistanceSpecifiedDirection.c	\
	PositionLevel.c	\
	PositionLevelLevel.c	\
	PositionLevelSpeed.c	\
	PositionPosition.c	\
	PositionProcedureName.c	\
	PositionReport.c	\
	PositionRouteClearanceIndex.c	\
	PositionSpeed.c	\
	PositionSpeedSpeed.c	\
	PositionTime.c	\
	PositionTimeLevel.c	\
	PositionTimeTime.c	\
	PositionUnitNameFrequency.c	\
	Procedure.c	\
	ProcedureName.c	\
	ProcedureTransition.c	\
	ProcedureType.c	\
	PublishedIdentifier.c	\
	RemainingFuel.c	\
	RemainingFuelPersonsOnBoard.c	\
	ReportingPoints.c	\
	RevisionNumber.c	\
	RouteAndLevels.c	\
	RouteClearance.c	\
	RouteClearanceIndex.c	\
	RouteInformation.c	\
	RouteInformationAdditional.c	\
	RTARequiredTimeArrival.c	\
	RTATime.c	\
	RTATolerance.c	\
	Runway.c	\
	RunwayDirection.c	\
	RunwayConfiguration.c	\
	RunwayRVR.c	\
	RVR.c	\
	RVRFeet.c	\
	RVRMeters.c	\
	SecondsLatLon.c	\
	Speed.c	\
	SpeedIndicated.c	\
	SpeedIndicatedMetric.c	\
	SpeedGround.c	\
	SpeedGroundMetric.c	\
	SpeedMach.c	\
	SpeedSpeed.c	\
	SpeedTime.c	\
	SpeedTrue.c	\
	SpeedTrueMetric.c	\
	SpeedType.c	\
	SpeedTypeSpeedTypeSpeedType.c	\
	SpeedTypeSpeedTypeSpeedTypeSpeed.c	\
	Temperature.c	\
	TimeLevel.c	\
	TimeDeparture.c	\
	TimeDistanceSpecifiedDirection.c	\
	TimeDistanceToFromPosition.c	\
	Timehhmmss.c	\
	TimeHours.c	\
	TimeUnitNameFrequency.c	\
	TimeMinutes.c	\
	TimePosition.c	\
	TimePositionLevel.c	\
	TimePositionLevelSpeed.c	\
	TimeSeconds.c	\
	TimeSpeed.c	\
	TimeSpeedSpeed.c	\
	TimeTime.c	\
	TimeToFromPosition.c	\
	TimeTolerance.c	\
	ToFrom.c	\
	ToFromPosition.c	\
	TrafficType.c	\
	Turbulence.c	\
	UnitName.c	\
	UnitNameFrequency.c	\
	DMVersionNumber.c	\
	VerticalChange.c	\
	VerticalDirection.c	\
	VerticalRate.c	\
	VerticalRateEnglish.c	\
	VerticalRateMetric.c	\
	WaypointSpeedLevel.c	\
	WindDirection.c	\
	Winds.c	\
	WindSpeed.c	\
	WindSpeedEnglish.c	\
	WindSpeedMetric.c

ASN_MODULE_HEADERS=	\
	Fully-encoded-data.h	\
	PDV-list.h	\
	Transfer-syntax-name.h	\
	Presentation-context-identifier.h	\
	EXTERNALt.h	\
	ACSE-apdu.h	\
	AARQ-apdu.h	\
	AARE-apdu.h	\
	RLRQ-apdu.h	\
	RLRE-apdu.h	\
	ABRT-apdu.h	\
	ABRT-diagnostic.h	\
	ABRT-source.h	\
	ACSE-requirements.h	\
	Application-context-name-list.h	\
	Application-context-name.h	\
	AP-title.h	\
	AE-qualifier.h	\
	AP-title-form1.h	\
	AE-qualifier-form1.h	\
	AP-title-form2.h	\
	AE-qualifier-form2.h	\
	AE-invocation-identifier.h	\
	AP-invocation-identifier.h	\
	Associate-result.h	\
	Associate-source-diagnostic.h	\
	Association-information.h	\
	Implementation-data.h	\
	Mechanism-name.h	\
	Release-request-reason.h	\
	Release-response-reason.h	\
	Authentication-value.h	\
	Name.h	\
	RDNSequence.h	\
	RelativeDistinguishedName.h	\
	AttributeTypeAndValue.h	\
	CMAircraftMessage.h	\
	CMGroundMessage.h	\
	AircraftFlightIdentification.h	\
	Airport.h	\
	APAddress.h	\
	AEQualifier.h	\
	AEQualifierVersion.h	\
	AEQualifierVersionAddress.h	\
	CMAbortReason.h	\
	CMContactRequest.h	\
	CMContactResponse.h	\
	CMForwardRequest.h	\
	CMForwardResponse.h	\
	CMLogonRequest.h	\
	CMLogonResponse.h	\
	CMUpdate.h	\
	Date.h	\
	DateTime.h	\
	Day.h	\
	FacilityDesignation.h	\
	LongTsap.h	\
	Month.h	\
	Response.h	\
	ShortTsap.h	\
	Time.h	\
	Timehours.h	\
	Timeminutes.h	\
	VersionNumber.h	\
	Year.h	\
	GroundPDUs.h	\
	UplinkMessage.h	\
	AircraftPDUs.h	\
	StartDownMessage.h	\
	Mode.h	\
	DownlinkMessage.h	\
	CPDLCUserAbortReason.h	\
	CPDLCProviderAbortReason.h	\
	ProtectedGroundPDUs.h	\
	ProtectedUplinkMessage.h	\
	ATCForwardMessage.h	\
	ForwardHeader.h	\
	ForwardMessage.h	\
	ATCForwardResponse.h	\
	ProtectedAircraftPDUs.h	\
	ProtectedStartDownMessage.h	\
	ProtectedMode.h	\
	ProtectedDownlinkMessage.h	\
	AlgorithmIdentifier.h	\
	CPDLCMessage.h	\
	PMCPDLCUserAbortReason.h	\
	PMCPDLCProviderAbortReason.h	\
	ATCUplinkMessage.h	\
	ATCUplinkMessageData.h	\
	ATCDownlinkMessage.h	\
	ATCDownlinkMessageData.h	\
	ATCMessageHeader.h	\
	MsgIdentificationNumber.h	\
	MsgReferenceNumber.h	\
	LogicalAck.h	\
	ATCUplinkMsgElementId.h	\
	ATCDownlinkMsgElementId.h	\
	AircraftAddress.h	\
	Altimeter.h	\
	AltimeterEnglish.h	\
	AltimeterMetric.h	\
	ATISCode.h	\
	ATSRouteDesignator.h	\
	ATWAlongTrackWaypoint.h	\
	ATWLevel.h	\
	ATWLevelSequence.h	\
	ATWLevelTolerance.h	\
	ATWDistance.h	\
	ATWDistanceTolerance.h	\
	ClearanceType.h	\
	Code.h	\
	CodeOctalDigit.h	\
	ControlledTime.h	\
	DateTimeGroup.h	\
	DegreeIncrement.h	\
	Degrees.h	\
	DegreesMagnetic.h	\
	DegreesTrue.h	\
	DepartureClearance.h	\
	DepartureMinimumInterval.h	\
	Direction.h	\
	DirectionDegrees.h	\
	Distance.h	\
	DistanceKm.h	\
	DistanceNm.h	\
	DistanceSpecified.h	\
	DistanceSpecifiedDirection.h	\
	DistanceSpecifiedDirectionTime.h	\
	DistanceSpecifiedKm.h	\
	DistanceSpecifiedNm.h	\
	ErrorInformation.h	\
	Facility.h	\
	FacilityFunction.h	\
	FacilityDesignationAltimeter.h	\
	FacilityDesignationATISCode.h	\
	FacilityName.h	\
	Fix.h	\
	FixName.h	\
	FlightInformation.h	\
	FreeText.h	\
	Frequency.h	\
	Frequencyhf.h	\
	Frequencysatchannel.h	\
	Frequencyuhf.h	\
	Frequencyvhf.h	\
	FurtherInstructions.h	\
	Holdatwaypoint.h	\
	HoldClearance.h	\
	Humidity.h	\
	InterceptCourseFrom.h	\
	InterceptCourseFromSelection.h	\
	Icing.h	\
	Latitude.h	\
	LatitudeDegrees.h	\
	LatitudeDegreesMinutes.h	\
	LatitudeDegreesMinutesSeconds.h	\
	LatitudeDirection.h	\
	LatitudeWholeDegrees.h	\
	LatitudeLongitude.h	\
	LatitudeReportingPoints.h	\
	LatitudeType.h	\
	LatLonWholeMinutes.h	\
	LatLonReportingPoints.h	\
	LegDistance.h	\
	LegDistanceEnglish.h	\
	LegDistanceMetric.h	\
	LegTime.h	\
	LegType.h	\
	Level.h	\
	LevelFeet.h	\
	LevelFlightLevel.h	\
	LevelFlightLevelMetric.h	\
	LevelLevel.h	\
	LevelMeters.h	\
	LevelPosition.h	\
	LevelProcedureName.h	\
	LevelsOfFlight.h	\
	LevelSpeed.h	\
	LevelSpeedSpeed.h	\
	LevelTime.h	\
	LevelType.h	\
	Longitude.h	\
	LongitudeDegrees.h	\
	LongitudeDegreesMinutes.h	\
	LongitudeDegreesMinutesSeconds.h	\
	LongitudeDirection.h	\
	LongitudeWholeDegrees.h	\
	LongitudeReportingPoints.h	\
	LongitudeType.h	\
	MinutesLatLon.h	\
	Navaid.h	\
	NavaidName.h	\
	PersonsOnBoard.h	\
	PlaceBearing.h	\
	PlaceBearingDistance.h	\
	PlaceBearingPlaceBearing.h	\
	Position.h	\
	PositionDegrees.h	\
	PositionDistanceSpecifiedDirection.h	\
	PositionLevel.h	\
	PositionLevelLevel.h	\
	PositionLevelSpeed.h	\
	PositionPosition.h	\
	PositionProcedureName.h	\
	PositionReport.h	\
	PositionRouteClearanceIndex.h	\
	PositionSpeed.h	\
	PositionSpeedSpeed.h	\
	PositionTime.h	\
	PositionTimeLevel.h	\
	PositionTimeTime.h	\
	PositionUnitNameFrequency.h	\
	Procedure.h	\
	ProcedureName.h	\
	ProcedureTransition.h	\
	ProcedureType.h	\
	PublishedIdentifier.h	\
	RemainingFuel.h	\
	RemainingFuelPersonsOnBoard.h	\
	ReportingPoints.h	\
	RevisionNumber.h	\
	RouteAndLevels.h	\
	RouteClearance.h	\
	RouteClearanceIndex.h	\
	RouteInformation.h	\
	RouteInformationAdditional.h	\
	RTARequiredTimeArrival.h	\
	RTATime.h	\
	RTATolerance.h	\
	Runway.h	\
	RunwayDirection.h	\
	RunwayConfiguration.h	\
	RunwayRVR.h	\
	RVR.h	\
	RVRFeet.h	\
	RVRMeters.h	\
	SecondsLatLon.h	\
	Speed.h	\
	SpeedIndicated.h	\
	SpeedIndicatedMetric.h	\
	SpeedGround.h	\
	SpeedGroundMetric.h	\
	SpeedMach.h	\
	SpeedSpeed.h	\
	SpeedTime.h	\
	SpeedTrue.h	\
	SpeedTrueMetric.h	\
	SpeedType.h	\
	SpeedTypeSpeedTypeSpeedType.h	\
	SpeedTypeSpeedTypeSpeedTypeSpeed.h	\
	Temperature.h	\
	TimeLevel.h	\
	TimeDeparture.h	\
	TimeDistanceSpecifiedDirection.h	\
	TimeDistanceToFromPosition.h	\
	Timehhmmss.h	\
	TimeHours.h	\
	TimeUnitNameFrequency.h	\
	TimeMinutes.h	\
	TimePosition.h	\
	TimePositionLevel.h	\
	TimePositionLevelSpeed.h	\
	TimeSeconds.h	\
	TimeSpeed.h	\
	TimeSpeedSpeed.h	\
	TimeTime.h	\
	TimeToFromPosition.h	\
	TimeTolerance.h	\
	ToFrom.h	\
	ToFromPosition.h	\
	TrafficType.h	\
	Turbulence.h	\
	UnitName.h	\
	UnitNameFrequency.h	\
	DMVersionNumber.h	\
	VerticalChange.h	\
	VerticalDirection.h	\
	VerticalRate.h	\
	VerticalRateEnglish.h	\
	VerticalRateMetric.h	\
	WaypointSpeedLevel.h	\
	WindDirection.h	\
	Winds.h	\
	WindSpeed.h	\
	WindSpeedEnglish.h	\
	WindSpeedMetric.h

ASN_MODULE_HEADERS+=ANY.h
ASN_MODULE_SOURCES+=ANY.c
ASN_MODULE_HEADERS+=BOOLEAN.h
ASN_MODULE_SOURCES+=BOOLEAN.c
ASN_MODULE_HEADERS+=INTEGER.h
ASN_MODULE_HEADERS+=NativeEnumerated.h
ASN_MODULE_HEADERS+=GraphicString.h
ASN_MODULE_SOURCES+=GraphicString.c
ASN_MODULE_HEADERS+=IA5String.h
ASN_MODULE_SOURCES+=IA5String.c
ASN_MODULE_SOURCES+=INTEGER.c
ASN_MODULE_HEADERS+=NULL.h
ASN_MODULE_SOURCES+=NULL.c
ASN_MODULE_SOURCES+=NativeEnumerated.c
ASN_MODULE_HEADERS+=NativeInteger.h
ASN_MODULE_SOURCES+=NativeInteger.c
ASN_MODULE_HEADERS+=NumericString.h
ASN_MODULE_SOURCES+=NumericString.c
ASN_MODULE_HEADERS+=OBJECT_IDENTIFIER.h
ASN_MODULE_SOURCES+=OBJECT_IDENTIFIER.c
ASN_MODULE_HEADERS+=ObjectDescriptor.h
ASN_MODULE_SOURCES+=ObjectDescriptor.c
ASN_MODULE_HEADERS+=RELATIVE-OID.h
ASN_MODULE_SOURCES+=RELATIVE-OID.c
ASN_MODULE_HEADERS+=asn_SEQUENCE_OF.h
ASN_MODULE_SOURCES+=asn_SEQUENCE_OF.c
ASN_MODULE_HEADERS+=asn_SET_OF.h
ASN_MODULE_SOURCES+=asn_SET_OF.c
ASN_MODULE_HEADERS+=constr_CHOICE.h
ASN_MODULE_SOURCES+=constr_CHOICE.c
ASN_MODULE_HEADERS+=constr_SEQUENCE.h
ASN_MODULE_SOURCES+=constr_SEQUENCE.c
ASN_MODULE_HEADERS+=constr_SEQUENCE_OF.h
ASN_MODULE_SOURCES+=constr_SEQUENCE_OF.c
ASN_MODULE_HEADERS+=constr_SET_OF.h
ASN_MODULE_SOURCES+=constr_SET_OF.c
ASN_MODULE_HEADERS+=asn_application.h
ASN_MODULE_HEADERS+=asn_system.h
ASN_MODULE_HEADERS+=asn_codecs.h
ASN_MODULE_HEADERS+=asn_internal.h
ASN_MODULE_HEADERS+=OCTET_STRING.h
ASN_MODULE_SOURCES+=OCTET_STRING.c
ASN_MODULE_HEADERS+=BIT_STRING.h
ASN_MODULE_SOURCES+=BIT_STRING.c
ASN_MODULE_SOURCES+=asn_codecs_prim.c
ASN_MODULE_HEADERS+=asn_codecs_prim.h
ASN_MODULE_HEADERS+=ber_tlv_length.h
ASN_MODULE_SOURCES+=ber_tlv_length.c
ASN_MODULE_HEADERS+=ber_tlv_tag.h
ASN_MODULE_SOURCES+=ber_tlv_tag.c
ASN_MODULE_HEADERS+=ber_decoder.h
ASN_MODULE_SOURCES+=ber_decoder.c
ASN_MODULE_HEADERS+=der_encoder.h
ASN_MODULE_SOURCES+=der_encoder.c
ASN_MODULE_HEADERS+=constr_TYPE.h
ASN_MODULE_SOURCES+=constr_TYPE.c
ASN_MODULE_HEADERS+=constraints.h
ASN_MODULE_SOURCES+=constraints.c
ASN_MODULE_HEADERS+=xer_support.h
ASN_MODULE_SOURCES+=xer_support.c
ASN_MODULE_HEADERS+=xer_decoder.h
ASN_MODULE_SOURCES+=xer_decoder.c
ASN_MODULE_HEADERS+=xer_encoder.h
ASN_MODULE_SOURCES+=xer_encoder.c
ASN_MODULE_HEADERS+=per_support.h
ASN_MODULE_SOURCES+=per_support.c
ASN_MODULE_HEADERS+=per_decoder.h
ASN_MODULE_SOURCES+=per_decoder.c
ASN_MODULE_HEADERS+=per_encoder.h
ASN_MODULE_SOURCES+=per_encoder.c
ASN_MODULE_HEADERS+=per_opentype.h
ASN_MODULE_SOURCES+=per_opentype.c

LIB=asn1.a
OBJS=${ASN_MODULE_SOURCES:.c=.o}

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rv $@ $^
	$(RANLIB) $@

clean:
	rm -f $(LIB) $(OBJS)

regen:
	asn1c -fcompound-names -gen-PER atn-ulcs.asn1 atn-cm.asn1 atn-cpdlc.asn1
# workaround for missing SET_OF_encode_uper function
	sed -i 's/SET_OF_encode_uper,/NULL,/'  RelativeDistinguishedName.c
# enable printing of CHOICE names by asn_print()
	sed -i 's/if(0)/if(1)/' constr_CHOICE.c

