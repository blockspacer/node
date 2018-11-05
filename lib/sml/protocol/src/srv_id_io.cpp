/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */


#include <smf/sml/srv_id_io.h>
#include <smf/sml/defs.h>
#include <cyng/io/io_buffer.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <boost/io/ios_state.hpp>

namespace node
{
	namespace sml
	{
		bool is_mbus(cyng::buffer_t const& buffer)
		{
			return ((buffer.size() == 9)
				&& (buffer.at(0) == MBUS_WIRELESS || buffer.at(0) == MBUS_WIRED));
		}

		bool is_w_mbus(cyng::buffer_t const& buffer)
		{
			//	Each meter (or radio module connected to the meter) has a unique 8 byte ID or address. 
			//	The address consists of 
			//	* a Manufacturer ID (2 Bytes), 
			//	* a serial number (4 Bytes), 
			//	* the device type (e.g. water meter) (1 Byte) and 
			//	* the version of the meter (product revision). (1 Bytes)
			if (buffer.size() == 8)
			{
				return (buffer.at(6) >= 0x00) && (buffer.at(6) <= 0x0F);
			}
			return false;
		}

		bool is_serial(cyng::buffer_t const& buffer)
		{
			if (buffer.size() == 8)
			{
				return std::all_of(buffer.begin(), buffer.end(), [](char c) {
					return ((c >= '0') && (c <= '9'))
						|| ((c >= 'A') && (c <= 'Z'));
				});
			}
			return false;
		}

		bool is_gateway(cyng::buffer_t const& buffer)
		{
			return ((buffer.size() == 7) && (buffer.at(0) == MAC_ADDRESS));
		}

		bool is_dke_1(cyng::buffer_t const& buffer)
		{
			return (buffer.size() == 10) 
				&& (buffer.at(0) == '0') 
				&& (buffer.at(1) == '6');
		}

		bool is_dke_2(cyng::buffer_t const& buffer)
		{
			return (buffer.size() == 10)
				&& (buffer.at(0) == '0')
				&& (buffer.at(1) == '9');
		}

		std::uint32_t get_srv_type(cyng::buffer_t const& buffer)
		{
			if (is_mbus(buffer))	return SRV_MBUS;
			if (is_serial(buffer))	return SRV_SERIAL;
			if (is_gateway(buffer))	return SRV_GW;
			if (buffer.size() == 10 && buffer.at(1) == '3')	return SRV_BCD;
			if (buffer.size() == 8 && buffer.at(2) == '4')	return SRV_EON;
			if (is_dke_1(buffer))	return SRV_DKE_1;
			if (is_dke_2(buffer))	return SRV_DKE_2;

			return SRV_OTHER;
		}

		bool is_mbus(std::string const& str)
		{
			//example: 02-e61e-03197715-3c-07
			const auto size = str.size();
			if (size == 22 
				&& (str.at(0) == '0')
				&& ((str.at(1) == '1') || (str.at(1) == '2'))
				&& (str.at(2) == '-')
				&& (str.at(7) == '-')
				&& (str.at(16) == '-')
				&& (str.at(19) == '-')) {

				//
				//	only hex values allowed
				//
				return std::all_of(str.begin(), str.end(), [](char c) {
					return ((c >= '0') && (c <= '9'))
						|| ((c >= 'a') && (c <= 'f'))
						|| ((c >= 'A') && (c <= 'F'))
						|| ((c == '-'))
						;
				});
			}
			return false;
		}

		bool is_serial(std::string const& str)
		{
			//	example: 05823740
			const auto size = str.size();
			if (size == 8) {

				//
				//	only decimal values allowed
				//
				return std::all_of(str.begin(), str.end(), [](char c) {
					return ((c >= '0') && (c <= '9'));
				});
			}
			return false;
		}

		void serialize_server_id(std::ostream& os, cyng::buffer_t const& buffer)
		{
			//	store and reset stream state
			boost::io::ios_flags_saver  ifs(os);

			if (is_mbus(buffer))
			{
				//
				//	serial number with M-bus prefix
				//	example: 01-e61e-13090016-3c-07
				//
				os
					<< std::hex
					<< std::setfill('0')
					;

				std::size_t counter{ 0 };
				for (auto c : buffer)
				{ 
					os << std::setw(2) << (+c & 0xFF);
					counter++;
					switch (counter)
					{
					case 1: case 3: case 7: case 8:
						os << '-';
						break;
					default:
						break;
					}
				}
			}
			else if (is_serial(buffer))
			{
				//
				//	serial number ASCII encoded
				//
				os << cyng::io::to_ascii(buffer);
			}
			else if (is_gateway(buffer))
			{
				//
				//	MAC as serial number
				//
				os
					<< std::hex
					<< std::setfill('0')
					;

				//	skip leading 0x05
				std::size_t counter{ 0 };
				for (auto c : buffer)
				{
					if (counter > 0) {
						if (counter > 1) {
							os << ':';
						}
						os << std::setw(2) << (+c & 0xFF);
					}
					++counter;
				}
			}
			else if (buffer.size() == 8)
			{
				//
				//	serial number without prefix
				//
				//	e.g. 2d2c688668691c04 => 2d2c-68866869-1c-04
				os
					<< std::hex
					<< std::setfill('0')
					;

				std::size_t counter{ 0 };
				for (auto c : buffer)
				{
					os << std::setw(2) << (+c & 0xFF);
					counter++;
					switch (counter)
					{
					case 2: case 6: case 7:
						os << '-';
						break;
					default:
						break;
					}
				}
			}
			else
			{
				//
				//	something else
				//
				os
					<< std::hex
					<< std::setfill('0')
					;
				for (auto c : buffer)
				{
					os << std::setw(2) << (+c & 0xFF);
				}
			}
		}

		std::string from_server_id(cyng::buffer_t const& buffer)
		{
			std::ostringstream ss;
			serialize_server_id(ss, buffer);
			return ss.str();
		}

		std::string get_serial(cyng::buffer_t const& buffer)
		{
			if (is_mbus(buffer))
			{
				std::stringstream ss;
				ss
					<< std::hex
					<< std::setfill('0')
					<< std::setw(2)
					<< (+buffer.at(6) & 0xFF)
					<< std::setw(2)
					<< (+buffer.at(5) & 0xFF)
					<< std::setw(2)
					<< (+buffer.at(4) & 0xFF)
					<< std::setw(2)
					<< (+buffer.at(3) & 0xFF)
					;
				return ss.str();
			}
			else if (is_serial(buffer))
			{
				return cyng::io::to_ascii(buffer);
			}

			return "00000000";
		}

		std::string get_serial(std::string const& str)
		{
			if (is_mbus(str))
			{
				//example: 02-e61e-03197715-3c-07
				return str.substr(14, 2)
					+ str.substr(12, 2)
					+ str.substr(10, 2)
					+ str.substr(8, 2)
					;
			}
			else if (is_serial(str))
			{
				return str;
			}

			return str;
		}

		cyng::buffer_t to_gateway_srv_id(cyng::mac48 mac)
		{
			cyng::buffer_t buffer(mac.to_buffer());
			buffer.insert(buffer.begin(), 0x05);
			return buffer;
		}

	}
}