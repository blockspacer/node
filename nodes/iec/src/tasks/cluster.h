/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#ifndef NODE_IP_MASTER_TASK_CLUSTER_H
#define NODE_IP_MASTER_TASK_CLUSTER_H

//#include "../processor.h"

#include <smf/cluster/bus.h>
#include <smf/cluster/config.h>
//#include <smf/https/srv/server.h>

#include <cyng/log.h>
#include <cyng/async/mux.h>
#include <cyng/async/policy.h>

namespace node
{

	class cluster
	{
	public:
		using msg_0 = std::tuple<cyng::version>;
		using msg_1 = std::tuple<>;
		using signatures_t = std::tuple<msg_0, msg_1>;

	public:
		cluster(cyng::async::base_task* bt
			, cyng::logging::log_ptr
			, boost::uuids::uuid tag
			, cluster_config_t const& cfg
			, boost::asio::ip::tcp::endpoint ep
			, std::string const& doc_root
			, std::vector<std::string> const& sub_protocols);
		void run();
		void stop();

		/**
		 * @brief slot [0]
		 *
		 * sucessful cluster login
		 */
		cyng::continuation process(cyng::version const&);

		/**
		 * @brief slot [1]
		 *
		 * reconnect
		 */
		cyng::continuation process();

	private:
		void connect();
		void reconfigure(cyng::context& ctx);
		void reconfigure_impl();
		//void session_callback(boost::uuids::uuid, cyng::vector_t&&);

	private:
		cyng::async::base_task& base_;
		bus::shared_type bus_;
		cyng::logging::log_ptr logger_;
		const cluster_config_t	config_;
		std::size_t master_;
		//https::server	server_;
		//processor processor_;
	};
	
}

#endif