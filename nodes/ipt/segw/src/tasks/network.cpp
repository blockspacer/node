﻿/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include "network.h"
#include "../cache.h"
#include "../storage.h"
#include "../segw.h"
#include "../cfg_ipt.h"

#include <smf/serial/baudrate.h>
#include <smf/ipt/response.hpp>
#include <smf/ipt/generator.h>
#include <smf/sml/protocol/serializer.h>
#include <smf/sml/obis_db.h>
#include <smf/sml/event.h>

#include <cyng/factory/set_factory.h>
//#include <cyng/async/task/task_builder.hpp>
#include <cyng/io/serializer.h>
#include <cyng/vm/generator.h>
#include <cyng/io/serializer.h>
#include <cyng/tuple_cast.hpp>
#include <cyng/numeric_cast.hpp>

#ifdef SMF_IO_DEBUG
#include <cyng/io/hex_dump.hpp>
#endif

namespace node
{
	namespace ipt
	{
		network::network(cyng::async::base_task* btp
			, cyng::logging::log_ptr logger
			, cache& cfg
			, storage& db
			, std::string account
			, std::string pwd
			, bool accept_all
			, boost::uuids::uuid tag)
		: bus(logger
				, btp->mux_
				, tag	
				, "ipt:gateway"
				, 1u)
			, base_(*btp)
			, logger_(logger)
			, cache_(cfg)
			, storage_(db)
			, parser_([this](cyng::vector_t&& prg) {
				CYNG_LOG_INFO(logger_, prg.size() << " instructions received");
#ifdef _DEBUG
				CYNG_LOG_TRACE(logger_, cyng::io::to_str(prg));
#endif
				vm_.async_run(std::move(prg));
			}, false, false, false)
			, router_(logger_
				, false	//	client mode
				, vm_
				, cfg
				, db
				, account
				, pwd
				, accept_all)
			, seq_open_channel_map_()
			, task_gpio_(cyng::async::NO_TASK)
		{
			CYNG_LOG_INFO(logger_, "initialize task #"
				<< base_.get_id()
				<< " <"
				<< base_.get_class_name()
				<< ">");

			//
			//	request handler
			//
			vm_.register_function("bus.reconfigure", 1, std::bind(&network::reconfigure, this, std::placeholders::_1));
			vm_.register_function("bus.store.rel.channel.open", 3, std::bind(&network::insert_seq_open_channel_rel, this, std::placeholders::_1));

			vm_.register_function("update.status.ip", 2, [&](cyng::context& ctx) {

				//	 [192.168.1.200:59536,192.168.1.100:26862]
				const cyng::vector_t frame = ctx.get_frame();
				CYNG_LOG_INFO(logger_, ctx.get_name()
					<< " - "
					<< cyng::io::to_str(frame));

				auto const tpl = cyng::tuple_cast<
					boost::asio::ip::tcp::endpoint,		//	[0] remote
					boost::asio::ip::tcp::endpoint		//	[1] local
				>(frame);

				//	with IP-T prefix
				this->cache_.set_cfg(build_cfg_key({ sml::OBIS_ROOT_IPT_PARAM }, "ep.remote"), std::get<0>(tpl));
				this->cache_.set_cfg(build_cfg_key({ sml::OBIS_ROOT_IPT_PARAM }, "ep.local"), std::get<1>(tpl));

			});

			//
			//	statistics
			//
			vm_.async_run(cyng::generate_invoke("log.msg.info", cyng::invoke("lib.size"), "callbacks registered"));

			//
			//	ToDo: set task_gpio_
			//

			//
			//	wireless-LMN configuration
			//	update status word
			//
			//auto pos = tid_map.find(47);
			//exec_.start_wireless_lmn(start_wireless_lmn(config_db, cfg_wireless_lmn, (pos != tid_map.end()) ? pos->second : cyng::async::NO_TASK));

			//
			// wired-LMN configuration
			//
			//pos = tid_map.find(50);
			//start_wired_lmn(config_db, cfg_wired_lmn, (pos != tid_map.end()) ? pos->second : cyng::async::NO_TASK);

		}

