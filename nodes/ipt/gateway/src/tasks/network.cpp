﻿/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include "network.h"
#include <smf/ipt/response.hpp>
#include <smf/ipt/generator.h>
#include <smf/sml/protocol/serializer.h>
#include <cyng/factory/set_factory.h>
#include <cyng/async/task/task_builder.hpp>
#include <cyng/io/serializer.h>
#include <cyng/vm/generator.h>
#include <cyng/io/serializer.h>
#include <cyng/tuple_cast.hpp>

#include <boost/uuid/random_generator.hpp>
#ifdef SMF_IO_DEBUG
#include <cyng/io/hex_dump.hpp>
#endif

namespace node
{
	namespace ipt
	{
		network::network(cyng::async::base_task* btp
			, cyng::logging::log_ptr logger
			, master_config_t const& cfg
			, std::string account
			, std::string pwd
			, std::string manufacturer
			, std::string model
			, cyng::mac48 mac)
		: base_(*btp)
			, bus_(bus_factory(btp->mux_, logger, boost::uuids::random_generator()(), scramble_key::default_scramble_key_, btp->get_id(), "ipt:gateway"))
			, logger_(logger)
			, config_(cfg)
			, master_(0)
			, parser_([this](cyng::vector_t&& prg) {
				CYNG_LOG_INFO(logger_, prg.size() << " instructions received");
#ifdef _DEBUG
				CYNG_LOG_TRACE(logger_, cyng::io::to_str(prg));
#endif
				bus_->vm_.async_run(std::move(prg));
			}, false)
			, core_(logger_, bus_->vm_, false, account, pwd, manufacturer, model, mac)
			, seq_target_map_()
			, channel_target_map_()
		{
			CYNG_LOG_INFO(logger_, "task #"
				<< base_.get_id()
				<< " <"
				<< base_.get_class_name()
				<< "> is running");

			//
			//	request handler
			//
			bus_->vm_.register_function("network.task.resume", 4, std::bind(&network::task_resume, this, std::placeholders::_1));
			bus_->vm_.register_function("bus.reconfigure", 1, std::bind(&network::reconfigure, this, std::placeholders::_1));
			bus_->vm_.register_function("net.insert.rel", 1, std::bind(&network::insert_rel, this, std::placeholders::_1));

			bus_->vm_.async_run(cyng::generate_invoke("log.msg.info", cyng::invoke("lib.size"), "callbacks registered"));

		}

		void network::run()
		{
			if (bus_->is_online())
			{
				//
				//	deregister target
				//
				//bus_->vm_.async_run(cyng::generate_invoke("req.deregister.push.target", "DEMO"));

				//
				//	send watchdog response - without request
				//
				bus_->vm_.async_run(cyng::generate_invoke("res.watchdog", static_cast<std::uint8_t>(0)));
				bus_->vm_.async_run(cyng::generate_invoke("stream.flush"));
			}
			else
			{
				//
				//	reset parser and serializer
				//
				bus_->vm_.async_run(cyng::generate_invoke("ipt.reset.parser", config_[master_].sk_));
				bus_->vm_.async_run(cyng::generate_invoke("ipt.reset.serializer", config_[master_].sk_));

				//
				//	login request
				//
				if (config_[master_].scrambled_)
				{
					bus_->vm_.async_run(gen::ipt_req_login_scrambled(config_, master_));
				}
				else
				{
					bus_->vm_.async_run(gen::ipt_req_login_public(config_, master_));
				}
			}
		}

		void network::stop()
		{
			bus_->stop();
			CYNG_LOG_INFO(logger_, "task #"
				<< base_.get_id()
				<< " <"
				<< base_.get_class_name()
				<< "> is stopped");
		}

		//	slot 0
		cyng::continuation network::process(std::uint16_t watchdog, std::string redirect)
		{
			if (watchdog != 0)
			{
				CYNG_LOG_INFO(logger_, "start watchdog: " << watchdog << " minutes");
				//base_.suspend(std::chrono::minutes(watchdog));
				base_.suspend(std::chrono::seconds(watchdog));
			}

			//
			//	register targets
			//
			//register_targets();

			return cyng::continuation::TASK_CONTINUE;
		}

		cyng::continuation network::process()
		{
			//
			//	switch to other configuration
			//
			reconfigure_impl();

			//
			//	continue task
			//
			return cyng::continuation::TASK_CONTINUE;
		}

		//	slot [2]
		cyng::continuation network::process(sequence_type seq, std::string const& number)
		{
			CYNG_LOG_TRACE(logger_, "incoming call " << +seq << ':' << number);

			BOOST_ASSERT(bus_->is_online());

			//
			//	accept incoming calls
			//
			bus_->vm_.async_run(cyng::generate_invoke("res.open.connection", seq, static_cast<response_type>(ipt::tp_res_open_connection_policy::DIALUP_SUCCESS)));
			bus_->vm_.async_run(cyng::generate_invoke("stream.flush"));

			//
			//	continue task
			//
			return cyng::continuation::TASK_CONTINUE;
		}

