/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include <smf/sml/protocol/reader.h>
#include <smf/sml/obis_db.h>
#include <smf/sml/obis_io.h>
#include <smf/sml/srv_id_io.h>
#include <smf/sml/units.h>
#include <smf/sml/scaler.h>

#include <cyng/io/io_buffer.h>
#include <cyng/io/io_chrono.hpp>
#include <cyng/io/serializer.h>
#include <cyng/value_cast.hpp>
#include <cyng/numeric_cast.hpp>
#include <cyng/xml.h>
#include <cyng/factory.h>
#include <cyng/vm/generator.h>
#include <cyng/vm/manip.h>
#include <cyng/io/swap.h>

#include <boost/uuid/nil_generator.hpp>
#include <boost/core/ignore_unused.hpp>

namespace node
{
	namespace sml
	{

		reader::reader()
			: rgn_()
			, ro_(rgn_())
		{
			reset();
		}

		void reader::reset()
		{
			ro_.reset(rgn_(), 0);
		}

		cyng::vector_t reader::read(cyng::tuple_t const& msg, std::size_t idx)
		{
			return read_msg(msg.begin(), msg.end(), idx);
		}

		cyng::vector_t reader::read_msg(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end, std::size_t idx)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 5, "SML message");
			if (count!= 5)	return cyng::vector_t{};

			//
			//	instruction vector
			//
			cyng::vector_t prg;

			//
			//	delayed output
			//
			prg << cyng::generate_invoke_unwinded("log.msg.debug"
				, "message #"
				, idx);

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
			prg << cyng::unwinder(read_choice(choice));
			//BOOST_ASSERT_MSG(choice.size() == 2, "CHOICE");
			//if (choice.size() == 2)
			//{
			//	ro_.set_value("code", choice.front());
			//	prg << cyng::unwinder(read_body(choice.front(), choice.back()));
			//}

			//
			//	(6) CRC16
			//
			ro_.set_value("crc16", *pos);

