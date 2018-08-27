/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#include "close_connection.h"
#include <smf/cluster/generator.h>
#include <smf/ipt/response.hpp>
#include <cyng/async/task/base_task.h>
#include <cyng/io/serializer.h>
#include <cyng/vm/generator.h>
#include <boost/uuid/random_generator.hpp>

namespace node
{
	close_connection::close_connection(cyng::async::base_task* btp
		, cyng::logging::log_ptr logger
		, bus::shared_type bus
		, cyng::controller& vm
		, bool shutdown
		, boost::uuids::uuid tag
		, std::size_t seq
		, cyng::param_map_t const& options
		, cyng::param_map_t const& bag
		, std::chrono::seconds timeout)
	: base_(*btp)
		, logger_(logger)
		, bus_(bus)
		, vm_(vm)
		, shutdown_(shutdown)
		, tag_(tag)
		, seq_(seq)
		, options_(options)
		, bag_(bag)
		, timeout_(timeout)
		, response_(ipt::tp_res_close_connection_policy::CONNECTION_CLEARING_FAILED)
		, start_(std::chrono::system_clock::now())
		, is_waiting_(false)
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> is running in "
			<< (shutdown_ ? "shutdown" : "normal")
			<< " mode until "
			<< cyng::to_str(start_ + timeout_));
	}

	cyng::continuation close_connection::run()
	{	
		if (!is_waiting_)
		{
			//
			//	update task state
			//
			is_waiting_ = true;

			//
			//	* forward connection close request to device
			//	* store sequence - task relation
			//	* start timer to check connection setup
			//

			//	prepare IP-T command: close connection request
			vm_.async_run(cyng::generate_invoke("req.close.connection"))

				//	tie IP-T sequence with this task id
				.async_run(cyng::generate_invoke("session.store.relation", cyng::invoke("ipt.push.seq"), base_.get_id()))

				//	send IP-T request
				.async_run(cyng::generate_invoke("stream.flush"))

				//	logging
				.async_run(cyng::generate_invoke("log.msg.info", "client.req.close.connection.forward", cyng::invoke("ipt.push.seq"), timeout_))
				;

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
			<< "> timeout after "
			<< cyng::to_str(std::chrono::system_clock::now() - start_));

		//
		//	close session
		//	could crash if session is already gone
		//
		vm_.async_run(cyng::generate_invoke("ip.tcp.socket.shutdown"));
		vm_.async_run(cyng::generate_invoke("ip.tcp.socket.close"));

		//
		//	stop this task
		//
		return cyng::continuation::TASK_STOP;
	}

	void close_connection::stop()
	{
		//
		//	send response to cluster master
		//
		if (!shutdown_)
		{
			bus_->vm_.async_run(client_res_close_connection(tag_
				, seq_
				, ipt::tp_res_close_connection_policy::is_success(response_)
				, options_
				, bag_));
		}

		//
		//	terminate task
		//
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> is stopped");

	}

	//	slot 0
	cyng::continuation close_connection::process(ipt::response_type res)
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> received response ["
			<< ipt::tp_res_close_connection_policy::get_response_name(res)
			<< "]");

		response_ = res;
		return cyng::continuation::TASK_STOP;
	}

	//	slot 1
	cyng::continuation close_connection::process()
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> session closed");

		shutdown_ = true;
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
		std::map<std::string, std::size_t> cyng::async::task<node::close_connection>::slot_names_({ { "shutdown", 1 } });

	}
}