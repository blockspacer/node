/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_IP_MASTER_TASK_REBOOT_H
#define NODE_IP_MASTER_TASK_REBOOT_H

#include <smf/cluster/bus.h>
#include <smf/ipt/defs.h>
#include <cyng/log.h>
#include <cyng/async/mux.h>
#include <cyng/vm/controller.h>

namespace node
{

	class reboot
	{
	public:
		using msg_0 = std::tuple<ipt::response_type>;
		using msg_1 = std::tuple<>;
		using signatures_t = std::tuple<msg_0, msg_1>;

	public:
		reboot(cyng::async::base_task* btp
			, cyng::logging::log_ptr
			, bus::shared_type bus
			, cyng::controller& vm
			, boost::uuids::uuid tag_remote
			, std::uint64_t seq_cluster		//	cluster seq
			, cyng::buffer_t const& server	//	server id
			, std::string user
			, std::string pwd
			, boost::uuids::uuid tag_ctx);
		cyng::continuation run();
		void stop();

		/**
		 * @brief slot [0]
		 *
		 * sucessful cluster login
		 */
		cyng::continuation process(ipt::response_type);

		/**
		 * @brief slot [1]
		 *
		 * session closed
		 */
		cyng::continuation process();

	private:
		void send_reboot_cmd();

	private:
		cyng::async::base_task& base_;
		cyng::logging::log_ptr logger_;
		cyng::controller& vm_;
		const boost::uuids::uuid tag_remote_;
		const cyng::buffer_t server_id_;
		const std::string user_, pwd_;
		const std::chrono::system_clock::time_point start_;
		bool is_waiting_;
	};
	
}

namespace cyng {
	namespace async {

		//
		//	initialize static slot names
		//
		template <>
		std::map<std::string, std::size_t> cyng::async::task<node::reboot>::slot_names_;
    }
}

#endif
