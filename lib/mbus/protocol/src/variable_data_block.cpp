/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 
#include <smf/mbus/variable_data_block.h>
// #include <smf/mbus/defs.h>
// #include <cyng/vm/generator.h>
 #include <iostream>
// #include <sstream>
// #include <ios>
// #include <iomanip>
// #include <boost/numeric/conversion/cast.hpp>
#include <boost/assert.hpp>

namespace node 
{
	variable_data_block::variable_data_block()
	: length_(0)
	, storage_nr_(0)
	, tariff_(0)
	, sub_unit_(0)
	, scaler_(0)
	{}
	
	std::size_t variable_data_block::decode(cyng::buffer_t const& data, std::size_t offset, std::size_t size)
	{
		//
		//	0 - instantaneous value
		//	1 - maximum value
		//	2 - minimum value
		//	all other values signal an error
		//
		std::uint8_t func_field = (data.at(offset) & 0x30) >> 4;
		BOOST_ASSERT_MSG(func_field < 3, "function field out of range");
		
		//
		//	length (and data type)
		//
 		length_ = (data.at(offset) & 0x0F);	
 		
 		//
 		//	initial value
 		//
 		storage_nr_= (data.at(offset) & 0x40);
		
		//
		//	finalize DIB
		//
		offset = finalize_dib(data, offset);
		
		//
		//	decode VIB
		//
		
		//
		//	get VIF
		//
		offset = decode_vif(data, offset);
		
		return offset;
	}
	
	std::size_t variable_data_block::finalize_dib(cyng::buffer_t const& data, std::size_t offset)
	{
		int pos{ 0 };
		while ((data.at(offset++) & 0x80) == 0x80) {
			sub_unit_ += (((data.at(offset) & 0x40) >> 6) << pos);
			tariff_ += ((data.at(offset) & 0x30) >> 4) << (pos * 2);
			storage_nr_ += ((data.at(offset) & 0x0F) << ((pos * 4) + 1));
			pos++;
		}
		
		return offset;
	}
	
	std::size_t variable_data_block::decode_vif(cyng::buffer_t const& data, std::size_t offset)
	{
		const std::uint8_t vif = data.at(offset++) & 0xFF;
		bool more_vifs = false;
		
        if (vif == 0xfb) {
//             decodeAlternateExtendedVif(data.at(offset));
            if ((data.at(offset) & 0x80) == 0x80) {
                more_vifs = true;
            }
            ++offset;
        }
        else if ((vif & 0x7f) == 0x7c) {
//             offset += decodeUserDefinedVif(buffer, offset);
            if ((vif & 0x80) == 0x80) {
                more_vifs = true;
            }
        }
        else if (vif == 0xfd) {
//             decodeMainExtendedVif(data.at(offset));
            if ((data.at(offset) & 0x80) == 0x80) {
                more_vifs = true;
            }
            ++offset;
        }
        else {
            decode_main_vif(vif);
            if ((vif & 0x80) == 0x80) {
                more_vifs = true;
            }
        }

        if (more_vifs) {
            while ((data.at(offset++) & 0x80) == 0x80) {
                // ToDO: implement
            }
        }
		
		return offset;
	}
	
	void variable_data_block::decode_main_vif(std::uint8_t vif)
	{
        if ((vif & 0x40) == 0) {
            read_e0(vif);
        }
        else {
            read_e1(vif);
        }
	}
	
    void variable_data_block::read_e1(std::uint8_t vif) 
	{
        // E1
        if ((vif & 0x20) == 0) {
            read_e10(vif);
        }
        else {
            read_e11(vif);
        }
    }

    void variable_data_block::read_e11(std::uint8_t vif) 
	{
        // E11
        if ((vif & 0x10) == 0) {
            read_e110(vif);
        }
        else {
            read_e111(vif);
        }
    }

