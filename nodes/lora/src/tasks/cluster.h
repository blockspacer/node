/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#ifndef NODE_IP_MASTER_TASK_CLUSTER_H
#define NODE_IP_MASTER_TASK_CLUSTER_H

#include <smf/cluster/bus.h>
#include <smf/cluster/config.h>
#include <cyng/log.h>
#include <cyng/async/mux.h>
#include <cyng/async/policy.h>
//#include <cyng/store/db.h>

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
			, cluster_config_t const& cfg);
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
		//void db_insert(cyng::context& ctx);
		//void task_resume(cyng::context& ctx);

	private:
		cyng::async::base_task& base_;
		bus::shared_type bus_;
		cyng::logging::log_ptr logger_;
		const cluster_config_t	config_;
		std::size_t master_;

	};
	
}

#endif