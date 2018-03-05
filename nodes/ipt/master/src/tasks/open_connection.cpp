/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#include "open_connection.h"
#include <smf/cluster/generator.h>
#include <smf/ipt/response.hpp>
#include <cyng/async/task/task_builder.hpp>
#include <cyng/io/serializer.h>
#include <cyng/vm/generator.h>
#include <boost/uuid/random_generator.hpp>

namespace node
{
	open_connection::open_connection(cyng::async::base_task* btp
		, cyng::logging::log_ptr logger
		, bus::shared_type bus
		, cyng::controller& vm
		, boost::uuids::uuid tag
		, std::size_t seq
		, std::string number
		, cyng::param_map_t const& options
		, cyng::param_map_t const& bag
		, std::chrono::seconds timeout)
	: base_(*btp)
		, logger_(logger)
		, bus_(bus)
		, vm_(vm)
		, tag_(tag)
		, seq_(seq)
		, number_(number)
		, options_(options)
		, bag_(bag)
		, timeout_(timeout)
		, response_(ipt::tp_res_open_connection_policy::UNREACHABLE)
	{
		CYNG_LOG_INFO(logger_, "task #"
		<< base_.get_id()
		<< " <"
		<< base_.get_class_name()
		<< "> is running");

	}

	void open_connection::run()
	{	
		//
		//	* forward connection opne request to device
		//	* store sequence - task relation
		//	* start timer to check connection setup
		//

		vm_.async_run(cyng::generate_invoke("log.msg.info", "client.req.open.connection.forward", number_));

		cyng::vector_t prg;
		prg
			<< cyng::generate_invoke_unwinded("req.open.connection", number_)
			<< cyng::generate_invoke_unwinded("session.store.relation", cyng::invoke("ipt.push.seq"), base_.get_id())
			<< cyng::generate_invoke_unwinded("stream.flush")
			;
		vm_.async_run(std::move(prg));

		//
		//	start monitor
		//
		base_.suspend(timeout_);
	}

	void open_connection::stop()
	{
		//
		//	send response to cluster master
		//
		bus_->vm_.async_run(client_res_open_connection(tag_
			, seq_
			, ipt::tp_res_open_connection_policy::is_success(response_)
			, options_
			, bag_));

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
	cyng::continuation open_connection::process(ipt::response_type res)
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> received response ["
			<< ipt::tp_res_open_connection_policy::get_response_name(res)
			<< "]");

		response_ = res;
		return cyng::continuation::TASK_STOP;
	}

}