			return prg;
		}

		void reader::set_trx(cyng::buffer_t const& trx)
		{
			ro_.set_trx(trx);
		}

		cyng::vector_t reader::read_choice(cyng::tuple_t const& choice)
		{
			BOOST_ASSERT_MSG(choice.size() == 2, "CHOICE");
			if (choice.size() == 2)
			{
				ro_.set_value("code", choice.front());
				return read_body(choice.front(), choice.back());
			}
			return cyng::vector_t();
		}

		cyng::vector_t reader::read_body(cyng::object type, cyng::object body)
		{
			auto code = cyng::value_cast<std::uint16_t>(type, 0);

			cyng::tuple_t tpl;
			tpl = cyng::value_cast(body, tpl);

			switch (code)
			{
			case BODY_OPEN_REQUEST:
				return read_public_open_request(tpl.begin(), tpl.end());
			case BODY_OPEN_RESPONSE:
				return read_public_open_response(tpl.begin(), tpl.end());
			case BODY_CLOSE_REQUEST:
				return read_public_close_request(tpl.begin(), tpl.end());
			case BODY_CLOSE_RESPONSE:
				return read_public_close_response(tpl.begin(), tpl.end());
			case BODY_GET_PROFILE_PACK_REQUEST:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_PROFILE_PACK_RESPONSE:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_PROFILE_LIST_REQUEST:
				return read_get_profile_list_request(tpl.begin(), tpl.end());
			case BODY_GET_PROFILE_LIST_RESPONSE:
				return read_get_profile_list_response(tpl.begin(), tpl.end());
			case BODY_GET_PROC_PARAMETER_REQUEST:
				return read_get_proc_parameter_request(tpl.begin(), tpl.end());
			case BODY_GET_PROC_PARAMETER_RESPONSE:
				return read_get_proc_parameter_response(tpl.begin(), tpl.end());
			case BODY_SET_PROC_PARAMETER_REQUEST:
				return read_set_proc_parameter_request(tpl.begin(), tpl.end());
			case BODY_SET_PROC_PARAMETER_RESPONSE:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_GET_LIST_REQUEST:
				return read_get_list_request(tpl.begin(), tpl.end());
			case BODY_GET_LIST_RESPONSE:
				//	ToDo: read get list response (0701)
				//cyng::xml::write(node.append_child("data"), body);
				break;
			case BODY_ATTENTION_RESPONSE:
				return read_attention_response(tpl.begin(), tpl.end());
			default:
				//cyng::xml::write(node.append_child("data"), body);
				break;
			}

			return cyng::generate_invoke("log.msg.fatal"
				, "unknown SML message code"
				, code);
		}

		cyng::vector_t reader::read_public_open_request(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 7, "Public Open Request");
			if (count != 7)	return cyng::vector_t{};

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

			//
			//	instruction vector
			//
			return cyng::generate_invoke("sml.public.open.request"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, ro_.client_id_
				, ro_.server_id_
				, ro_.get_value("reqFileId")
				, ro_.get_value("userName")
				, ro_.get_value("password"));

		}

		cyng::vector_t reader::read_public_open_response(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 6, "Public Open Response");
			if (count != 6)	return cyng::vector_t{};

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

			return cyng::generate_invoke("sml.public.open.response"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, ro_.client_id_
				, ro_.server_id_
				, ro_.get_value("reqFileId"));
		}

		cyng::vector_t reader::read_public_close_request(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 1, "Public Close Request");
			if (count != 1)	return cyng::vector_t{};
			
			ro_.set_value("globalSignature", *pos++);

			return cyng::generate_invoke("sml.public.close.request"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_);
		}

		cyng::vector_t reader::read_public_close_response(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 1, "Public Close Response");
			if (count != 1)	return cyng::vector_t{};
			
			ro_.set_value("globalSignature", *pos++);

			return cyng::generate_invoke("sml.public.close.response"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_);
		}

		cyng::vector_t reader::read_get_profile_list_response(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 9, "Get Profile List Response");
			if (count != 9)	return cyng::vector_t{};
			
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

			return cyng::generate_invoke("db.insert.meta"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, ro_.get_value("roTime")
				, ro_.get_value("actTime")
				, ro_.get_value("valTime")
				, ro_.client_id_
				, ro_.server_id_
				, ro_.get_value("status"));

		}

		cyng::vector_t reader::read_get_profile_list_request(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 9, "Get Profile List Request");
			if (count != 9)	return cyng::vector_t{};

			//
			//	serverId
			//	Typically 7 bytes to identify gateway/MUC
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
			//	rawdata (typically not set)
			//
			ro_.set_value("rawData", *pos++);

			//
			//	start/end time
			//
			read_time("beginTime", *pos++);
			read_time("endTime", *pos++);

			//	parameterTreePath (OBIS)
			//
			std::vector<obis> path = read_param_tree_path(*pos++);
			BOOST_ASSERT(path.size() == 1);

			//
			//	object list
			//
			auto obj_list = *pos++;
			boost::ignore_unused(obj_list);

			//
			//	dasDetails
			//
			auto das_details = *pos++;
			boost::ignore_unused(das_details);

			//
			//	instruction vector
			//
			if (path.size()) {
				return cyng::generate_invoke("sml.get.profile.list.request"
					, ro_.pk_
					, ro_.trx_
					, ro_.idx_
					, ro_.client_id_
					, ro_.server_id_
					, ro_.get_value("userName")
					, ro_.get_value("password")
					, ro_.get_value("beginTime")
					, ro_.get_value("endTime")
					, path.front().to_buffer());
			}

			return cyng::vector_t{};
		}

		cyng::vector_t reader::read_get_proc_parameter_response(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 3, "Get Proc Parameter Response");
			if (count != 3)	return cyng::vector_t{};
			
			//
			//	serverId
			//
			read_server_id(*pos++);

			//
			//	parameterTreePath (OBIS)
			//
			std::vector<obis> path = read_param_tree_path(*pos++);
			BOOST_ASSERT(path.size() == 1);

			//
			//	parameterTree
			//
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(*pos++, tpl);

			//
			//	this is a parameter tree
			//	read_param_tree(0, tpl.begin(), tpl.end());
			//
			return read_get_proc_parameter_response(path, 0, tpl.begin(), tpl.end());
		}

		cyng::vector_t reader::read_get_proc_parameter_response(std::vector<obis> path
			, std::size_t depth
			, cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 3, "SML Tree");
			if (count != 3)	return cyng::vector_t{};
			
			//
			//	1. parameterName Octet String,
			//
			obis code = read_obis(*pos++);
			if (code == path.back()) {
				//	root
				//std::cout << "root: " << code << std::endl;
				BOOST_ASSERT(depth == 0);
			}
			else {
				path.push_back(code);
			}

			//
			//	2. parameterValue SML_ProcParValue OPTIONAL,
			//
			auto attr = read_parameter(*pos++);

			//
			//	program vector
			//
			cyng::vector_t prg;

			switch (path.size()) {
			case 1:
				if (path.front() == OBIS_CLASS_OP_LOG_STATUS_WORD) {
					//	generate status word

					//to_param_map
					return cyng::generate_invoke("sml.get.proc.status.word"
						, ro_.pk_
						, ro_.trx_
						, ro_.idx_
						, from_server_id(ro_.server_id_)
						, OBIS_CLASS_OP_LOG_STATUS_WORD.to_buffer()	//	same as path.front()
						, cyng::value_cast<std::uint32_t>(attr.second, 0u));	//	[u32] value
				}
				else if (path.front() == OBIS_CODE_ROOT_MEMORY_USAGE) {
					//	get memory usage
					read_get_proc_multiple_parameters(*pos++);
					return cyng::generate_invoke("sml.get.proc.param.memory"
						, ro_.pk_
						, ro_.trx_
						, ro_.idx_
						, from_server_id(ro_.server_id_)
						, OBIS_CODE_ROOT_MEMORY_USAGE.to_buffer()	//	same as path.front()
						, ro_.get_value(OBIS_CODE_ROOT_MEMORY_MIRROR)	//	mirror
						, ro_.get_value(OBIS_CODE_ROOT_MEMORY_TMP));	//	tmp
				}
				else if (path.front() == OBIS_CODE_ROOT_W_MBUS_STATUS) {
					//	get memory usage
					read_get_proc_multiple_parameters(*pos++);
					return cyng::generate_invoke("sml.get.proc.param.wmbus.status"
						, ro_.pk_
						, ro_.trx_
						, ro_.idx_
						, from_server_id(ro_.server_id_)
						, OBIS_CODE_ROOT_W_MBUS_STATUS.to_buffer()	//	same as path.front()
						, ro_.get_string(OBIS_W_MBUS_ADAPTER_MANUFACTURER)	//	Manufacturer (RC1180-MBUS3)
						, ro_.get_value(OBIS_W_MBUS_ADAPTER_ID)	//	Adapter ID (A8 15 34 83 40 04 01 31)
						, ro_.get_string(OBIS_W_MBUS_FIRMWARE)	//	firmware version (3.08)
						, ro_.get_string(OBIS_W_MBUS_HARDWARE)	//	hardware version (2.00)
					);
				}
				else if (path.front() == OBIS_CODE_IF_wMBUS) {
					//	get memory usage
					read_get_proc_multiple_parameters(*pos++);
					return cyng::generate_invoke("sml.get.proc.param.wmbus.config"
						, ro_.pk_
						, ro_.trx_
						, ro_.idx_
						, from_server_id(ro_.server_id_)
						, OBIS_CODE_IF_wMBUS.to_buffer()	//	same as path.front()
						, ro_.get_value(OBIS_W_MBUS_PROTOCOL)	//	protocol
						, ro_.get_value(OBIS_W_MBUS_S_MODE)	//	S2 mode in seconds (30)
						, ro_.get_value(OBIS_W_MBUS_T_MODE)	//	T2 mode in seconds (20)
						, ro_.get_value(OBIS_W_MBUS_REBOOT)	//	automatic reboot in seconds (86400)
						, ro_.get_value(OBIS_W_MBUS_POWER)	//	0
						, ro_.get_value(OBIS_W_MBUS_INSTALL_MODE)	//	installation mode (true)
					);
				}
				else if (path.front() == OBIS_CODE_ROOT_IPT_STATE) {
					//	get IP-T status
					read_get_proc_multiple_parameters(*pos++);
					//	81 49 17 07 00 00 ip address
					//	81 49 1A 07 00 00 local port
					//	81 49 19 07 00 00 remote port
					return cyng::generate_invoke("sml.get.proc.param.ipt.state"
						, ro_.pk_
						, ro_.trx_
						, ro_.idx_
						, from_server_id(ro_.server_id_)
						, OBIS_CODE_ROOT_IPT_STATE.to_buffer()	//	same as path.front()
						, ro_.get_value(OBIS_CODE_ROOT_IPT_STATE_ADDRESS)	//	IP adress
						, ro_.get_value(OBIS_CODE_ROOT_IPT_STATE_PORT_LOCAL)	//	local port
						, ro_.get_value(OBIS_CODE_ROOT_IPT_STATE_PORT_REMOTE)	//	remote port
					);
				}
				break;
			case 2:
				if (path.front() == OBIS_CODE_ROOT_DEVICE_IDENT)
				{
					if (path.back() == OBIS_CODE_DEVICE_CLASS) {
						//
						//	device class
						//	CODE_ROOT_DEVICE_IDENT (81, 81, C7, 82, 01, FF)
						//		CODE_DEVICE_CLASS (81, 81, C7, 82, 02, FF)
						//
#ifdef _DEBUG
						//std::cout << "OBIS_CODE_DEVICE_CLASS: " << cyng::io::to_str(attr.second) << std::endl;
#endif
						return cyng::generate_invoke("sml.get.proc.param.simple"
							, ro_.pk_
							, ro_.trx_
							, ro_.idx_
							, ro_.server_id_
							, path.back().to_buffer()
							, attr.second);	//	value

					}
					else if (path.back() == OBIS_DATA_MANUFACTURER) {
						//
						//	device class
						//
#ifdef _DEBUG
						//std::cout << "OBIS_DATA_MANUFACTURER: " << cyng::io::to_str(attr.second) << std::endl;
#endif
						return cyng::generate_invoke("sml.get.proc.param.simple"
							, ro_.pk_
							, ro_.trx_
							, ro_.idx_
							, ro_.server_id_
							, path.back().to_buffer()
							, attr.second);	//	value
					}
					else if (path.back() == OBIS_CODE_SERVER_ID) {
						//
						//	server ID (81 81 C7 82 04 FF)
						//
#ifdef _DEBUG
						//std::cout << "OBIS_CODE_SERVER_ID: " << cyng::io::to_str(attr.second) << std::endl;
#endif
						return cyng::generate_invoke("sml.get.proc.param.simple"
							, ro_.pk_
							, ro_.trx_
							, ro_.idx_
							, ro_.server_id_
							, path.back().to_buffer()
							, attr.second);	//	value
					}
				}
				else if (path.front() == OBIS_CODE_ROOT_IPT_PARAM) {
					const auto r = path.back().is_matching(0x81, 0x49, 0x0D, 0x07, 0x00);
					if (r.second) {
#ifdef _DEBUG
						std::cout << "OBIS_CODE_ROOT_IPT_PARAM: " << +r.first << std::endl;
#endif
						//	get IP-T params
						read_get_proc_multiple_parameters(*pos++);
						//	81 49 17 07 00 NN ip address
						//	81 49 17 07 00 NN hostname as string - optional
						//	81 49 1A 07 00 NN local port
						//	81 49 19 07 00 NN remote port
						return cyng::generate_invoke("sml.get.proc.param.ipt.param"
							, ro_.pk_
							, ro_.trx_
							, ro_.idx_
							, from_server_id(ro_.server_id_)
							, OBIS_CODE_ROOT_IPT_PARAM.to_buffer()	//	same as path.front()
							, r.first
							, ro_.get_value(make_obis(0x81, 0x49, 0x17, 0x07, 0x00, r.first))	//	IP adress / hostname
							, ro_.get_value(make_obis(0x81, 0x49, 0x1A, 0x07, 0x00, r.first))	//	local port
							, ro_.get_value(make_obis(0x81, 0x49, 0x19, 0x07, 0x00, r.first))	//	remote port
							, ro_.get_string(make_obis(0x81, 0x49, 0x63, 0x3C, 0x01, r.first))	//	device name
							, ro_.get_string(make_obis(0x81, 0x49, 0x63, 0x3C, 0x02, r.first))	//	password
						);
					}
				}
				break;
			case 3:
				if ((path.front() == OBIS_CODE_ROOT_VISIBLE_DEVICES) && path.back().is_matching(0x81, 0x81, 0x10, 0x06)) {

					//
					//	collect meter info
					//	* 81 81 C7 82 04 FF: server ID
					//	* 81 81 C7 82 02 FF: --- (device class)
					//	* 01 00 00 09 0B 00: timestamp
					//
					read_get_proc_multiple_parameters(*pos++);

					//cyng::buffer_t meter;
					//meter = cyng::value_cast(ro_.get_value(OBIS_CODE_SERVER_ID), meter);

					return cyng::generate_invoke("sml.get.proc.param.srv.visible"
						, ro_.pk_
						, ro_.trx_
						, ro_.idx_
						, from_server_id(ro_.server_id_)
						, OBIS_CODE_ROOT_VISIBLE_DEVICES.to_buffer()
						, path.back().get_number()	//	4/5 
						, ro_.get_value(OBIS_CODE_SERVER_ID)	//	meter ID
						, ro_.get_string(OBIS_CODE_DEVICE_CLASS)	//	device class
						, ro_.get_value(OBIS_CURRENT_UTC));	//	UTC
				}
				else if ((path.front() == OBIS_CODE_ROOT_ACTIVE_DEVICES) && path.back().is_matching(0x81, 0x81, 0x11, 0x06)) {
					cyng::tuple_t tpl;
					tpl = cyng::value_cast(*pos++, tpl);

					//
					//	collect meter info
					//	* 81 81 C7 82 04 FF: server ID
					//	* 81 81 C7 82 02 FF: --- (device class)
					//	* 01 00 00 09 0B 00: timestamp
					//
					for (auto const child : tpl)
					{
						cyng::tuple_t tmp;
						tmp = cyng::value_cast(child, tmp);
						read_get_proc_single_parameter(tmp.begin(), tmp.end());

					}
					return cyng::generate_invoke("sml.get.proc.param.srv.active"
						, ro_.pk_
						, ro_.trx_
						, ro_.idx_
						, from_server_id(ro_.server_id_)
						, OBIS_CODE_ROOT_ACTIVE_DEVICES.to_buffer()
						, path.back().get_number()	//	4/5 
						, ro_.get_value(OBIS_CODE_SERVER_ID)	//	meter ID
						, ro_.get_string(OBIS_CODE_DEVICE_CLASS)	//	device class
						, ro_.get_value(OBIS_CURRENT_UTC));	//	UTC
				}
				else if ((path.front() == OBIS_CODE_ROOT_DEVICE_IDENT) && path.back().is_matching(0x81, 0x81, 0xc7, 0x82, 0x07).second) {

					//	* 81, 81, c7, 82, 08, ff:	CURRENT_VERSION/KERNEL
					//	* 81, 81, 00, 02, 00, 00:	VERSION
					//	* 81, 81, c7, 82, 0e, ff:	activated/deactivated
					read_get_proc_multiple_parameters(*pos++);

#ifdef _DEBUG
					//std::cout << "81 81 c7 82 08 ff: " << cyng::io::to_str(ro_.get_value("81 81 c7 82 08 ff")) << std::endl;
					//std::cout << "81 81 00 02 00 00: " << cyng::io::to_str(ro_.get_value("81 81 00 02 00 00")) << std::endl;
					//std::cout << "81 81 c7 82 0e ff: " << cyng::io::to_str(ro_.get_value("81 81 c7 82 0e ff")) << std::endl;
#endif

					return cyng::generate_invoke("sml.get.proc.param.firmware"
						, ro_.pk_
						, ro_.trx_
						, ro_.idx_
						, from_server_id(ro_.server_id_)
						, OBIS_CODE_ROOT_DEVICE_IDENT.to_buffer()
						, path.back().get_storage()	//	[5] as u32
						, ro_.get_string(OBIS_CODE_DEVICE_KERNEL)	//	root-device-id name/section
						, ro_.get_string(OBIS_CODE_VERSION)	//	version
						, ro_.get_value(OBIS_CODE_DEVICE_ACTIVATED));	//	active/inactive

				}

				break;
			default:
				break;
			}

			//
			//	3. child_List List_of_SML_Tree OPTIONAL
			//
			return read_tree_list(path, *pos++, ++depth);

		}

		cyng::vector_t reader::read_tree_list(std::vector<obis> path, cyng::object obj, std::size_t depth)
		{
			cyng::vector_t prg;
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(obj, tpl);
			for (auto const child : tpl)
			{
				cyng::tuple_t tmp;
				tmp = cyng::value_cast(child, tmp);

				prg << cyng::unwinder(read_get_proc_parameter_response(path, depth, tmp.begin(), tmp.end()));
			}
			return prg;
		}

		void reader::read_get_proc_multiple_parameters(cyng::object obj)
		{
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(obj, tpl);
			for (auto const child : tpl)
			{
				read_get_proc_single_parameter(child);
			}

		}

		void reader::read_get_proc_single_parameter(cyng::object obj)
		{
			cyng::tuple_t tmp;
			tmp = cyng::value_cast(obj, tmp);
			read_get_proc_single_parameter(tmp.begin(), tmp.end());
		}

		void reader::read_get_proc_single_parameter(cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 3, "SML Tree");
			if (count != 3)	return;
			
			//
			//	1. parameterName Octet String,
			//
			obis code = read_obis(*pos++);

			//
			//	2. parameterValue SML_ProcParValue OPTIONAL,
			//
			auto attr = read_parameter(*pos++);
			ro_.set_value(code, attr.second);

			//std::cerr
			//	<< to_hex(code)
			//	<< " = "
			//	//<< cyng::io::to_str(attr.second)
			//	<< std::endl;

		}

		cyng::vector_t reader::read_get_proc_parameter_request(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 5, "Get Profile List Request");
			if (count != 5)	return cyng::vector_t{};
			
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
			//	parameterTreePath == parameter address
			//
			std::vector<obis> path = read_param_tree_path(*pos++);
			BOOST_ASSERT(!path.empty());

			//
			//	attribute/constraints
			//
			//	*pos

			if (path.size() == 1)
			{
				return cyng::generate_invoke("sml.get.proc.parameter.request"
					, ro_.pk_
					, ro_.trx_
					, ro_.idx_
					, ro_.server_id_
					, ro_.get_value("userName")
					, ro_.get_value("password")
					, path.at(0).to_buffer()
					, *pos);

			}

			return cyng::generate_invoke("sml.get.proc.parameter.request"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, ro_.server_id_
				, ro_.get_value("userName")
				, ro_.get_value("password")
				, to_hex(path.at(0))	//	ToDo: send complete path
				, *pos);	//	attribute
		}

		cyng::vector_t reader::read_set_proc_parameter_request(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 5, "Set Proc Parameter Request");
            if (count != 5) return cyng::vector_t{};
            
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
            //	parameterTreePath == parameter address
            //
            std::vector<obis> path = read_param_tree_path(*pos++);

            //
            //	parameterTree
            //
            cyng::tuple_t tpl;
            tpl = cyng::value_cast(*pos++, tpl);

            //
            //	recursiv call to an parameter tree - similiar to read_param_tree()
            //
            return read_set_proc_parameter_request_tree(path, 0, tpl.begin(), tpl.end());
		}

		cyng::vector_t reader::read_set_proc_parameter_request_tree(std::vector<obis> path
			, std::size_t depth
			, cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			cyng::vector_t prg;

			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 3, "SML Tree");
            if (count != 3) return cyng::vector_t{};

			//
			//	1. parameterName Octet String,
			//
			obis code = read_obis(*pos++);
			if (code == path.back()) {
				//	root
				std::cout << "root: " << code << std::endl;
				BOOST_ASSERT(depth == 0);
			}
			else {
				path.push_back(code);
			}

			//
			//	2. parameterValue SML_ProcParValue OPTIONAL,
			//
			auto attr = read_parameter(*pos++);
			if (attr.first != PROC_PAR_UNDEF) {
				//	example: push delay
				//	81 81 c7 8a 01 ff => 81 81 c7 8a 01 [01] => 81 81 c7 8a 03 ff
				//	std::cout << to_hex(path) << std::endl;
				if (OBIS_PUSH_OPERATIONS == path.front()) {

					BOOST_ASSERT_MSG(path.size() == 3, "OBIS_PUSH_OPERATIONS param tree too short");

					if (path.size() > 2) {
						auto r = path.at(1).is_matching(0x81, 0x81, 0xc7, 0x8a, 0x01);
						if (r.second) {
							if (path.at(2) == OBIS_CODE(81, 81, c7, 8a, 02, ff)) {


								prg << cyng::generate_invoke_unwinded("sml.set.proc.push.interval"
									, ro_.pk_
									, ro_.trx_
									, r.first
									, ro_.server_id_
									, ro_.get_value("userName")
									, ro_.get_value("password")
									, std::chrono::seconds(cyng::numeric_cast<std::int64_t>(attr.second, 900)));

							}
							else if (path.at(2) == OBIS_CODE(81, 81, c7, 8a, 03, ff)) {

								prg << cyng::generate_invoke_unwinded("sml.set.proc.push.delay"
									, ro_.pk_
									, ro_.trx_
									, r.first
									, ro_.server_id_
									, ro_.get_value("userName")
									, ro_.get_value("password")
									, std::chrono::seconds(cyng::numeric_cast<std::int64_t>(attr.second, 0)));

							}
							else if (path.at(2) == OBIS_CODE(81, 47, 17, 07, 00, FF)) {

								cyng::buffer_t tmp;
								tmp = cyng::value_cast(attr.second, tmp);

								prg << cyng::generate_invoke_unwinded("sml.set.proc.push.name"
									, ro_.pk_
									, ro_.trx_
									, r.first
									, ro_.server_id_
									, ro_.get_value("userName")
									, ro_.get_value("password")
									, std::string(tmp.begin(), tmp.end()));

							}
						}
					}
					else {
						//	param tree too short
					}
				}
				else if (OBIS_CODE_ROOT_IPT_PARAM == path.front()) {
					BOOST_ASSERT_MSG(path.size() == 3, "OBIS_CODE_ROOT_IPT_PARAM param tree too short");
					if (path.size() > 2) {
						auto r = path.at(1).is_matching(0x81, 0x49, 0x0D, 0x07, 0x00);
						if (r.second) {
							BOOST_ASSERT(r.first != 0);

							//
							//	set IP-T host address
							//
							auto m = path.at(2).is_matching(0x81, 0x49, 0x17, 0x07, 0x00);
							if (m.second) {

								BOOST_ASSERT(r.first == m.first);
								BOOST_ASSERT(m.first != 0);
								--m.first;

								//	remove host byte ordering
								auto num = cyng::swap_num(cyng::numeric_cast<std::uint32_t>(attr.second, 0u));
								boost::asio::ip::address address = boost::asio::ip::make_address_v4(num);
								prg << cyng::generate_invoke_unwinded("sml.set.proc.ipt.param.address"
									, ro_.pk_
									, ro_.trx_
									, m.first
									, address);

							}

							//
							//	set IP-T target port
							//
							m = path.at(2).is_matching(0x81, 0x49, 0x1A, 0x07, 0x00);
							if (m.second) {

								BOOST_ASSERT(r.first == m.first);
								BOOST_ASSERT(m.first != 0);
								--m.first;

								//
								//	set IP-T target port
								//
								auto port = cyng::numeric_cast<std::uint16_t>(attr.second, 0u);
								prg << cyng::generate_invoke_unwinded("sml.set.proc.ipt.param.port.target"
									, ro_.pk_
									, ro_.trx_
									, m.first
									, port);

							}

							//
							//	set IP-T user name
							//
							m = path.at(2).is_matching(0x81, 0x49, 0x63, 0x3C, 0x01);
							if (m.second) {

								BOOST_ASSERT(r.first == m.first);
								BOOST_ASSERT(m.first != 0);
								--m.first;

								//
								//	set IP-T user name
								//
								cyng::buffer_t tmp;
								tmp = cyng::value_cast(attr.second, tmp);

								prg << cyng::generate_invoke_unwinded("sml.set.proc.ipt.param.user"
									, ro_.pk_
									, ro_.trx_
									, m.first
									, std::string(tmp.begin(), tmp.end()));

							}

							//
							//	set IP-T password
							//
							m = path.at(2).is_matching(0x81, 0x49, 0x63, 0x3C, 0x02);
							if (m.second) {

								BOOST_ASSERT(r.first == m.first);
								BOOST_ASSERT(m.first != 0);
								--m.first;

								//
								//	set IP-T password
								//
								cyng::buffer_t tmp;
								tmp = cyng::value_cast(attr.second, tmp);

								prg << cyng::generate_invoke_unwinded("sml.set.proc.ipt.param.pwd"
									, ro_.pk_
									, ro_.trx_
									, m.first
									, std::string(tmp.begin(), tmp.end()));

							}
						}
					}
				}
			}

			//
			//	3. child_List List_of_SML_Tree OPTIONAL
			//
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(*pos++, tpl);
			++depth;
			for (auto const child : tpl)
			{
				cyng::tuple_t tmp;
				tmp = cyng::value_cast(child, tmp);
				//read_param_tree(depth, tmp.begin(), tmp.end());
				//
				//	recursive call
				//
				prg << cyng::unwinder(read_set_proc_parameter_request_tree(path, depth, tmp.begin(), tmp.end()));
			}
			return prg;
		}

		cyng::vector_t reader::read_get_list_request(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 5, "Get List Request");
			if (count != 5) return cyng::vector_t{};

			//
			//	clientId - MAC address from caller
			//
			read_client_id(*pos++);

			//
			//	serverId - meter ID
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
			//	list name
			//
			obis code = read_obis(*pos++);

			return cyng::generate_invoke("sml.get.list.request"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, ro_.client_id_
				, ro_.server_id_
				, ro_.get_value("reqFileId")
				, ro_.get_value("userName")
				, ro_.get_value("password")
				, code.to_buffer());
		}

		cyng::vector_t reader::read_attention_response(cyng::tuple_t::const_iterator pos, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 4, "Attention Response");
            if (count != 4) return cyng::vector_t{};

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
			if (tpl.size() == 3) {
				read_param_tree(0, tpl.begin(), tpl.end());
			}

			return cyng::generate_invoke("sml.attention.msg"
				, ro_.pk_
				, ro_.trx_
				, ro_.idx_
				, from_server_id(ro_.server_id_)
				, code.to_buffer()
				, ro_.get_value("attentionMsg"));
		}

		void reader::read_param_tree(std::size_t depth
			, cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 3, "SML Tree");
            if (count != 3) return;

			//
			//	1. parameterName Octet String,
			//
			obis code = read_obis(*pos++);
			boost::ignore_unused(code);

			//
			//	2. parameterValue SML_ProcParValue OPTIONAL,
			//
			auto attr = read_parameter(*pos++);

			//
			//	3. child_List List_of_SML_Tree OPTIONAL
			//
			//cyng::xml::write(node.append_child("child_List"), *pos++);
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(*pos++, tpl);
			++depth;
			for (auto const child : tpl)
			{
				cyng::tuple_t tmp;
				tmp = cyng::value_cast(child, tmp);
				read_param_tree(depth, tmp.begin(), tmp.end());
			}
		}

		void reader::read_period_list(std::vector<obis> const& path
			, cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			boost::ignore_unused(count);	//	release version
			
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

		void reader::read_period_entry(std::vector<obis> const& path
			, std::size_t index
			, cyng::tuple_t::const_iterator pos
			, cyng::tuple_t::const_iterator end)
		{
			std::size_t count = std::distance(pos, end);
			BOOST_ASSERT_MSG(count == 5, "Period Entry");
            if (count != 5) return;

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


		cyng::object reader::read_time(std::string const& name, cyng::object obj)
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
					return cyng::make_time_point(sec);
				}
				break;
				case TIME_SECINDEX:
					ro_.set_value(name, choice.back());
					return choice.back();
				default:
					break;
				}
			}
			return cyng::make_object();
		}

		std::vector<obis> reader::read_param_tree_path(cyng::object obj)
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

		obis reader::read_obis(cyng::object obj)
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

		std::uint8_t reader::read_unit(std::string const& name, cyng::object obj)
		{
			std::uint8_t unit = cyng::value_cast<std::uint8_t>(obj, 0);
			//node.append_attribute("type").set_value(get_unit_name(unit));
			//node.append_child(pugi::node_pcdata).set_value(std::to_string(+unit).c_str());
			ro_.set_value(name, obj);
			return unit;
		}

		std::string reader::read_string(std::string const& name, cyng::object obj)
		{
			cyng::buffer_t buffer;
			buffer = cyng::value_cast(obj, buffer);
			const auto str = cyng::io::to_ascii(buffer);
			ro_.set_value(name, cyng::make_object(str));
			return str;
		}

		std::string reader::read_server_id(cyng::object obj)
		{
			ro_.server_id_ = cyng::value_cast(obj, ro_.server_id_);
			return from_server_id(ro_.server_id_);
		}

		std::string reader::read_client_id(cyng::object obj)
		{
			ro_.client_id_ = cyng::value_cast(obj, ro_.client_id_);
			return from_server_id(ro_.client_id_);
		}

		std::int8_t reader::read_scaler(cyng::object obj)
		{
			std::int8_t scaler = cyng::value_cast<std::int8_t>(obj, 0);
			//node.append_child(pugi::node_pcdata).set_value(std::to_string(+scaler).c_str());
			return scaler;
		}

		void reader::read_value(obis code, std::int8_t scaler, std::uint8_t unit, cyng::object obj)
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
			else if (code == OBIS_CURRENT_UTC)
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

		cyng::attr_t reader::read_parameter(cyng::object obj)
		{
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(obj, tpl);
			if (tpl.size() == 2)
			{
				const auto type = cyng::value_cast<std::uint8_t>(tpl.front(), 0);
				switch (type)
				{
				case PROC_PAR_VALUE:		return cyng::attr_t(type, tpl.back());
				case PROC_PAR_PERIODENTRY:	return cyng::attr_t(type, tpl.back());
				case PROC_PAR_TUPELENTRY:	return cyng::attr_t(type, tpl.back());
				case PROC_PAR_TIME:			return cyng::attr_t(type, read_time("parTime", tpl.back()));
				default:
					break;
				}
			}
			return cyng::attr_t(PROC_PAR_UNDEF, cyng::make_object());
		}
	}	//	sml
}

