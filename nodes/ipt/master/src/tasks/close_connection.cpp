/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#include "close_connection.h"
#include <smf/cluster/generator.h>
#include <smf/ipt/response.hpp>
#include <cyng/async/task/task_builder.hpp>
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
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> is running");
	}

	void close_connection::run()
	{	
		//
		//	* forward connection opne request to device
		//	* store sequence - task relation
		//	* start timer to check connection setup
		//

		cyng::vector_t prg;
		prg
			<< cyng::generate_invoke_unwinded("req.close.connection")
			<< cyng::generate_invoke_unwinded("session.store.relation", cyng::invoke("ipt.push.seq"), base_.get_id())
			<< cyng::generate_invoke_unwinded("stream.flush")
			<< cyng::generate_invoke_unwinded("log.msg.info", "client.req.close.connection.forward", cyng::invoke("ipt.push.seq"))
			;
		vm_.async_run(std::move(prg));

		//
		//	start monitor
		//
		base_.suspend(timeout_);
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

}