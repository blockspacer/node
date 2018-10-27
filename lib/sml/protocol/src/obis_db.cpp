/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */


#include <smf/sml/obis_db.h>

namespace node
{
	namespace sml
	{
		bool is_profile(obis const& code)
		{
			return code.is_matching(0x81, 0x81, 0xC7, 0x86);
		}

		const char* get_name(obis const& code)
		{
			if (code == OBIS_DATA_MANUFACTURER)			return "manufacturer";
			else if (code == OBIS_DATA_PUBLIC_KEY)		return "public-key";
			else if (code == OBIS_PROFILE_1_MINUTE)		return "1min";
			else if (code == OBIS_PROFILE_15_MINUTE)	return "15min";
			else if (code == OBIS_PROFILE_60_MINUTE)	return "1h";
			else if (code == OBIS_PROFILE_24_HOUR)		return "1d";
			else if (code == OBIS_PROFILE_LAST_2_HOURS)	return "last-2h";
			else if (code == OBIS_PROFILE_LAST_WEEK)	return "last-week";
			else if (code == OBIS_PROFILE_1_MONTH)		return "1month";
			else if (code == OBIS_PROFILE_1_YEAR)		return "1y";
			else if (code == OBIS_PROFILE_INITIAL)		return "initial";
			else if (code == OBIS_PROFILE)				return "encode-profile";

			//
			//	root elements
			//
			else if (code == OBIS_CODE_ROOT_NTP)				return "root-NTP";
			else if (code == OBIS_CODE_ROOT_DEVICE_IDENT)		return "root-device-id";
			else if (code == OBIS_CODE_DEVICE_CLASS)			return "device-class";
			else if (code == OBIS_CODE_SERVER_ID)		return "server-id-visible";
			else if (code == OBIS_CODE_ROOT_ACCESS_RIGHTS)		return "root-auth";
			else if (code == OBIS_CODE_ROOT_CUSTOM_INTERFACE)	return "root-custom-interface";
			else if (code == OBIS_CODE_ROOT_CUSTOM_PARAM)		return "root-custom-param";
			else if (code == OBIS_CODE_ROOT_WAN)				return "root-WAN-state";
			//else if (code == OBIS_CODE_ROOT_WAN_PARAM)			return "root-WAN-param";
			else if (code == OBIS_CODE_ROOT_GSM)				return "root-GSM";
			else if (code == OBIS_CODE_ROOT_IPT_STATE)			return "root-IPT-state";
			else if (code == OBIS_CODE_ROOT_IPT_PARAM)			return "root-IPT-param";
			else if (code == OBIS_CODE_ROOT_GPRS_PARAM)			return "root-GPRS";
			else if (OBIS_CODE_ROOT_W_MBUS_STATUS == code)		return "root-sMBus-status";
			else if (code == OBIS_CODE_ROOT_LAN_DSL)			return "root-LAN";
			else if (code == OBIS_CODE_ROOT_MEMORY_USAGE)		return "root-memory-usage";
			else if (code == OBIS_CODE_ROOT_DEVICE_TIME)		return "root-device-time";
			else if (code == OBIS_CODE_ROOT_ACTIVE_DEVICES)		return "root-active-devices";
			else if (code == OBIS_CODE_ROOT_NEW_DEVICES)		return "root-new-devices";
			else if (code == OBIS_CODE_ROOT_INVISIBLE_DEVICES)	return "root-lost-devices";
			else if (code == OBIS_CODE_ROOT_DEVICE_INFO)		return "root-device-info";
			else if (code == OBIS_CODE_ROOT_VISIBLE_DEVICES)	return "root-visible-devices";
			else if (code == OBIS_CODE_ROOT_SENSOR_PROPERTY)	return "root-sensor-prop";
			else if (code == OBIS_CODE_ROOT_DATA_COLLECTOR)		return "root-data-prop";
			

			else if (OBIS_CODE_IF_LAN_DSL == code)	return "IF_LAN_DSL";
			else if (OBIS_CODE_IF_GSM == code)		return "IF_GSM";
			else if (OBIS_CODE_IF_GPRS == code)		return "IF_GPRS";
			else if (OBIS_CODE_IF_USER == code)		return "IF_USER";
			else if (OBIS_CODE_IF_IPT == code)		return "IF_IPT";
			else if (OBIS_CODE_IF_EDL == code)		return "IF_EDL";
			else if (OBIS_CODE_IF_wMBUS == code)	return "IF_wMBUS";
			else if (OBIS_CODE_IF_PLC == code)		return "IF_PLC";
			else if (OBIS_CODE_IF_SyM2 == code)		return "IF_SyM2";

			else if (OBIS_W_MBUS_ADAPTER_MANUFACTURER == code)		return "W_MBUS_ADAPTER_MANUFACTURER";
			else if (OBIS_W_MBUS_ADAPTER_ID == code)				return "W_MBUS_ADAPTER_ID";
			else if (OBIS_W_MBUS_FIRMWARE == code)					return "W_MBUS_FIRMWARE";
			else if (OBIS_W_BUS_HARDWARE == code)					return "W_BUS_HARDWARE";

			else if (code == OBIS_CLASS_OP_LOG)		return "class-operation-log";
			else if (code == OBIS_CLASS_EVENT)		return "class-event";
			else if (code == OBIS_CLASS_STATUS)		return "class-status";

			else if (code == OBIS_ACT_SENSOR_TIME)		return "act-sensor-time";
			else if (code == OBIS_SERIAL_NR)			return "serial-number-I";
			else if (code == OBIS_SERIAL_NR_SECOND)		return "serial-number-II";
			else if (code == OBIS_MBUS_STATE)			return "status-EN13757-3";

			//	Identifikationsnummer 1.1 - comes as unsigned int with 3 bytes (this is the server ID)
			else if (code == OBIS_SERVER_ID_1_1)		return "identifier-1-1";
			else if (code == OBIS_SERVER_ID_1_2)		return "identifier-1-2";
			else if (code == OBIS_SERVER_ID_1_3)		return "identifier-1-3";
			else if (code == OBIS_SERVER_ID_1_4)		return "identifier-1-4";
			else if (code == OBIS_DEVICE_ID)			return "device-id";

			else if (code == OBIS_CURRENT_UTC)			return "readout-utc";

			else if (code == OBIS_REG_POS_AE_NO_TARIFF)			return "pos-act-energy-no-tariff";
			else if (code == OBIS_REG_POS_AE_T1)				return "pos-act-energy-tariff-1";
			else if (code == OBIS_REG_POS_AE_T2)				return "pos-act-energy-tariff-2";
			else if (code == OBIS_REG_NEG_AE_NO_TARIFF)			return "neg-act-energy-no-tariff";
			else if (code == OBIS_REG_NEG_AE_T1)				return "neg-act-energy-tariff-1";
			else if (code == OBIS_REG_NEG_AE_T2)				return "neg-act-energy-tariff-2";
			else if (code == OBIS_REG_CUR_POS_AE)				return "pos-act-energy-current";
			else if (code == OBIS_REG_CUR_AP)					return "act-power-current";

			//																				//		const static obis	OBIS_CLASS_OP_LOG_EVENT(0x81, 0x81, 0xC7, 0x89, 0xE2, 0xFF);	//	Ereignis (uint32)
			//const static obis	OBIS_CLASS_OP_LOG_PEER_ADDRESS(0x81, 0x81, 0x00, 0x00, 0x00, 0xFF);	//	Peer Adresse (octet string)
			//const static obis	OBIS_CLASS_OP_MONITORING_STATUS(0x00, 0x80, 0x80, 0x11, 0x13, 0x01);	//	�berwachungsstatus (uint8) - tats�chlich i32
			//																							/**
			//																							*	Statuswort (per Schreiben ist das R�cksetzen ausgew�hlter Statusbits) Unsigned64
			//																							*/
			//const static obis	OBIS_CLASS_OP_LOG_STATUS_WORD(0x81, 0x00, 0x60, 0x05, 0x00, 0x00);
			//const static obis	OBIS_CLASS_OP_LOG_FIELD_STRENGTH(0x81, 0x04, 0x2B, 0x07, 0x00, 0x00);
			//const static obis	OBIS_CLASS_OP_LOG_CELL(0x81, 0x04, 0x1A, 0x07, 0x00, 0x00);	//	aktuelle Zelleninformation (uint16)
			//const static obis	OBIS_CLASS_OP_LOG_AREA_CODE(0x81, 0x04, 0x17, 0x07, 0x00, 0x00);	//	aktueller Location - oder Areacode(uint16)
			//const static obis	OBIS_CLASS_OP_LOG_PROVIDER(0x81, 0x04, 0x0D, 0x06, 0x00, 0x00);	//	aktueller Provider-Identifier(uint32)
			//const static obis	OBIS_CURRENT_UTC(0x01, 0x00, 0x00, 0x09, 0x0B, 0x00);	//	timestamp UTC - SML_Time (Zeitsynchronisation per NTPZugriff erfolgt)

			else if (code == OBIS_CLASS_OP_LSM_STATUS)					return "LSM-status";
			else if (code == OBIS_CLASS_OP_LSM_ACTOR_ID)				return "LSM-actor-id";
			else if (code == OBIS_CLASS_OP_LSM_CONNECT)					return "LSM-connection-status";
			else if (code == OBIS_CLASS_OP_LSM_SWITCH)					return "LSM-switch";
			else if (code == OBIS_CLASS_OP_LSM_FEEDBACK)				return "LSM-feedback";
			else if (code == OBIS_CLASS_OP_LSM_LOAD)					return "LSM-load-count";
			else if (code == OBIS_CLASS_OP_LSM_POWER)					return "LSM-total-power";
			else if (code == OBIS_CLASS_OP_LSM_VERSION)					return "LSM-version";
			else if (code == OBIS_CLASS_OP_LSM_TYPE)					return "LSM-type";
			else if (code == OBIS_CLASS_OP_LSM_ACTIVE_RULESET)			return "LSM-active-ruleset";
			else if (code == OBIS_CLASS_OP_LSM_PASSIVE_RULESET)			return "LSM-passive-ruleset";
			else if (code == OBIS_CLASS_OP_LSM_JOB)						return "LSM-job-name";
			else if (code == OBIS_CLASS_OP_LSM_POSITION)				return "LSM-position";

			//
			//	attention codes
			//
			return get_attention_name(code);
		}

