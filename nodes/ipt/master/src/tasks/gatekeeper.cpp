/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include "gatekeeper.h"
#include <smf/cluster/generator.h>
#include <cyng/async/task/task_builder.hpp>
#include <cyng/io/serializer.h>
#include <cyng/vm/generator.h>
#include <cyng/factory/chrono_factory.h>
#include <boost/uuid/random_generator.hpp>

namespace node
{
	gatekeeper::gatekeeper(cyng::async::base_task* btp
		, cyng::logging::log_ptr logger
		, cyng::controller& vm
		, boost::uuids::uuid tag
		, std::chrono::seconds timeout)
	: base_(*btp)
		, logger_(logger)
		, vm_(vm)
		, tag_(tag)
		, timeout_(timeout)
		, response_(ipt::ctrl_res_login_public_policy::ACCOUNT_LOCKED)
		, start_(std::chrono::system_clock::now())
		, is_waiting_(false)
	{
		CYNG_LOG_INFO(logger_, "task #"
		<< base_.get_id()
		<< " <"
		<< base_.get_class_name()
		<< "> is running: "
		<< tag_
		<< " until "
		<< cyng::to_str(start_ + timeout_));

	}

	cyng::continuation gatekeeper::run()
	{	
		if (!is_waiting_)
		{
			//
			//	update task state
			//
			is_waiting_ = true;

			//
			//	start monitor
			//
			base_.suspend(timeout_);

			return cyng::continuation::TASK_CONTINUE;
		}

		CYNG_LOG_WARNING(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> timeout");

		return cyng::continuation::TASK_STOP;
	}

	//	slot 1
	cyng::continuation gatekeeper::process()
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> session closed");

		//
		//	session already stopped
		//
		response_ = ipt::ctrl_res_login_public_policy::GENERAL_ERROR;
		return cyng::continuation::TASK_STOP;
	}


	void gatekeeper::stop()
	{
		if (response_ == ipt::ctrl_res_login_public_policy::ACCOUNT_LOCKED)
		{
			//
			//	no login response received - stop client
			//	Could crash if session is already closed.
			//
			if (!vm_.is_halted()) {

				CYNG_LOG_WARNING(logger_, "task #"
					<< base_.get_id()
					<< " <"
					<< base_.get_class_name()
					<< "> stop "
					<< tag_);

				vm_.async_run({ cyng::generate_invoke("ip.tcp.socket.shutdown"), cyng::generate_invoke("ip.tcp.socket.close") });
			}
			else {
				CYNG_LOG_WARNING(logger_, "task #"
					<< base_.get_id()
					<< " <"
					<< base_.get_class_name()
					<< "> stop (session already halted) "
					<< tag_);
			}
		}

		//
		//	terminate task
		//
		auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_);
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> is stopped: "
			<< tag_
			<< " after "
			<< uptime.count()
			<< " milliseconds");

	}

	//	slot 0
	cyng::continuation gatekeeper::process(ipt::response_type res)
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> "
			<< tag_
			<< " received response ["
			<< ipt::ctrl_res_login_public_policy::get_response_name(res)
			<< "]");

		response_ = res;
		return cyng::continuation::TASK_STOP;
	}

}

#include <cyng/async/task/task.hpp>

namespace cyng {
	namespace async {

		//
		//	initialize static slot names
		//
		template <>
		std::map<std::string, std::size_t> cyng::async::task<node::gatekeeper>::slot_names_({ { "shutdown", 1 } });

	}
}