		cyng::continuation network::process(sequence_type seq, std::uint32_t channel, std::uint32_t source, cyng::buffer_t const& data)
		{
			//
			//	distribute to output tasks
			//


			//
			//	continue task
			//
			return cyng::continuation::TASK_CONTINUE;
		}

		//	slot [4] - register target response
		cyng::continuation network::process(sequence_type seq, bool success, std::uint32_t channel)
		{
			if (success)
			{
				auto pos = seq_target_map_.find(seq);
				if (pos != seq_target_map_.end())
				{
					CYNG_LOG_INFO(logger_, "channel "
						<< channel
						<< " ==> "
						<< pos->second);

					channel_target_map_.emplace(channel, pos->second);
					seq_target_map_.erase(pos);

					//cyng::vector_t prg;
					//prg
					//	<< cyng::generate_invoke_unwinded("req.deregister.push.target", "DEMO")
					//	//<< cyng::generate_invoke_unwinded("net.remove.rel", cyng::invoke("ipt.push.seq"), "DEMO")
					//	<< cyng::generate_invoke_unwinded("stream.flush")
					//	;

					//bus_->vm_.async_run(std::move(prg));

				}
			}

			//
			//	continue task
			//
			return cyng::continuation::TASK_CONTINUE;
		}

		//	slot [5]
		cyng::continuation network::process(cyng::buffer_t const& data)
		{
			CYNG_LOG_TRACE(logger_, "incoming SML data " << data.size() << " bytes");

			//
			//	parse incoming data
			//
			parser_.read(data.begin(), data.end());

			//
			//	continue task
			//
			return cyng::continuation::TASK_CONTINUE;
		}

		cyng::continuation network::process(sequence_type seq, bool success, std::string const& target)
		{
			CYNG_LOG_INFO(logger_, "target "
				<< target
				<< " deregistered");

			//
			//	continue task
			//
			return cyng::continuation::TASK_CONTINUE;
		}

		void network::task_resume(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			//	[1,0,TDevice,3]
			CYNG_LOG_TRACE(logger_, "resume task - " << cyng::io::to_str(frame));
			std::size_t tsk = cyng::value_cast<std::size_t>(frame.at(0), 0);
			std::size_t slot = cyng::value_cast<std::size_t>(frame.at(1), 0);
			base_.mux_.post(tsk, slot, cyng::tuple_t{ frame.at(2), frame.at(3) });
		}

		void network::reconfigure(cyng::context& ctx)
		{
			reconfigure_impl();
		}

		void network::reconfigure_impl()
		{
			//
			//	switch to other master
			//
			if (config_.size() > 1)
			{
				master_++;
				if (master_ == config_.size())
				{
					master_ = 0;
				}
				CYNG_LOG_INFO(logger_, "switch to redundancy "
					<< config_[master_].host_
					<< ':'
					<< config_[master_].service_);

			}
			else
			{
				CYNG_LOG_ERROR(logger_, "network login failed - no other redundancy available");
			}

			//
			//	trigger reconnect 
			//
			CYNG_LOG_INFO(logger_, "reconnect to network in "
				<< config_[master_].monitor_.count()
				<< " seconds");
			base_.suspend(config_[master_].monitor_);

		}

		void network::insert_rel(cyng::context& ctx)
		{
			//	[5,power@solostec]
			//
			//	* ipt sequence
			//	* target name
			const cyng::vector_t frame = ctx.get_frame();

			auto const tpl = cyng::tuple_cast<
				sequence_type,		//	[0] ipt seq
				std::string			//	[1] target
			>(frame);

			CYNG_LOG_TRACE(logger_, "ipt sequence "
				<< +std::get<0>(tpl)
				<< " ==> "
				<< std::get<1>(tpl));

			seq_target_map_.emplace(std::get<0>(tpl), std::get<1>(tpl));
		}

		void network::register_targets()
		{
			seq_target_map_.clear();
			channel_target_map_.clear();
			CYNG_LOG_INFO(logger_, "register target DEMO" );
			cyng::vector_t prg;
			prg
				<< cyng::generate_invoke_unwinded("req.register.push.target", "DEMO", static_cast<std::uint16_t>(0xffff), static_cast<std::uint8_t>(1))
				<< cyng::generate_invoke_unwinded("net.insert.rel", cyng::invoke("ipt.push.seq"), "DEMO")
				<< cyng::generate_invoke_unwinded("stream.flush")
				;

			bus_->vm_.async_run(std::move(prg));

		}
	}
}