    void variable_data_block::read_e111(std::uint8_t vif) 
	{
        // E111
        if ((vif & 0x08) == 0) {
            // E111 0
            if ((vif & 0x04) == 0) {
//                 description = Description.AVERAGING_DURATION;
            }
            else {
//                 description = Description.ACTUALITY_DURATION;
            }
            decodeTimeUnit(vif);
        }
        else {
            // E111 1
            if ((vif & 0x04) == 0) {
                // E111 10
                if ((vif & 0x02) == 0) {
                    // E111 100
                    if ((vif & 0x01) == 0) {
                        // E111 1000
//                         description = Description.FABRICATION_NO;
                    }
                    else {
                        // E111 1001
//                         description = Description.EXTENDED_IDENTIFICATION;
                    }
                }
                else {
                    // E111 101
                    if ((vif & 0x01) == 0) {
//                         description = Description.ADDRESS;
                    }
                    else {
                        // E111 1011
                        // Codes used with extension indicator 0xFB (table 29 of DIN EN 13757-3:2011)
						std::cerr << "Trying to decode a mainVIF even though it is an alternate extended vif" << std::endl;
						BOOST_ASSERT_MSG(false, "Trying to decode a mainVIF even though it is an alternate extended vif");
                    }
                }
            }
            else {
                // E111 11
                if ((vif & 0x02) == 0) {
                    // E111 110
                    if ((vif & 0x01) == 0) {
                        // E111 1100
                        // Extension indicator 0xFC: VIF is given in following string
//                         description = Description.NOT_SUPPORTED;
                    }
                    else {
                        // E111 1101
                        // Extension indicator 0xFD: main VIFE-code extension table (table 28 of DIN EN
                        // 13757-3:2011)
						std::cerr << "Trying to decode a mainVIF even though it is a main extended vif" << std::endl;
						BOOST_ASSERT_MSG(false, "Trying to decode a mainVIF even though it is a main extended vif");
                    }
                }
                else {
                    // E111 111
                    if ((vif & 0x01) == 0) {
                        // E111 1110
//                         description = Description.FUTURE_VALUE;
                    }
                    else {
                        // E111 1111
//                         description = Description.MANUFACTURER_SPECIFIC;
                    }
                }
            }
        }
    }

    void variable_data_block::read_e110(std::uint8_t vif) 
	{
        // E110
        if ((vif & 0x08) == 0) {
            // E110 0
            if ((vif & 0x04) == 0) {
                // E110 00
//                 description = Description.TEMPERATURE_DIFFERENCE;
                scaler_ = (vif & 0x03) - 3;
                unit = DlmsUnit.KELVIN;
            }
            else {
                // E110 01
//                 description = Description.EXTERNAL_TEMPERATURE;
                scaler_ = (vif & 0x03) - 3;
                unit = DlmsUnit.DEGREE_CELSIUS;
            }
        }
        else {
            // E110 1
            if ((vif & 0x04) == 0) {
                // E110 10
//                 description = Description.PRESSURE;
                scaler_ = (vif & 0x03) - 3;
                unit = DlmsUnit.BAR;
            }
            else {
                // E110 11
                if ((vif & 0x02) == 0) {
                    // E110 110
                    if ((vif & 0x01) == 0) {
                        // E110 1100
//                         description = Description.DATE;
                        dateTypeG = true;
                    }
                    else {
                        // E110 1101
//                         description = Description.DATE_TIME;
                        dateTypeF = true;
                    }
                }
                else {
                    // E110 111
                    if ((vif & 0x01) == 0) {
                        // E110 1110
//                         description = Description.HCA;
                        unit = DlmsUnit.RESERVED;
                    }
                    else {
//                         description = Description.NOT_SUPPORTED;
                    }

                }

            }
        }
    }

    void variable_data_block::read_e10(std::uint8_t vif) 
	{
        // E10
        if ((vif & 0x10) == 0) {
            // E100
            if ((vif & 0x08) == 0) {
                // E100 0
//                 description = Description.VOLUME_FLOW_EXT;
                scaler_ = (vif & 0x07) - 7;
                unit = DlmsUnit.CUBIC_METRE_PER_MINUTE;
            }
            else {
                // E100 1
//                 description = Description.VOLUME_FLOW_EXT;
                scaler_ = (vif & 0x07) - 9;
                unit = DlmsUnit.CUBIC_METRE_PER_SECOND;
            }
        }
        else {
            // E101
            if ((vif & 0x08) == 0) {
                // E101 0
//                 description = Description.MASS_FLOW;
                scaler_ = (vif & 0x07) - 3;
                unit = DlmsUnit.KILOGRAM_PER_HOUR;
            }
            else {
                // E101 1
                if ((vif & 0x04) == 0) {
                    // E101 10
//                     description = Description.FLOW_TEMPERATURE;
                    scaler_ = (vif & 0x03) - 3;
                    unit = DlmsUnit.DEGREE_CELSIUS;
                }
                else {
                    // E101 11
//                     description = Description.RETURN_TEMPERATURE;
                    scaler_ = (vif & 0x03) - 3;
                    unit = DlmsUnit.DEGREE_CELSIUS;
                }
            }
        }
    }

