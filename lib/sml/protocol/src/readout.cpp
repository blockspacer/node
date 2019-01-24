/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include <smf/sml/protocol/readout.h>
#include <smf/sml/obis_io.h>

#include <cyng/io/io_buffer.h>
#include <cyng/io/io_chrono.hpp>
#include <cyng/io/serializer.h>
#include <cyng/factory.h>

namespace node
{
	namespace sml
	{


		readout::readout(boost::uuids::uuid pk)
			: pk_(pk)
			, idx_(0)
			, trx_()
			, server_id_()
			, client_id_()
			, values_()
		{}

		void readout::reset(boost::uuids::uuid pk, std::size_t idx)
		{
			pk_ = pk;
			idx_ = idx;
			trx_.clear();
			server_id_.clear();
			client_id_.clear();
			values_.clear();
		}

		readout& readout::set_trx(cyng::buffer_t const& buffer)
		{
			trx_ = cyng::io::to_ascii(buffer);
			return *this;
		}

		readout& readout::set_index(std::size_t idx)
		{
			idx_ = idx;
			return *this;
		}

		readout& readout::set_value(std::string const& name, cyng::object obj)
		{
			values_[name] = obj;
			return *this;
		}

		readout& readout::set_value(obis code, cyng::object obj)
		{
			values_[to_hex(code)] = obj;
			return *this;
		}

		readout& readout::set_value(obis code, std::string name, cyng::object obj)
		{
			BOOST_ASSERT_MSG(!name.empty(), "name for map entry is empty");
			if (values_.find(name) == values_.end()) {

				//
				//	new element
				//
				auto map = cyng::param_map_factory(to_hex(code), obj)();
				values_.emplace(name, map);
			}
			else {

				//
				//	more elements
				//
				auto p = cyng::object_cast<cyng::param_map_t>(values_.at(name));
				if (p != nullptr) {
					const_cast<cyng::param_map_t*>(p)->emplace(to_hex(code), obj);
				}
			}
			return *this;
		}

		cyng::object readout::get_value(std::string const& name) const
		{
			auto pos = values_.find(name);
			return (pos != values_.end())
				? pos->second
				: cyng::make_object()
				;
		}

		cyng::object readout::get_value(obis code) const
		{
			return get_value(to_hex(code));
		}

		std::string readout::get_string(obis code) const
		{
			auto obj = get_value(code);
			cyng::buffer_t buffer;
			buffer = cyng::value_cast(obj, buffer);
			return std::string(buffer.begin(), buffer.end());
		}

	}	//	sml
}