		cyng::continuation network::run()
		{

			//
			//	get IP-T configuration
			//
			cfg_ipt ipt(cache_);

			if (is_online())
			{
				//
				//	re/start monitor
				//
				base_.suspend(ipt.get_ipt_tcp_wait_to_reconnect());
			}
			else
			{
				//
				//	reset parser and serializer
				//
				auto const sk = ipt.get_ipt_sk();
				vm_.async_run({ cyng::generate_invoke("ipt.reset.parser", sk)
					, cyng::generate_invoke("ipt.reset.serializer", sk) });

				//
				//	login request
				//
				req_login(ipt.get_ipt_master());
			}

			return cyng::continuation::TASK_CONTINUE;
		}

		void network::stop(bool shutdown)
		{
			//
			//	call base class
			//
			bus::stop();
			while (!vm_.is_halted()) {
				CYNG_LOG_INFO(logger_, "task #"
					<< base_.get_id()
					<< " <"
					<< base_.get_class_name()
					<< "> continue gracefull shutdown");
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			CYNG_LOG_INFO(logger_, "task #"
				<< base_.get_id()
				<< " <"
				<< base_.get_class_name()
				<< "> is stopped");
		}

		//	slot [0] 0x4001/0x4002: response login
		void network::on_login_response(std::uint16_t watchdog, std::string redirect)
		{

			//
			//	op log entry
			//
			auto const sw = cache_.get_status_word();
			auto srv = cache_.get_srv_id();

			//LOG_CODE_61 = 0x4970000A,	//	IP-T - Zugang erfolgt

			storage_.generate_op_log(sw
				, sml::LOG_CODE_61	//	0x4970000A - IP-T - Zugang erfolgt
				, sml::OBIS_CODE_PEER_ADDRESS_WANGSM	//	source is WANGSM (or LOG_SOURCE_ETH == 81, 04, 00, 00, 00, FF)
				, srv	//	server ID
				, ""	//	target
				, 0		//	push nr
				, "IP-T login");	//	description


			//
			//	signal LED
			//
			if (cyng::async::NO_TASK != task_gpio_) {
				base_.mux_.post(task_gpio_, 0, cyng::tuple_factory(true));
			}

			//
			//	update IP address
			//
			vm_.async_run(cyng::generate_invoke("update.status.ip", cyng::invoke("ip.tcp.socket.ep.local"), cyng::invoke("ip.tcp.socket.ep.remote")));
		}

		//	slot [1] - connection lost / reconnect
		void network::on_logout()
		{
			//
			//	op log entry
			//
			auto const sw = cache_.get_status_word();
			auto srv = cache_.get_srv_id();

			//LOG_CODE_65 = 0x4970000E,	//	IP-T - Zugang verloren, Verbindung unerwartet abgebrochen
			storage_.generate_op_log(sw
				, sml::LOG_CODE_65	//	0x4970000A - IP-T - Zugang erfolgt
				, sml::OBIS_CODE_PEER_ADDRESS_WANGSM	//	source is WANGSM (or LOG_SOURCE_ETH == 81, 04, 00, 00, 00, FF)
				, srv	//	server ID
				, ""	//	target
				, 0		//	nr
				, "IP-T login");	//	description

			//
			//	signal LED
			//
			if (cyng::async::NO_TASK != task_gpio_) {
				base_.mux_.post(task_gpio_, 0, cyng::tuple_factory(false));
			}

			//
			//	switch to other configuration
			//
			reconfigure_impl();
		}

		//	slot [2] - 0x4005: push target registered response
		void network::on_res_register_target(sequence_type seq
			, bool success
			, std::uint32_t channel
			, std::string target)
		{	//	no implementation
			BOOST_ASSERT_MSG(false, "[register target response] not implemented");
		}

		//	@brief slot [3] - 0x4006: push target deregistered response
		void network::on_res_deregister_target(sequence_type, bool, std::string const&)
		{	//	no implementation
			CYNG_LOG_WARNING(logger_, "push target deregistered response not implemented");
		}

		//	slot [4] - 0x1000: push channel open response
		void network::on_res_open_push_channel(sequence_type seq
			, bool success
			, std::uint32_t channel
			, std::uint32_t source
			, std::uint16_t status
			, std::size_t count)
		{
			auto pos = seq_open_channel_map_.find(seq);
			if (pos != seq_open_channel_map_.end()) {


				//auto r = make_tp_res_open_push_channel(seq, res);
				//CYNG_LOG_INFO(logger_, "open push channel response "
				//	<< channel
				//	<< ':'
				//	<< source
				//	<< ':'
				//	<< r.get_response_name());

				base_.mux_.post(pos->second.first, 0, cyng::tuple_factory(success, channel, source, status, count, pos->second.second));
			}
			else {
				CYNG_LOG_ERROR(logger_, "open push channel response "
					<< channel
					<< ':'
					<< source
					<< '@'
					<< seq
					<< " not found");
			}
		}

		//	slot [5] - 0x1001: push channel close response
		void network::on_res_close_push_channel(sequence_type seq
			, bool success
			, std::uint32_t channel)
		{
			CYNG_LOG_INFO(logger_, "push channel "
				<< channel
				<< " close response received");
		}

		//	slot [6] 0x9003: connection open request 
		bool network::on_req_open_connection(sequence_type seq, std::string const& number)
		{
			CYNG_LOG_TRACE(logger_, "incoming call " << +seq << ':' << number);

			BOOST_ASSERT(is_online());

			//
			//	accept incoming calls
			//
			return true;
		}

		//	slot [7] - 0x1003: connection open response
		cyng::buffer_t network::on_res_open_connection(sequence_type seq, bool success)
		{	//	no implementation
			CYNG_LOG_WARNING(logger_, "connection open response not implemented");
			return cyng::buffer_t();	//	no data
		}

		//	slot [8] - 0x9004/0x1004: connection close request/response
		void network::on_req_close_connection(sequence_type seq)
		{	
			CYNG_LOG_INFO(logger_, "connection closed");
		}
		void network::on_res_close_connection(sequence_type)
		{	//	no implementation
			CYNG_LOG_WARNING(logger_, "connection close response not implemented");
		}

		//	slot [9] - 0x9002: push data transfer request
		void network::on_req_transfer_push_data(sequence_type seq
			, std::uint32_t channel
			, std::uint32_t source
			, cyng::buffer_t const& data)
		{
			//CYNG_LOG_INFO(logger_, "task #"
			//	<< base_.get_id()
			//	<< " <"
			//	<< base_.get_class_name()
			//	<< "> "
			//	<< config_.get().account_
			//	<< " received "
			//	<< data.size()
			//	<< " bytes push data from "
			//	<< channel
			//	<< '.'
			//	<< source);
		}

		//	slot [10] - transmit data (if connected)
		cyng::buffer_t network::on_transmit_data(cyng::buffer_t const& data)
		{
			CYNG_LOG_TRACE(logger_, "incoming IPT/SML data " << data.size() << " bytes");
			if (data.size() == 6
				&& data.at(0) == 'h'
				&& data.at(1) == 'e'
				&& data.at(2) == 'l'
				&& data.at(3) == 'l'
				&& data.at(4) == 'o'
				&& data.at(5) == '!')
			{
				CYNG_LOG_DEBUG(logger_, "answer with hey!");
				return cyng::make_buffer({ 'h', 'e', 'y', '!' });
			}

			//
			//	parse incoming data
			//
			parser_.read(data.begin(), data.end());
			return cyng::buffer_t();
		}

		void network::reconfigure(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			CYNG_LOG_WARNING(logger_, "bus.reconfigure " << cyng::io::to_str(frame));
			reconfigure_impl();
		}

		void network::reconfigure_impl()
		{
			//
			//	get IP-T configuration
			//
			cfg_ipt ipt(cache_);

			//
			//	switch to other master
			//
			auto const rec = ipt.switch_ipt_redundancy();

			CYNG_LOG_INFO(logger_, "switch to redundancy ["
				<< rec.host_
				<< ':'
				<< rec.service_
				<< "]"	);

			//
			//	trigger reconnect 
			//
			CYNG_LOG_INFO(logger_, "reconnect to network in "
				<< cyng::to_str(rec.monitor_));

			base_.suspend(rec.monitor_);
		}

		void network::insert_seq_open_channel_rel(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();

			auto const tpl = cyng::tuple_cast<
				sequence_type,		//	[0] ipt seq
				std::size_t,		//	[1] task id
				std::string			//	[2] target name
			>(frame);

			CYNG_LOG_TRACE(logger_, "ipt sequence "
				<< +std::get<0>(tpl)
				<< " ==> "
				<< std::get<1>(tpl)
				<< ':'
				<< std::get<2>(tpl));

			seq_open_channel_map_.emplace(std::piecewise_construct
				, std::forward_as_tuple(std::get<0>(tpl))
				, std::forward_as_tuple(std::get<1>(tpl), std::get<2>(tpl)))
				;
		}

	}
}