/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#include "cluster.h"
#include <smf/cluster/generator.h>
#include <cyng/async/task/task_builder.hpp>
#include <cyng/io/serializer.h>
#include <cyng/vm/generator.h>
#include <boost/uuid/random_generator.hpp>

namespace node
{
	cluster::cluster(cyng::async::base_task* btp
		, cyng::logging::log_ptr logger
		, cluster_config_t const& cfg_cluster
		, std::string const& address
		, std::string const& service
		, int timeout
		, bool auto_answer
		, std::chrono::milliseconds guard_time)
	: base_(*btp)
		, bus_(bus_factory(btp->mux_, logger, boost::uuids::random_generator()(), btp->get_id()))
		, logger_(logger)
		, config_(cfg_cluster)
		, address_(address)
		, service_(service)
		, server_(btp->mux_, logger_, bus_, timeout, auto_answer, guard_time)
		, master_(0)
	{
		CYNG_LOG_INFO(logger_, "task #"
		<< base_.get_id()
		<< " <"
		<< base_.get_class_name()
		<< "> is running");

		//
		//	implement request handler
		//
		bus_->vm_.register_function("bus.reconfigure", 1, std::bind(&cluster::reconfigure, this, std::placeholders::_1));

	}

	void cluster::run()
	{	
		if (!bus_->is_online())
		{
			connect();
		}
	}

	void cluster::stop()
	{
		//
		//	stop server
		//
		server_.close();

		//
		//	sign off from cluster
		//
		bus_->stop();
		CYNG_LOG_INFO(logger_, "cluster is stopped");
	}

	//	slot 0
	cyng::continuation cluster::process(cyng::version const& v)
	{
		if (v < cyng::version(NODE_VERSION_MAJOR, NODE_VERSION_MINOR))
		{
			CYNG_LOG_WARNING(logger_, "insufficient cluster protocol version: "	<< v);
		}

		//
		//	start modem server
		//
		server_.run(address_, service_);

		return cyng::continuation::TASK_CONTINUE;
	}

	cyng::continuation cluster::process()
	{
		//
		//	Connection to master lost
		//

		//
		//	stop server
		//
		server_.close();

		//
		//	switch to other configuration
		//
		reconfigure_impl();

		//
		//	continue task
		//
		return cyng::continuation::TASK_CONTINUE;
	}

	void cluster::connect()
	{

		bus_->vm_.async_run(bus_req_login(config_[master_].host_
			, config_[master_].service_
			, config_[master_].account_
			, config_[master_].pwd_
			, config_[master_].auto_config_
			, config_[master_].group_
			, "modem"));

		CYNG_LOG_INFO(logger_, "cluster login request is sent");

	}

	void cluster::reconfigure(cyng::context& ctx)
	{
		reconfigure_impl();
	}


	void cluster::reconfigure_impl()
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
			CYNG_LOG_ERROR(logger_, "cluster login failed - no other redundancy available");
		}

		//
		//	trigger reconnect 
		//
		CYNG_LOG_INFO(logger_, "reconnect to cluster in "
			<< config_[master_].monitor_.count()
			<< " seconds");
		base_.suspend(config_[master_].monitor_);

	}

}
