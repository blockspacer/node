/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#ifndef NODE_IPT_STORE_TASK_STORAGE_DB_H
#define NODE_IPT_STORE_TASK_STORAGE_DB_H

#include <cyng/log.h>
#include <cyng/async/mux.h>
#include <cyng/async/policy.h>
#include <cyng/db/session_pool.h>
#include <cyng/table/meta_interface.h>
#include <cyng/vm/context.h>
#include <map>
#include <boost/uuid/random_generator.hpp>

namespace node
{
	class storage_db
	{
	public:
		using msg_0 = std::tuple<std::uint32_t, std::uint32_t, std::string, cyng::buffer_t>;
		using signatures_t = std::tuple<msg_0>;

	public:
		storage_db(cyng::async::base_task* bt
			, cyng::logging::log_ptr
			, cyng::param_map_t);
		void run();
		void stop();

		/**
		 * @brief slot [0]
		 *
		 * push data
		 */
		cyng::continuation process(std::uint32_t, std::uint32_t, std::string const&, cyng::buffer_t const&);

		/**
		 * static method to create tables.
		 */
		static int init_db(cyng::tuple_t);
		static std::map<std::string, cyng::table::meta_table_ptr> init_meta_map();

	private:
		void stop_writer(cyng::context& ctx);

	private:
		cyng::async::base_task& base_;
		cyng::logging::log_ptr logger_;
		cyng::db::session_pool pool_;
		std::map<std::string, cyng::table::meta_table_ptr>	meta_map_;
		std::map<std::uint64_t, std::size_t>	lines_;
		boost::uuids::random_generator rng_;

	};
}

#endif