		const char* get_attention_name(obis const& code)
		{
			if (OBIS_ATTENTION_UNKNOWN_ERROR == code) {
				return "UNKNOWN ERROR";
			}
			else if (OBIS_ATTENTION_UNKNOWN_SML_ID == code) {
				return "UNKNOWN SML ID";
			}
			else if (OBIS_ATTENTION_NOT_AUTHORIZED == code) {
				return "NOT AUTHORIZED";
			}
			else if (OBIS_ATTENTION_NO_SERVER_ID == code) {
				return "NO SERVER ID";
			}
			else if (OBIS_ATTENTION_NO_REQ_FIELD == code) {
				return "NO REQ FIELD";
			}
			else if (OBIS_ATTENTION_CANNOT_WRITE == code) {
				return "CANNOT WRITE";
			}
			else if (OBIS_ATTENTION_CANNOT_READ == code) {
				return "CANNOT READ";
			}
			else if (OBIS_ATTENTION_COMM_ERROR == code) {
				return "COMM ERROR";
			}
			else if (OBIS_ATTENTION_PARSER_ERROR == code) {
				return "PARSER ERROR";
			}
			else if (OBIS_ATTENTION_OUT_OF_RANGE == code) {
				return "OUT OF RANGE";
			}
			else if (OBIS_ATTENTION_NOT_EXECUTED == code) {
				return "NOT EXECUTED";
			}
			else if (OBIS_ATTENTION_INVALID_CRC == code) {
				return "INVALID CRC";
			}
			else if (OBIS_ATTENTION_NO_BROADCAST == code) {
				return "NO BROADCAST";
			}
			else if (OBIS_ATTENTION_UNEXPECTED_MSG == code) {
				return "UNEXPECTED MSG";
			}
			else if (OBIS_ATTENTION_UNKNOWN_OBIS_CODE == code) {
				return "UNKNOWN OBIS CODE";
			}
			else if (OBIS_ATTENTION_UNSUPPORTED_DATA_TYPE == code) {
				return "UNSUPPORTED DATA TYPE";
			}
			else if (OBIS_ATTENTION_ELEMENT_NOT_OPTIONAL == code) {
				return "NOT OPTIONAL";
			}
			else if (OBIS_ATTENTION_NO_ENTRIES == code) {
				return "NO ENTRIES";
			}
			else if (OBIS_ATTENTION_OK == code) {
				return "OK";
			}
			else if (OBIS_ATTENTION_JOB_IS_RUNNINNG == code) {
				return "JOB IS RUNNINNG";
			}
			return "no-entry";
		}

		const char* get_LSM_event_name(std::uint32_t evt)
		{
			switch (evt & 0xFFFFFF)
			{
			case 0x810120:
				return "switching operation";
				break;
			case 0x810101:
				return "Regelsatz�nderung";
				break;
			case 0x810102:
				return "job received";
				break;
			case 0x810103:
				return "Kontaktmatrix ge�ndert";
				break;
			case 0x810104:
				return "keine Kontaktmatrix parametriert";
				break;
			case 0x810105:
				return "Objektkonfiguration ge�ndert";
				break;
			case 0x810106:
				return "Konfiguration in LSM programmiert";
				break;
			case 0x810107:
				return "Konfiguration konnte nicht in LSM programmiert werden";
				break;
			case 0x810108:
				return "Abweichende Konfiguration";
				break;
			case 0x810130:
				return "Relaistest";
				break;
			case 0x810131:
				return "Pegel�nderung am MUC Eingangskontakt";
				break;
			case 0x810001:
				return "MUC Zeitbasis unsicher";
				break;
			case 0x810111:
				return "Objektschaltauftrag angenommen";
				break;
			case 0x810112:
				return "Objektschaltauftrag abgelehnt";
				break;
			default:
				break;
			}
			return "unknown";

		}

	}
}