    void variable_data_block::read_e0(std::uint8_t vif) 
	{
        // E0
        if ((vif & 0x20) == 0) {
            read_e00(vif);
        }
        else {
            read_01(vif);
        }
    }

    void variable_data_block::read_01(std::uint8_t vif) 
	{
        // E01
        if ((vif & 0x10) == 0) {
            // E010
            if ((vif & 0x08) == 0) {
                // E010 0
                if ((vif & 0x04) == 0) {
                    // E010 00
//                     description = Description.ON_TIME;
                }
                else {
                    // E010 01
//                     description = Description.OPERATING_TIME;
                }
                decodeTimeUnit(vif);
            }
            else {
                // E010 1
//                 description = Description.POWER;
                scaler_ = (vif & 0x07) - 3;
                unit = DlmsUnit.WATT;
            }
        }
        else {
            // E011
            if ((vif & 0x08) == 0) {
                // E011 0
//                 description = Description.POWER;
                scaler_ = vif & 0x07;
                unit = DlmsUnit.JOULE_PER_HOUR;
            }
            else {
                // E011 1
//                 description = Description.VOLUME_FLOW;
                scaler_ = (vif & 0x07) - 6;
                unit = DlmsUnit.CUBIC_METRE_PER_HOUR;
            }
        }
    }

    void variable_data_block::read_e00(std::uint8_t vif) 
	{
        // E00
        if ((vif & 0x10) == 0) {
            // E000
            if ((vif & 0x08) == 0) {
                // E000 0
//                 description = Description.ENERGY;
                scaler_ = (vif & 0x07) - 3;
                unit = DlmsUnit.WATT_HOUR;
            }
            else {
                // E000 1
//                 description = Description.ENERGY;
                scaler_ = vif & 0x07;
                unit = DlmsUnit.JOULE;
            }
        }
        else {
            // E001
            if ((vif & 0x08) == 0) {
                // E001 0
//                 description = Description.VOLUME;
                scaler_ = (vif & 0x07) - 6;
                unit = DlmsUnit.CUBIC_METRE;
            }
            else {
                // E001 1
//                 description = Description.MASS;
                scaler_ = (vif & 0x07) - 3;
                unit = DlmsUnit.KILOGRAM;
            }
        }
    }

    void variable_data_block::decodeTimeUnit(std::uint8_t vif) 
	{
        if ((vif & 0x02) == 0) {
            if ((vif & 0x01) == 0) {
                unit = DlmsUnit.SECOND;
            }
            else {
                unit = DlmsUnit.MIN;
            }
        }
        else {
            if ((vif & 0x01) == 0) {
                unit = DlmsUnit.HOUR;
            }
            else {
                unit = DlmsUnit.DAY;
            }
        }
    }
    
