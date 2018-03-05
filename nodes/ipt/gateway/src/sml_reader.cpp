/*
* Copyright Sylko Olzscher 2016
*
* Use, modification, and distribution is subject to the Boost Software
* License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
* http://www.boost.org/LICENSE_1_0.txt)
*/

#include "sml_reader.h"
#include <smf/sml/obis_db.h>
#include <smf/sml/obis_io.h>
#include <smf/sml/srv_id_io.h>
#include <smf/sml/units.h>
#include <smf/sml/scaler.h>

#include <cyng/io/io_buffer.h>
#include <cyng/io/io_chrono.hpp>
#include <cyng/value_cast.hpp>
#include <cyng/xml.h>
#include <cyng/factory.h>
#include <cyng/vm/generator.h>

#include <boost/uuid/nil_generator.hpp>

namespace node
{
	namespace sml
	{

		sml_reader::sml_reader()
			: rgn_()
			, ro_(rgn_())
		{
			reset();
		}

		void sml_reader::reset()
		{
			ro_.reset(rgn_(), 0);
		}

		void sml_reader::read(cyng::context& ctx, cyng::tuple_t const& msg, std::size_t idx)
		{
			read_msg(ctx, msg.begin(), msg.end(), idx);
		}

		void sml_reader::read_msg(cyng::context& ctx, cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end, std::size_t idx)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 5, "SML message");

			//
			//	delayed output
			//
			ctx.attach(cyng::generate_invoke("log.msg.debug"
				, "message #"
				, idx));

			//
			//	reset readout context
			//
			ro_.set_index(idx);

			//
			//	(1) - transaction id
			//	This is typically a printable sequence of 6 up to 9 ASCII values
			//
			cyng::buffer_t buffer;
			ro_.set_trx(cyng::value_cast(*pos++, buffer));

			//
			//	(2) groupNo
			//
			ro_.set_value("groupNo", *pos++);

			//
			//	(3) abortOnError
			//
			ro_.set_value("abortOnError", *pos++);

			//
			//	(4/5) CHOICE - msg type
			//
			cyng::tuple_t choice;
			choice = cyng::value_cast(*pos++, choice);
			BOOST_ASSERT_MSG(choice.size() == 2, "CHOICE");
			if (choice.size() == 2)
			{
				ro_.set_value("code", choice.front());
				read_body(ctx, choice.front(), choice.back());
			}

