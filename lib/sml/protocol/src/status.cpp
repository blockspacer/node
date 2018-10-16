﻿/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include <smf/sml/status.h>
#include <cyng/factory/set_factory.h>

namespace node
{
	namespace sml
	{

		status::status()
		: word_(0)
		{
			reset();
		}

		status::status(status const& other)
			: word_(other.word_)
		{}

		status& status::operator=(status const& other)
		{
			if (this != &other)
			{
				word_ = other.word_;
			}
			return *this;
		}

		void status::reset()
		{
			//
			//	reset status word
			//
			word_ = 0u;
			word_ |= STATUS_BIT_ON;	//!< bit 1 is always ON
			word_ |= STATUS_BIT_RESET_BY_WATCHDOG;	
			//word_ |= STATUS_BIT_IP_ADDRESS_AVAILABLE;	//!< 1 if NOT available
			word_ |= STATUS_BIT_EXT_IF_AVAILABLE;	//!< 1 if NOT available
			word_ |= STATUS_BIT_AUTHORIZED_IPT;	//!< 1 if NOT authorized
		}

		void status::reset(std::uint32_t word)
		{
			word_ = word;
		}

		status::operator std::uint64_t() const
		{
			return word_;
		}

		void status::set_fatal_error(bool b)
		{
			(b) ? set(STATUS_BIT_FATAL_ERROR) : remove(STATUS_BIT_FATAL_ERROR);
		}

		void status::set_authorized(bool b)
		{
			//	0 if authorized
			(b) ? remove(STATUS_BIT_AUTHORIZED_IPT) : set(STATUS_BIT_AUTHORIZED_IPT);
		}

		void status::set_ip_address_available(bool b)
		{
			//	0 if IP address is available (DHCP)
			(b) ? remove(STATUS_BIT_IP_ADDRESS_AVAILABLE) : set(STATUS_BIT_IP_ADDRESS_AVAILABLE);
		}

		void status::set_ethernet_link_available(bool b)
		{
			//	0 if ethernet link / GSM network is available
			b ? remove(STATUS_BIT_ETHERNET_AVAILABLE) : set(STATUS_BIT_ETHERNET_AVAILABLE);
		}

		void status::set_service_if_available(bool b)
		{
			(b) ? set(STATUS_BIT_SERVICE_IF_AVAILABLE) : remove(STATUS_BIT_SERVICE_IF_AVAILABLE);
		}

		void status::set_ext_if_available(bool b)
		{
			(b) ? set(STATUS_BIT_EXT_IF_AVAILABLE) : remove(STATUS_BIT_EXT_IF_AVAILABLE);
		}

		void status::set_mbus_if_available(bool b)
		{
			(b) ? set(STATUS_BIT_MBUS_IF_AVAILABLE) : remove(STATUS_BIT_MBUS_IF_AVAILABLE);
		}

		bool status::is_authorized() const
		{
			return !is_set(STATUS_BIT_AUTHORIZED_IPT);
		}

		bool status::is_fatal_error() const
		{
			return is_set(STATUS_BIT_FATAL_ERROR);
		}

		bool status::is_out_of_memory() const
		{
			return is_set(STATUS_BIT_OUT_OF_MEMORY);
		}

		bool status::is_service_if_available() const
		{
			return is_set(STATUS_BIT_SERVICE_IF_AVAILABLE);
		}

		bool status::is_ext_if_available() const 
		{
			return is_set(STATUS_BIT_EXT_IF_AVAILABLE);
		}

		bool status::is_mbus_available() const
		{
			return is_set(STATUS_BIT_MBUS_IF_AVAILABLE);
		}

		bool status::is_plc_available() const
		{
			return is_set(STATUS_BIT_PLC_AVAILABLE);
		}

		bool status::is_timebase_uncertain() const
		{
			return is_set(STATUS_BIT_NO_TIMEBASE);
		}

		bool status::is_set(status_bits e) const
		{
			return (word_ & e) == e;
		}

		void status::set(status_bits e)
		{
			word_ |= e;
		}

		void status::remove(status_bits e)
		{
			word_ &= ~e;
		}

		cyng::attr_map_t to_attr_map(status const& word)
		{
			return cyng::attr_map_factory
                (sml::STATUS_BIT_AUTHORIZED_IPT, word.is_authorized())
				(sml::STATUS_BIT_FATAL_ERROR, word.is_fatal_error())
				(sml::STATUS_BIT_OUT_OF_MEMORY, word.is_out_of_memory())
				(sml::STATUS_BIT_SERVICE_IF_AVAILABLE, word.is_service_if_available())
				(sml::STATUS_BIT_EXT_IF_AVAILABLE, word.is_ext_if_available())
				(sml::STATUS_BIT_MBUS_IF_AVAILABLE, word.is_mbus_available())
				(sml::STATUS_BIT_PLC_AVAILABLE, word.is_plc_available())
				(sml::STATUS_BIT_NO_TIMEBASE, word.is_timebase_uncertain())
				;
		}

	}	//	sml
}