    // implements table 28 of DIN EN 13757-3:2013
    void variable_data_block::decode_main_ext_vif(std::uint8_t vif)
    {
//         if ((vif & 0x7f) == 0x0b) { // E000 1011
//             description = Description.PARAMETER_SET_ID;
//         }
//         else if ((vif & 0x7f) == 0x0c) { // E000 1100
//             description = Description.MODEL_VERSION;
//         }
//         else if ((vif & 0x7f) == 0x0d) { // E000 1101
//             description = Description.HARDWARE_VERSION;
//         }
//         else if ((vif & 0x7f) == 0x0e) { // E000 1110
//             description = Description.FIRMWARE_VERSION;
//         }
//         else if ((vif & 0x7f) == 0x0f) { // E000 1111
//             description = Description.OTHER_SOFTWARE_VERSION;
//         }
//         else if ((vif & 0x7f) == 0x10) { // E001 0000
//             description = Description.CUSTOMER_LOCATION;
//         }
//         else if ((vif & 0x7f) == 0x11) { // E001 0001
//             description = Description.CUSTOMER;
//         }
//         else if ((vif & 0x7f) == 0x12) { // E001 0010
//             description = Description.ACCSESS_CODE_USER;
//         }
//         else if ((vif & 0x7f) == 0x13) { // E001 0011
//             description = Description.ACCSESS_CODE_OPERATOR;
//         }
//         else if ((vif & 0x7f) == 0x14) { // E001 0100
//             description = Description.ACCSESS_CODE_SYSTEM_OPERATOR;
//         }
//         else if ((vif & 0x7f) == 0x15) { // E001 0101
//             description = Description.ACCSESS_CODE_SYSTEM_DEVELOPER;
//         }
//         else if ((vif & 0x7f) == 0x16) { // E001 0110
//             description = Description.PASSWORD;
//         }
//         else if ((vif & 0x7f) == 0x17) { // E001 0111
//             description = Description.ERROR_FLAGS;
//         }
//         else if ((vif & 0x7f) == 0x18) { // E001 1000
//             description = Description.ERROR_MASK;
//         }
//         else if ((vif & 0x7f) == 0x19) { // E001 1001
//             description = Description.SECURITY_KEY;
//         }
//         else if ((vif & 0x7f) == 0x1a) { // E001 1010
//             description = Description.DIGITAL_OUTPUT;
//         }
//         else if ((vif & 0x7f) == 0x1b) { // E001 1011
//             description = Description.DIGITAL_INPUT;
//         }
//         else if ((vif & 0x7f) == 0x1c) { // E001 1100
//             description = Description.BAUDRATE;
//         }
//         else if ((vif & 0x7f) == 0x1d) { // E001 1101
//             description = Description.RESPONSE_DELAY_TIME;
//         }
//         else if ((vif & 0x7f) == 0x1e) { // E001 1110
//             description = Description.RETRY;
//         }
//         else if ((vif & 0x7f) == 0x1f) { // E001 1111
//             description = Description.REMOTE_CONTROL;
//         }
//         else if ((vif & 0x7f) == 0x20) { // E010 0000
//             description = Description.FIRST_STORAGE_NUMBER_CYCLIC;
//         }
//         else if ((vif & 0x7f) == 0x21) { // E010 0001
//             description = Description.LAST_STORAGE_NUMBER_CYCLIC;
//         }
//         else if ((vif & 0x7f) == 0x22) { // E010 0010
//             description = Description.SIZE_STORAGE_BLOCK;
//         }
//         else if ((vif & 0x7f) == 0x23) { // E010 0011
//             description = Description.RESERVED;
//         }
//         else if ((vif & 0x7c) == 0x24) { // E010 01nn
//             description = Description.STORAGE_INTERVALL;
//             this.unit = unitFor(vif);
//         }
//         else if ((vif & 0x7f) == 0x28) { // E010 1000
//             description = Description.STORAGE_INTERVALL;
//             unit = DlmsUnit.MONTH;
//         }
//         else if ((vif & 0x7f) == 0x29) { // E010 1001
//             description = Description.STORAGE_INTERVALL;
//             unit = DlmsUnit.YEAR;
//         }
//         else if ((vif & 0x7f) == 0x2a) { // E010 1010
//             description = Description.OPERATOR_SPECIFIC_DATA;
//         }
//         else if ((vif & 0x7f) == 0x2b) { // E010 1011
//             description = Description.TIME_POINT;
//             unit = DlmsUnit.SECOND;
//         }
//         else if ((vif & 0x7c) == 0x2c) { // E010 11nn
//             description = Description.DURATION_LAST_READOUT;
//             this.unit = unitFor(vif);
//         }
//         else if ((vif & 0x7c) == 0x30) { // E011 00nn
//             description = Description.TARIF_DURATION;
//             switch (vif & 0x03) {
//             case 0: // E011 0000
//                 description = Description.NOT_SUPPORTED; // TODO: TARIF_START (Date/Time)
//                 break;
//             default:
//                 this.unit = unitFor(vif);
//             }
//         }
//         else if ((vif & 0x7c) == 0x34) { // E011 01nn
//             description = Description.TARIF_PERIOD;
//             this.unit = unitFor(vif);
//         }
//         else if ((vif & 0x7f) == 0x38) { // E011 1000
//             description = Description.TARIF_PERIOD;
//             unit = DlmsUnit.MONTH;
//         }
//         else if ((vif & 0x7f) == 0x39) { // E011 1001
//             description = Description.TARIF_PERIOD;
//             unit = DlmsUnit.YEAR;
//         }
//         else if ((vif & 0x70) == 0x40) { // E100 0000
//             description = Description.VOLTAGE;
//             scaler_ = (vif & 0x0f) - 9;
//             unit = DlmsUnit.VOLT;
//         }
//         else if ((vif & 0x70) == 0x50) { // E101 0000
//             description = Description.CURRENT;
//             scaler_ = (vif & 0x0f) - 12;
//             unit = DlmsUnit.AMPERE;
//         }
//         else if ((vif & 0x7f) == 0x60) { // E110 0000
//             description = Description.RESET_COUNTER;
//         }
//         else if ((vif & 0x7f) == 0x61) { // E110 0001
//             description = Description.CUMULATION_COUNTER;
//         }
//         else if ((vif & 0x7f) == 0x62) { // E110 0010
//             description = Description.CONTROL_SIGNAL;
//         }
//         else if ((vif & 0x7f) == 0x63) { // E110 0011
//             description = Description.DAY_OF_WEEK; // 1 = Monday; 7 = Sunday; 0 = all Days
//         }
//         else if ((vif & 0x7f) == 0x64) { // E110 0100
//             description = Description.WEEK_NUMBER;
//         }
//         else if ((vif & 0x7f) == 0x65) { // E110 0101
//             description = Description.TIME_POINT_DAY_CHANGE;
//         }
//         else if ((vif & 0x7f) == 0x66) { // E110 0110
//             description = Description.PARAMETER_ACTIVATION_STATE;
//         }
//         else if ((vif & 0x7f) == 0x67) { // E110 0111
//             description = Description.SPECIAL_SUPPLIER_INFORMATION;
//         }
//         else if ((vif & 0x7c) == 0x68) { // E110 10nn
//             description = Description.LAST_CUMULATION_DURATION;
//             this.unit = unitBiggerFor(vif);
//         }
//         else if ((vif & 0x7c) == 0x6c) { // E110 11nn
//             description = Description.OPERATING_TIME_BATTERY;
//             this.unit = unitBiggerFor(vif);
//         }
//         else if ((vif & 0x7f) == 0x70) { // E111 0000
//             description = Description.NOT_SUPPORTED; // TODO: BATTERY_CHANGE_DATE_TIME
//         }
//         else if ((vif & 0x7f) == 0x71) { // E111 0001
//             description = Description.NOT_SUPPORTED; // TODO: RF_LEVEL dBm
//         }
//         else if ((vif & 0x7f) == 0x72) { // E111 0010
//             description = Description.NOT_SUPPORTED; // TODO: DAYLIGHT_SAVING (begin, ending, deviation)
//         }
//         else if ((vif & 0x7f) == 0x73) { // E111 0011
//             description = Description.NOT_SUPPORTED; // TODO: Listening window management data type L
//         }
//         else if ((vif & 0x7f) == 0x74) { // E111 0100
//             description = Description.REMAINING_BATTERY_LIFE_TIME;
//             unit = DlmsUnit.DAY;
//         }
//         else if ((vif & 0x7f) == 0x75) { // E111 0101
//             description = Description.NUMBER_STOPS;
//         }
//         else if ((vif & 0x7f) == 0x76) { // E111 0110
//             description = Description.MANUFACTURER_SPECIFIC;
//         }
//         else if ((vif & 0x7f) >= 0x77) { // E111 0111 - E111 1111
//             description = Description.RESERVED;
//         }
//         else {
//             description = Description.NOT_SUPPORTED;
//         }
    }
	
}	//	node

