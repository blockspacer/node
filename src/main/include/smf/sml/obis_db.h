/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_SML_OBIS_DB_H
#define NODE_SML_OBIS_DB_H

#include <smf/sml/intrinsics/obis.h>

//
//	macro to create OBIS codes without the 0x prefix
//
#define DEFINE_OBIS_CODE(p1, p2, p3, p4, p5, p6, name)	\
	OBIS_##name (0x##p1, 0x##p2, 0x##p3, 0x##p4, 0x##p5, 0x##p6)

#define OBIS_CODE(p1, p2, p3, p4, p5, p6)	\
	node::sml::obis (0x##p1, 0x##p2, 0x##p3, 0x##p4, 0x##p5, 0x##p6)

namespace node
{
	namespace sml
	{

		//
		//	general
		//
		const static obis	DEFINE_OBIS_CODE(00, 00, 00, 00, 00, FF, METER_ADDRESS);
		const static obis	DEFINE_OBIS_CODE(00, 00, 00, 00, 01, FF, IDENTITY_NR_1);
		const static obis	DEFINE_OBIS_CODE(00, 00, 00, 00, 02, FF, IDENTITY_NR_2);
		const static obis	DEFINE_OBIS_CODE(00, 00, 00, 00, 03, FF, IDENTITY_NR_3);
		const static obis	DEFINE_OBIS_CODE(00, 00, 00, 01, 00, FF, RESET_COUNTER);

		const static obis	DEFINE_OBIS_CODE(00, 00, 60, 01, 00, FF, SERIAL_NR);	//	Serial number I (assigned by the manufacturer).
		const static obis	DEFINE_OBIS_CODE(00, 00, 60, 01, 01, FF, SERIAL_NR_SECOND);	//	Serial number II (assigned by the manufacturer).
		const static obis	DEFINE_OBIS_CODE(00, 00, 61, 61, 00, FF, MBUS_STATE);   //	Status according to EN13757-3
		const static obis	DEFINE_OBIS_CODE(00, 00, 60, 08, 00, FF, CONFIG_OVERVIEW);   //	

		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 10, 00, 01, CLASS_OP_LSM_STATUS);	//	LSM status
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, 00, FF, ACTUATORS);	//	list of actuators
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, 10, FF, CLASS_OP_LSM_ACTOR_ID);	//	ServerID des Aktors (uint16)
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, 11, FF, CLASS_OP_LSM_CONNECT);	//	Verbindungsstatus
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, 11, 01, CLASS_OP_LSM_SWITCH);	//	Schaltanforderung
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, 13, FF, CLASS_OP_LSM_FEEDBACK);	//	feedback configuration
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, 14, FF, CLASS_OP_LSM_LOAD);	//	number of loads
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, 15, FF, CLASS_OP_LSM_POWER);	//	total power
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, A0, FF, CLASS_STATUS);	//	see: 2.2.1.3 Status der Aktoren (Kontakte)
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, A1, FF, CLASS_OP_LSM_VERSION);	//	LSM version
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, A2, FF, CLASS_OP_LSM_TYPE);	//	LSM type
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, A9, 01, CLASS_OP_LSM_ACTIVE_RULESET); //	active ruleset
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, A9, 01, CLASS_RADIO_KEY);	//	or radio key
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 11, A9, 02, CLASS_OP_LSM_PASSIVE_RULESET);
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 14, 03, FF, CLASS_OP_LSM_JOB);//	Schaltauftrags ID ((octet string)	
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 14, 20, FF, CLASS_OP_LSM_POSITION);	//	current position

		//	Identifikationsnummer 1.1 - comes as unsigned int with 3 bytes (this is the server ID)
		const static obis	DEFINE_OBIS_CODE(01, 00, 00, 00, 00, FF, SERVER_ID_1_1);
		const static obis	DEFINE_OBIS_CODE(01, 00, 00, 00, 01, FF, SERVER_ID_1_2);		//	Identifikationsnummer 1.2
		const static obis	DEFINE_OBIS_CODE(01, 00, 00, 00, 02, FF, SERVER_ID_1_3);		//	Identifikationsnummer 1.3
		const static obis	DEFINE_OBIS_CODE(01, 00, 00, 00, 03, FF, SERVER_ID_1_4);		//	Identifikationsnummer 1.4
		const static obis	DEFINE_OBIS_CODE(01, 00, 00, 00, 09, FF, DEVICE_ID);	//	encode profiles

		const static obis	DEFINE_OBIS_CODE(01, 00, 00, 09, 0B, 00, CURRENT_UTC);	//	readout time in UTC

		//
		//	Lastg�nge (profiles)
		//	The OBIS code to encode profiles is 81 81 C7 8A 83 FF
		//

		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 82, 03, FF, DATA_MANUFACTURER);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 82, 05, FF, DATA_PUBLIC_KEY);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 82, 81, FF, DATA_IP_ADDRESS);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 03, FF, DATA_AES_KEY);
		const static obis	DEFINE_OBIS_CODE(81, 81, 61, 3C, 01, FF, DATA_USER_NAME);
		const static obis	DEFINE_OBIS_CODE(81, 81, 61, 3C, 02, FF, DATA_USER_PWD);

		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 10, FF, PROFILE_1_MINUTE);	//	1 minute
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 11, FF, PROFILE_15_MINUTE);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 12, FF, PROFILE_60_MINUTE);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 13, FF, PROFILE_24_HOUR);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 14, FF, PROFILE_LAST_2_HOURS);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 15, FF, PROFILE_LAST_WEEK);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 16, FF, PROFILE_1_MONTH);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 17, FF, PROFILE_1_YEAR);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 18, FF, PROFILE_INITIAL);

		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 8A, 83, FF, PROFILE);	//	encode profiles
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 8A, 01, FF, PUSH_OPERATIONS);	//	7.3.1.26 Datenstruktur zum Transport der Eigenschaften von Push-Vorg�ngen. 

		//
		/**
		 * @return true id the 4th first bytes are 0x81, 0x81, 0xC7, 0x86
		 */
		bool is_profile(obis const&);

		//
		//	root elements
		//
		const static obis	DEFINE_OBIS_CODE(00, 80, 80, 00, 10, FF, CODE_ROOT_MEMORY_USAGE);	//	request memory usage

		const static obis	DEFINE_OBIS_CODE(81, 02, 00, 07, 00, FF, CODE_ROOT_CUSTOM_INTERFACE);	//	see: 7.3.1.3 Datenstruktur zum Lesen / Setzen der Parameter f�r die Kundenschnittstelle
		const static obis	DEFINE_OBIS_CODE(81, 02, 00, 07, 10, FF, CODE_ROOT_CUSTOM_PARAM);	//	see: 7.3.1.4 Datenstruktur f�r dynamischen Eigenschaften der Endkundenschnittstelle 
		const static obis	DEFINE_OBIS_CODE(81, 04, 00, 06, 10, FF, CODE_ROOT_WAN);	//	see: 7.3.1.5 Datenstruktur zur Abfrage des WAN Status 
		//const static obis	DEFINE_OBIS_CODE(81, 04, 00, 06, 10, FF, CODE_ROOT_WAN_PARAM);	//	see: 7.3.1.6 Datenstruktur zum Lesen/Setzen der WAN Parameter 
		const static obis	DEFINE_OBIS_CODE(81, 04, 02, 07, 00, FF, CODE_ROOT_GSM);	//	see: Datenstruktur zum Lesen/Setzen der GSM Parameter 
		const static obis	DEFINE_OBIS_CODE(81, 04, 0D, 07, 00, FF, CODE_ROOT_GPRS_PARAM);	//	see: Datenstruktur zum Lesen / Setzen der Provider-abh�ngigen GPRS-Parameter 

		const static obis	DEFINE_OBIS_CODE(81, 06, 0F, 06, 00, FF, CODE_ROOT_W_MBUS_STATUS);	//	see: 7.3.1.23 Datenstruktur zum Lesen des W-MBUS-Status 
		const static obis	DEFINE_OBIS_CODE(81, 46, 00, 00, 02, FF, CODE_ADDRESSED_PROFILE);
		const static obis	DEFINE_OBIS_CODE(81, 47, 17, 07, 00, FF, CODE_PUSH_TARGET);	//	push target name
		const static obis	DEFINE_OBIS_CODE(81, 48, 0D, 06, 00, FF, CODE_ROOT_LAN_DSL);	//	see: 7.3.1.19 Datenstruktur zur Abfrage dynamischer LAN/DSL- Betriebsparameter
		const static obis	DEFINE_OBIS_CODE(81, 49, 0D, 06, 00, FF, CODE_ROOT_IPT_STATE);	//	see: 7.3.1.8 Datenstruktur zur Abfrage des IPT Status 
		const static obis	DEFINE_OBIS_CODE(81, 49, 0D, 07, 00, FF, CODE_ROOT_IPT_PARAM);	//	see: 7.3.1.9 Datenstruktur zur Lesen/Setzen der IPT Parameter 

		const static obis	DEFINE_OBIS_CODE(81, 81, 00, 00, 00, 13, CODE_PEER_ADDRESS_WANGSM);	//	peer address: WAN/GSM
		const static obis	DEFINE_OBIS_CODE(81, 81, 00, 00, 00, FF, CODE_PEER_ADDRESS);	//	unit is 255

		const static obis	DEFINE_OBIS_CODE(81, 81, 01, 16, FF, FF, CODE_ROOT_NEW_DEVICES);	//	new active devices
		const static obis	DEFINE_OBIS_CODE(81, 81, 10, 26, FF, FF, CODE_ROOT_INVISIBLE_DEVICES);	//	not longer visible devices
		const static obis	DEFINE_OBIS_CODE(81, 81, 10, 06, FF, FF, CODE_ROOT_VISIBLE_DEVICES);	//	visible devices (Liste der sichtbaren Sensoren)
		const static obis	DEFINE_OBIS_CODE(81, 81, 10, 06, 01, FF, CODE_LIST_1_VISIBLE_DEVICES);	//	1. Liste der sichtbaren Sensoren
		const static obis	DEFINE_OBIS_CODE(81, 81, 11, 06, FF, FF, CODE_ROOT_ACTIVE_DEVICES);	//	active devices (Liste der aktiven Sensoren)
		const static obis	DEFINE_OBIS_CODE(81, 81, 11, 06, 01, FF, CODE_LIST_1_ACTIVE_DEVICES);	//	1. Liste der aktiven Sensoren)
		const static obis	DEFINE_OBIS_CODE(81, 81, 12, 06, FF, FF, CODE_ROOT_DEVICE_INFO);	//	extended device information
		const static obis	DEFINE_OBIS_CODE(81, 81, 81, 60, FF, FF, CODE_ROOT_ACCESS_RIGHTS);	//	see: 7.3.1.2 Datenstruktur zur Parametrierung der Rollen / Benutzerrechte 
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 82, 01, FF, CODE_ROOT_DEVICE_IDENT);	//	see: 7.3.2.9 Datenstruktur zur Abfrage der Ger�te-Identifikation) 
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 82, 02, FF, CODE_DEVICE_CLASS);	//	Ger�teklasse 
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 82, 04, FF, CODE_SERVER_ID);	//	Server ID der sichtbaren Komponenten
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 82, 06, FF, CODE_ROOT_FIRMWARE);	//	Firmware
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 00, FF, CODE_ROOT_SENSOR_PROPERTY);	//	properties of data sensor/actor (Eigenschaften eines Datenspiegels)
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 88, 01, FF, CODE_ROOT_NTP);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 88, 10, FF, CODE_ROOT_DEVICE_TIME);	//	device time
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 20, FF, CODE_ROOT_DATA_COLLECTOR);	//	properties of data collector (Eigenschaften eines Datensammlers)
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 93, 00, FF, CODE_ROOT_1107_IF);	 //	1107 interface
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 86, 02, FF, CODE_AVERAGE_TIME_MS);	//	average time between two received data records (milliseconds)
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 83, 82, 01, CODE_REBOOT);	//	request reboot

		//
		//	Interfaces
		//
		const static obis	DEFINE_OBIS_CODE(81, 48, 17, 07, 00, FF, CODE_IF_LAN_DSL);	// see: 7.3.1.18 Datenstruktur zum Lesen / Setzen der LAN/DSL-Parameter
		const static obis	DEFINE_OBIS_CODE(81, 04, 02, 07, 00, FF, CODE_IF_GSM);
		const static obis	DEFINE_OBIS_CODE(81, 04, 0D, 07, 00, FF, CODE_IF_GPRS);
		const static obis	DEFINE_OBIS_CODE(81, 02, 00, 07, 00, FF, CODE_IF_USER);	//	Endkundenschnittstelle:
		const static obis	DEFINE_OBIS_CODE(81, 49, 0D, 07, 00, FF, CODE_IF_IPT);
		const static obis	DEFINE_OBIS_CODE(81, 05, 0D, 07, 00, FF, CODE_IF_EDL);		//	M-Bus EDL
		const static obis	DEFINE_OBIS_CODE(81, 06, 19, 07, 00, FF, CODE_IF_wMBUS);	//	Wireless M-BUS:
		const static obis	DEFINE_OBIS_CODE(81, 04, 18, 07, 00, FF, CODE_IF_PLC);
		const static obis	DEFINE_OBIS_CODE(81, 05, 0D, 07, 00, FF, CODE_IF_SyM2);	//	Erweiterungsschnittstelle:
		const static obis	DEFINE_OBIS_CODE(81, 00, 00, 09, 0B, 00, ACT_SENSOR_TIME);	//	actSensorTime - time delivered from sensor 

		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 89, E1, FF, CLASS_OP_LOG);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, 89, E2, FF, CLASS_EVENT);	//	Ereignis (uint32)

		//
		//	wMBus - 81 06 0F 06 00 FF
		//	7.3.1.23 Datenstruktur zum Lesen des W-MBUS-Status 
		//
		const static obis	DEFINE_OBIS_CODE(81, 06, 00, 00, 01, 00, W_MBUS_ADAPTER_MANUFACTURER);
		const static obis	DEFINE_OBIS_CODE(81, 06, 00, 00, 03, 00, W_MBUS_ADAPTER_ID);
		const static obis	DEFINE_OBIS_CODE(81, 06, 00, 02, 00, 00, W_MBUS_FIRMWARE);
		const static obis	DEFINE_OBIS_CODE(81, 06, 00, 02, 03, FF, W_BUS_HARDWARE);

		//	Spannung - voltage
		//	Strom - current
		//	Leistungsfaktor - power factor
		//	Scheinleistung - apparent power
		//	Wirkleistung - effective (or active) power

		const static obis	DEFINE_OBIS_CODE(01, 00, 01, 08, 00, FF, REG_POS_AE_NO_TARIFF);	//	Z�hlwerk pos. Wirkenergie, 	tariflos
		const static obis	DEFINE_OBIS_CODE(01, 00, 01, 08, 01, FF, REG_POS_AE_T1);	//	Z�hlwerk pos. Wirkenergie, Tarif 1
		const static obis	DEFINE_OBIS_CODE(01, 00, 01, 08, 02, FF, REG_POS_AE_T2);	//	Z�hlwerk pos. Wirkenergie, Tarif 2
		const static obis	DEFINE_OBIS_CODE(01, 00, 02, 08, 00, FF, REG_NEG_AE_NO_TARIFF);	//	Z�hlwerk neg. Wirkenergie, 	tariflos
		const static obis	DEFINE_OBIS_CODE(01, 00, 02, 08, 01, FF, REG_NEG_AE_T1);	//	Z�hlwerk neg. Wirkenergie, Tarif 1
		const static obis	DEFINE_OBIS_CODE(01, 00, 02, 08, 02, FF, REG_NEG_AE_T2); //	Z�hlwerk neg. Wirkenergie, Tarif 2
		//const static obis	DEFINE_OBIS_CODE(01, 00, 0F, 07, 00, FF, REG_CUR_POS_AE);	//	Aktuelle pos.	Wirkleistung 	Betrag
		const static obis	DEFINE_OBIS_CODE(01, 00, 0F, 07, 00, FF, REG_CUR_POS_AE);
		const static obis	DEFINE_OBIS_CODE(01, 00, 10, 07, 00, ff, REG_CUR_AP);	//	aktuelle Wirkleistung 

		//	01 00 24 07 00 FF: Wirkleistung L1
		//01 00 38 07 00 FF: Wirkleistung L2
		//01 00 4C 07 00 FF: Wirkleistung L3
		//01 00 60 32 00 02: Aktuelle Chiptemperatur
		//01 00 60 32 00 03: Minimale Chiptemperatur
		//01 00 60 32 00 04: Maximale Chiptemperatur
		//01 00 60 32 00 05: Gemittelte Chiptemperatur
		const static obis	DEFINE_OBIS_CODE(01, 00, 1F, 07, 00, FF, REG_CURRENT_L1);	//	Strom L1
		const static obis	DEFINE_OBIS_CODE(01, 00, 20, 07, 00, FF, REG_VOLTAGE_L1);	//	Spannung L1
		const static obis	DEFINE_OBIS_CODE(01, 00, 33, 07, 00, FF, REG_CURRENT_L2);	//	Strom L2
		const static obis	DEFINE_OBIS_CODE(01, 00, 34, 07, 00, FF, REG_VOLTAGE_L2);	//	Spannung L2
		const static obis	DEFINE_OBIS_CODE(01, 00, 47, 07, 00, FF, REG_CURRENT_L3);	//	Strom L3
		const static obis	DEFINE_OBIS_CODE(01, 00, 48, 07, 00, FF, REG_VOLTAGE_L3);	//	Spannung L3 
		const static obis	DEFINE_OBIS_CODE(01, 00, 60, 32, 03, 03, REG_VOLTAGE_MIN);	//	Spannungsminimum
		const static obis	DEFINE_OBIS_CODE(01, 00, 60, 32, 03, 04, REG_VOLTAGE_MAX);	//	Spannungsmaximum


		//		const static obis	OBIS_CLASS_OP_LOG_EVENT(0x81, 0x81, 0xC7, 0x89, 0xE2, 0xFF);	//	Ereignis (uint32)
		const static obis	OBIS_CLASS_OP_LOG_PEER_ADDRESS(0x81, 0x81, 0x00, 0x00, 0x00, 0xFF);	//	Peer Adresse (octet string)
		const static obis	OBIS_CLASS_OP_MONITORING_STATUS(0x00, 0x80, 0x80, 0x11, 0x13, 0x01);	//	�berwachungsstatus (uint8) - tats�chlich i32

		/**
		 *	Statuswort (per Schreiben ist das R�cksetzen ausgew�hlter Statusbits) Unsigned64
		 *	see also http://www.sagemcom.com/fileadmin/user_upload/Energy/Dr.Neuhaus/Support/ZDUE-MUC/Doc_communs/ZDUE-MUC_Anwenderhandbuch_V2_5.pdf
		 *	bit meaning
		 *	0	always 0
		 *	1	always 1
		 *	2-7	always 0
		 *	8	1 if error was detected
		 *	9	1 if restart was triggered by watchdog reset
		 *	10	0 if IP address is available (DHCP)
		 *	11	0 if ethernet link is available
		 *	12	always 0
		 *	13	0 if authorized on IP-T server
		 *	14	1 ic case of out of memory
		 *	15	always 0
		 *	16	1 if Service interface is available (Kundenschnittstelle)
		 *	17	1 if extension interface is available (Erweiterungs-Schnittstelle)
		 *	18	1 if Wireless M-Bus interface is available
		 *	19	1 if PLC is available
		 *	20-31	always 0
		 *	32	1 if time base is unsure
		 */
		const static obis	DEFINE_OBIS_CODE(81, 00, 60, 05, 00, 00, CLASS_OP_LOG_STATUS_WORD);	
		const static obis	DEFINE_OBIS_CODE(81, 04, 2B, 07, 00, 00, CLASS_OP_LOG_FIELD_STRENGTH);
		const static obis	DEFINE_OBIS_CODE(81, 04, 1A, 07, 00, 00, CLASS_OP_LOG_CELL);	//	aktuelle Zelleninformation (uint16)
		const static obis	DEFINE_OBIS_CODE(81, 04, 17, 07, 00, 00, CLASS_OP_LOG_AREA_CODE);	//	aktueller Location - oder Areacode(uint16)
		const static obis	DEFINE_OBIS_CODE(81, 04, 0D, 06, 00, 00, CLASS_OP_LOG_PROVIDER);	//	aktueller Provider-Identifier(uint32)

		//
		//	attention codes
		//
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 00, ATTENTION_UNKNOWN_ERROR);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 01, ATTENTION_UNKNOWN_SML_ID);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 02, ATTENTION_NOT_AUTHORIZED);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 03, ATTENTION_NO_SERVER_ID);	//	unable to find recipient for request
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 04, ATTENTION_NO_REQ_FIELD);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 05, ATTENTION_CANNOT_WRITE);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 06, ATTENTION_CANNOT_READ);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 07, ATTENTION_COMM_ERROR);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 08, ATTENTION_PARSER_ERROR);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 09, ATTENTION_OUT_OF_RANGE);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 0A, ATTENTION_NOT_EXECUTED);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 0B, ATTENTION_INVALID_CRC);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 0C, ATTENTION_NO_BROADCAST);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 0D, ATTENTION_UNEXPECTED_MSG);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 0E, ATTENTION_UNKNOWN_OBIS_CODE);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 0F, ATTENTION_UNSUPPORTED_DATA_TYPE);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 10, ATTENTION_ELEMENT_NOT_OPTIONAL);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FE, 11, ATTENTION_NO_ENTRIES);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FD, 00, ATTENTION_OK);
		const static obis	DEFINE_OBIS_CODE(81, 81, C7, C7, FD, 01, ATTENTION_JOB_IS_RUNNINNG);

		/**
		 * @return a short description of the OBIS code if available
		 */
		const char* get_name(obis const&);

		/**
		 * @return a short description of the OBIS code if available
		 */
		const char* get_attention_name(obis const&);

		/**
		 * @return name of teh LSM event
		 */
		const char* get_LSM_event_name(std::uint32_t);

	}	//	sml
}	//	node


#endif	