			//
			//	(6) CRC16
			//
			ro_.set_value("crc16", *pos);
		}

		void sml_reader::read_body(cyng::context& ctx, cyng::object type, cyng::object body)
		{
			auto code = cyng::value_cast<std::uint16_t>(type, 0);

			cyng::tuple_t tpl;
			tpl = cyng::value_cast(body, tpl);

			switch (code)
			{
			case BODY_OPEN_REQUEST:
				read_public_open_request(ctx, tpl.begin(), tpl.end());
				break;
			case BODY_OPEN_RESPONSE:
				read_public_open_response(tpl.begin(), tpl.end());
				break;
			case BODY_CLOSE_REQUEST:
				read_public_close_request(ctx, tpl.begin(), tpl.end());
				break;
			case BODY_CLOSE_RESPONSE:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_PROFILE_PACK_REQUEST:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_PROFILE_PACK_RESPONSE:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_PROFILE_LIST_REQUEST:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_PROFILE_LIST_RESPONSE:
				read_get_profile_list_response(ctx, tpl.begin(), tpl.end());
				break;
			case BODY_GET_PROC_PARAMETER_REQUEST:
				read_get_proc_parameter_request(ctx, tpl.begin(), tpl.end());
				break;
			case BODY_GET_PROC_PARAMETER_RESPONSE:
				read_get_proc_parameter_response(ctx, tpl.begin(), tpl.end());
				break;
			case BODY_SET_PROC_PARAMETER_REQUEST:
				//cyng::xml::write(node, body);
				break;
			case BODY_SET_PROC_PARAMETER_RESPONSE:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_LIST_REQUEST:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_LIST_RESPONSE:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_ATTENTION_RESPONSE:
				read_attention_response(tpl.begin(), tpl.end());
				break;
			default:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			}
		}

		void sml_reader::read_public_open_request(cyng::context& ctx, cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 7, "Public Open Request");

			//	codepage "ISO 8859-15"
			ro_.set_value("codepage", *pos++);

			//
			//	clientId (MAC)
			//	Typically 7 bytes to identify gateway/MUC
			read_client_id(*pos++);

			//
			//	reqFileId
			//
			//ro_.set_value("reqFileId", *pos++);
			read_string("reqFileId", *pos++);

			//
			//	serverId
			//
			read_server_id(*pos++);

			//
			//	username
			//
			read_string("userName", *pos++);

			//
			//	password
			//
			read_string("password", *pos++);

			//
			//	sml-Version: default = 1
			//
			ro_.set_value("SMLVersion", *pos++);

			ctx.attach(cyng::generate_invoke("sml.public.open.request"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, ro_.get_value("clientId")
				, ro_.get_value("serverId")
				, ro_.get_value("reqFileId")
				, ro_.get_value("userName")
				, ro_.get_value("password")));

		}

		void sml_reader::read_public_open_response(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 6, "Public Open Response");

			//	codepage "ISO 8859-15"
			ro_.set_value("codepage", *pos++);

			//
			//	clientId (MAC)
			//	Typically 7 bytes to identify gateway/MUC
			read_client_id(*pos++);

			//
			//	reqFileId
			//
			read_string("reqFileId", *pos++);

			//
			//	serverId
			//
			read_server_id(*pos++);

			//
			//	refTime
			//
			ro_.set_value("refTime", *pos++);

			//	sml-Version: default = 1
			ro_.set_value("SMLVersion", *pos++);

		}

		void sml_reader::read_public_close_request(cyng::context& ctx, cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 1, "Public Close Request");

			ro_.set_value("globalSignature", *pos++);

			ctx.attach(cyng::generate_invoke("sml.public.close.request"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_));

		}

		void sml_reader::read_get_profile_list_response(cyng::context& ctx, cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 9, "Get Profile List Response");

			//
			//	serverId
			//
			read_server_id(*pos++);

			//
			//	actTime
			//
			read_time("actTime", *pos++);

			//
			//	regPeriod
			//
			ro_.set_value("regPeriod", *pos++);

			//
			//	parameterTreePath (OBIS)
			//
			std::vector<obis> path = read_param_tree_path(*pos++);

			//
			//	valTime
			//
			read_time("valTime", *pos++);

			//
			//	M-bus status
			//
			ro_.set_value("status", *pos++);

			//	period-List (1)
			//	first we make the TSMLGetProfileListResponse complete, then 
			//	the we store the entries in TSMLPeriodEntry.
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(*pos++, tpl);
			read_period_list(path, tpl.begin(), tpl.end());

			//	rawdata
			ro_.set_value("rawData", *pos++);

			//	periodSignature
			ro_.set_value("signature", *pos++);

			ctx.attach(cyng::generate_invoke("db.insert.meta"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, ro_.get_value("roTime")
				, ro_.get_value("actTime")
				, ro_.get_value("valTime")
				, ro_.get_value("clientId")
				, ro_.get_value("serverId")
				, ro_.get_value("status")));

		}

		void sml_reader::read_get_proc_parameter_response(cyng::context& ctx, cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 3, "Get Proc Parameter Response");

			//
			//	serverId
			//
			read_server_id(*pos++);

			//
			//	parameterTreePath (OBIS)
			//
			std::vector<obis> path = read_param_tree_path(*pos++);

			//
			//	parameterTree
			//
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(*pos++, tpl);
			read_param_tree(0, tpl.begin(), tpl.end());

		}

		void sml_reader::read_get_proc_parameter_request(cyng::context& ctx, cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 5, "Get Profile List Request");

			//
			//	serverId
			//
			read_server_id(*pos++);

			//
			//	username
			//
			read_string("userName", *pos++);

			//
			//	password
			//
			read_string("password", *pos++);

			//
			//	parameterTreePath
			//
			std::vector<obis> path = read_param_tree_path(*pos++);

			//
			//	attribute
			//
			//	ToDo:

			ctx.attach(cyng::generate_invoke("sml.get.proc.parameter.request"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, ro_.get_value("serverId")
				, ro_.get_value("userName")
				, ro_.get_value("password")
				, to_hex(path.at(0))
				, *pos	//	attribute
			));

		}

		void sml_reader::read_attention_response(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 4, "Attention Response");

			//
			//	serverId
			//
			read_server_id(*pos++);

			//
			//	attentionNo (OBIS)
			//
			obis code = read_obis(*pos++);

			//
			//	attentionMsg
			//
			ro_.set_value("attentionMsg", *pos++);

			//
			//	attentionDetails (SML_Tree)
			//
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(*pos++, tpl);
			read_param_tree(0, tpl.begin(), tpl.end());

		}

		void sml_reader::read_param_tree(std::size_t depth
			, cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 3, "SML Tree");

			//
			//	1. parameterName Octet String,
			//
			obis code = read_obis(*pos++);

			//
			//	2. parameterValue SML_ProcParValue OPTIONAL,
			//
			read_parameter(code, *pos++);

			//
			//	3. child_List List_of_SML_Tree OPTIONAL
			//
			//cyng::xml::write(node.append_child("child_List"), *pos++);
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(*pos++, tpl);
			for (auto const child : tpl)
			{
				cyng::tuple_t tmp;
				tmp = cyng::value_cast(child, tmp);
				read_param_tree(++depth, tmp.begin(), tmp.end());
			}
		}

		void sml_reader::read_period_list(std::vector<obis> const& path
			, cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			//const std::string str_count = std::to_string(count);
			//node.append_attribute("size").set_value(str_count.c_str());

			//
			//	list of tuples (period entry)
			//
			std::size_t counter{ 0 };
			while (pos != end)
			{
				cyng::tuple_t tpl;
				tpl = cyng::value_cast(*pos++, tpl);
				read_period_entry(path, counter, tpl.begin(), tpl.end());
				++counter;

			}
		}

		void sml_reader::read_period_entry(std::vector<obis> const& path
			, std::size_t index
			, cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 5, "Period Entry");

			//
			//	object name
			//
			obis code = read_obis(*pos++);

			//
			//	unit (see sml_unit_enum)
			//
			const auto unit = read_unit("SMLUnit", *pos++);

			//
			//	scaler
			//
			const auto scaler = read_scaler(*pos++);

			//
			//	value
			//
			read_value(code, scaler, unit, *pos++);

			//
			//	valueSignature
			//
			ro_.set_value("signature", *pos++);

		}


		void sml_reader::read_time(std::string const& name, cyng::object obj)
		{
			cyng::tuple_t choice;
			choice = cyng::value_cast(obj, choice);
			BOOST_ASSERT_MSG(choice.size() == 2, "TIME");
			if (choice.size() == 2)
			{
				auto code = cyng::value_cast<std::uint8_t>(choice.front(), 0);
				switch (code)
				{
				case TIME_TIMESTAMP:
				{
					const std::uint32_t sec = cyng::value_cast<std::uint32_t>(choice.back(), 0);
					ro_.set_value(name, cyng::make_time_point(sec));
				}
				break;
				case TIME_SECINDEX:
					ro_.set_value(name, choice.back());
					//cyng::xml::write(node, choice.back());
					break;
				default:
					break;
				}
			}

		}

		std::vector<obis> sml_reader::read_param_tree_path(cyng::object obj)
		{
			std::vector<obis> result;

			cyng::tuple_t tpl;
			tpl = cyng::value_cast(obj, tpl);
			for (auto obj : tpl)
			{

				const obis object_type = read_obis(obj);
				result.push_back(object_type);

			}
			return result;
		}

		obis sml_reader::read_obis(cyng::object obj)
		{
			cyng::buffer_t tmp;
			tmp = cyng::value_cast(obj, tmp);

			const obis code = obis(tmp);
			//const std::string name_dec = to_string(code);
			//const std::string name_hex = to_hex(code);

			//auto child = node.append_child("profile");
			//child.append_attribute("type").set_value(get_name(code));
			//child.append_attribute("obis").set_value(name_hex.c_str());
			//child.append_child(pugi::node_pcdata).set_value(name_dec.c_str());

			return code;
		}

		std::uint8_t sml_reader::read_unit(std::string const& name, cyng::object obj)
		{
			std::uint8_t unit = cyng::value_cast<std::uint8_t>(obj, 0);
			//node.append_attribute("type").set_value(get_unit_name(unit));
			//node.append_child(pugi::node_pcdata).set_value(std::to_string(+unit).c_str());
			ro_.set_value(name, obj);
			return unit;
		}

		std::string sml_reader::read_string(std::string const& name, cyng::object obj)
		{
			cyng::buffer_t buffer;
			buffer = cyng::value_cast(obj, buffer);
			const auto str = cyng::io::to_ascii(buffer);
			ro_.set_value(name, cyng::make_object(str));
			return str;
		}

		std::string sml_reader::read_server_id(cyng::object obj)
		{
			cyng::buffer_t buffer;
			buffer = cyng::value_cast(obj, buffer);
			const auto str = from_server_id(buffer);
			ro_.set_value("serverId", cyng::make_object(str));
			return str;
		}

		std::string sml_reader::read_client_id(cyng::object obj)
		{
			cyng::buffer_t buffer;
			buffer = cyng::value_cast(obj, buffer);
			const auto str = from_server_id(buffer);
			ro_.set_value("clientId", cyng::make_object(str));
			return str;
		}

		std::int8_t sml_reader::read_scaler(cyng::object obj)
		{
			std::int8_t scaler = cyng::value_cast<std::int8_t>(obj, 0);
			//node.append_child(pugi::node_pcdata).set_value(std::to_string(+scaler).c_str());
			return scaler;
		}

		void sml_reader::read_value(obis code, std::int8_t scaler, std::uint8_t unit, cyng::object obj)
		{
			//
			//	write value
			//
			//cyng::xml::write(node, obj);

			if (code == OBIS_DATA_MANUFACTURER)
			{
				cyng::buffer_t buffer;
				buffer = cyng::value_cast(obj, buffer);
				const auto manufacturer = cyng::io::to_ascii(buffer);
				ro_.set_value("manufacturer", cyng::make_object(manufacturer));
			}
			else if (code == OBIS_READOUT_UTC)
			{
				if (obj.get_class().tag() == cyng::TC_TUPLE)
				{
					read_time("roTime", obj);
				}
				else
				{
					const auto tm = cyng::value_cast<std::uint32_t>(obj, 0);
					const auto tp = std::chrono::system_clock::from_time_t(tm);
					//const auto str = cyng::to_str(tp);
					//node.append_attribute("readoutTime").set_value(str.c_str());
					ro_.set_value("roTime", cyng::make_object(tp));
				}
			}
			else if (code == OBIS_ACT_SENSOR_TIME)
			{
				const auto tm = cyng::value_cast<std::uint32_t>(obj, 0);
				const auto tp = std::chrono::system_clock::from_time_t(tm);
				//const auto str = cyng::to_str(tp);
				//node.append_attribute("sensorTime").set_value(str.c_str());
				ro_.set_value("actTime", cyng::make_object(tp));
			}
			else if (code == OBIS_SERIAL_NR)
			{ 
				cyng::buffer_t buffer;
				buffer = cyng::value_cast(obj, buffer);
				std::reverse(buffer.begin(), buffer.end());
				const auto serial_nr = cyng::io::to_ascii(buffer);
				ro_.set_value("serialNr", cyng::make_object(serial_nr));
			}
			else if (code == OBIS_SERIAL_NR_SECOND)
			{
				cyng::buffer_t buffer;
				buffer = cyng::value_cast(obj, buffer);
				std::reverse(buffer.begin(), buffer.end());
				const auto serial_nr = cyng::io::to_ascii(buffer);
				ro_.set_value("serialNr", cyng::make_object(serial_nr));
			}
			else
			{
				if (scaler != 0)
				{
					switch (obj.get_class().tag())
					{
					case cyng::TC_INT64:
					{
						const std::int64_t value = cyng::value_cast<std::int64_t>(obj, 0);
						const auto str = scale_value(value, scaler);
						ro_.set_value("value", cyng::make_object(str));
					}
					break;
					case cyng::TC_INT32:
					{
						const std::int32_t value = cyng::value_cast<std::int32_t>(obj, 0);
						const auto str = scale_value(value, scaler);
						ro_.set_value("value", cyng::make_object(str));
					}
					break;
					default:
						break;
					}
				}
			}
		}

		void sml_reader::read_parameter(obis code, cyng::object obj)
		{
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(obj, tpl);
			if (tpl.size() == 2)
			{
				const auto type = cyng::value_cast<std::uint8_t>(tpl.front(), 0);
				switch (type)
				{
				case PROC_PAR_VALUE:
					//node.append_attribute("parameter").set_value("value");
					//cyng::xml::write(node, tpl.back());
					if ((code == OBIS_CLASS_OP_LSM_ACTIVE_RULESET) || (code == OBIS_CLASS_OP_LSM_PASSIVE_RULESET))
					{
						cyng::buffer_t buffer;
						buffer = cyng::value_cast(tpl.back(), buffer);
						const auto name = cyng::io::to_ascii(buffer);
						//node.append_attribute("name").set_value(name.c_str());
					}
					break;
				case PROC_PAR_PERIODENTRY:
					//node.append_attribute("parameter").set_value("period");
					//cyng::xml::write(node, tpl.back());
					break;
				case PROC_PAR_TUPELENTRY:
					//node.append_attribute("parameter").set_value("tuple");
					//cyng::xml::write(node, tpl.back());
					break;
				case PROC_PAR_TIME:
					//node.append_attribute("parameter").set_value("time");
					read_time("parTime", tpl.back());
					break;
				default:
					//node.append_attribute("parameter").set_value("undefined");
					//cyng::xml::write(node, obj);
					break;
				}
			}
			else
			{
				//cyng::xml::write(node, obj);
			}
		}

		sml_reader::readout::readout(boost::uuids::uuid pk)
			: pk_(pk)
			, idx_(0)
			, trx_()
			, values_()
		{}

		void sml_reader::readout::reset(boost::uuids::uuid pk, std::size_t idx)
		{
			pk_ = pk;
			idx_ = idx;
			trx_.clear();
			values_.clear();
		}

		sml_reader::readout& sml_reader::readout::set_trx(cyng::buffer_t const& buffer)
		{
			trx_ = cyng::io::to_ascii(buffer);
			return *this;
		}

		sml_reader::readout& sml_reader::readout::set_index(std::size_t idx)
		{
			idx_ = idx;
			return *this;
		}

		sml_reader::readout& sml_reader::readout::set_value(std::string const& name, cyng::object value)
		{
			values_[name] = value;
			return *this;
		}

		cyng::object sml_reader::readout::get_value(std::string const& name) const
		{
			auto pos = values_.find(name);
			return (pos != values_.end())
				? pos->second
				: cyng::make_object()
				;
		}


	}	//	sml
}

