/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_CSV_TASK_STORAGE_DB_H
#define NODE_CSV_TASK_STORAGE_DB_H

#include <smf/cluster/bus.h>
#include <cyng/log.h>
#include <cyng/async/mux.h>
#include <cyng/async/policy.h>
#include <cyng/db/session_pool.h>
#include <cyng/table/meta_interface.h>
#include <cyng/sql.h>

namespace node
{
	class storage_db
	{
	public:
		using msg_0 = std::tuple<std::chrono::system_clock::time_point
			, std::chrono::hours>;
		using msg_1 = std::tuple<std::chrono::system_clock::time_point
			, std::int32_t>;
		using signatures_t = std::tuple<msg_0, msg_1>;

	public:
		storage_db(cyng::async::base_task* bt
			, cyng::logging::log_ptr
			, bus::shared_type bus
			, cyng::param_map_t
			, cyng::param_map_t
			, cyng::param_map_t);
		cyng::continuation run();
		void stop();

		/**
		 * slot [0] - generate CSV file (15 min profile)
		 */
		cyng::continuation process(std::chrono::system_clock::time_point start
			, std::chrono::hours interval);

		/**
		 * slot [1] - generate CSV file (24 h profile)
		 * 
		 * @param end last timepoint for this months to report for
		 * @param days number of days in the specified month
		 */
		cyng::continuation process(std::chrono::system_clock::time_point end
			, std::int32_t days);

		/**
		 * static method to create tables.
		 */
		static cyng::table::mt_table init_meta_map(std::string const&);

	private:
		void generate_csv_files_daily(std::chrono::system_clock::time_point start
			, std::chrono::hours interval);

		void generate_csv_files_monthly(std::chrono::system_clock::time_point start
			, std::int32_t days);

		/**
		 * Get servers IDs for 15 min profile (8181c78611ff)
		 */
		std::vector<std::string> get_server_ids(std::chrono::system_clock::time_point start
			, std::chrono::system_clock::time_point end
			, cyng::sql::command&
			, cyng::db::statement_ptr);
		
		/**
		 * get OBIS codes that are used for the specified time range and used for 15 min profile (8181c78611ff)
		 */
		std::vector<std::string> get_obis_codes(std::chrono::system_clock::time_point start
			, std::chrono::system_clock::time_point end
			, std::string const& id
			, cyng::sql::command&
			, cyng::db::statement_ptr);


		std::ofstream open_file_15_min_profile(std::chrono::system_clock::time_point start
			, std::chrono::system_clock::time_point end
			, std::string const& id
			, std::vector<std::string> const&);

		std::ofstream open_file_24_h_profile(std::chrono::system_clock::time_point end);

		void collect_data_15_min_profile(std::ofstream&
			, std::chrono::system_clock::time_point start
			, std::chrono::system_clock::time_point end
			, cyng::sql::command&
			, cyng::db::statement_ptr
			, std::string const& id
			, std::vector<std::string> const& obis_code);

		void collect_data_24_h_profile(std::ofstream&
			, std::chrono::system_clock::time_point start
			, std::chrono::system_clock::time_point end
			, cyng::sql::command&
			, cyng::db::statement_ptr
			, std::string server
			, std::string code);

		/**
		 * Collect unique server/OBIS combinations for 24 h profile
		 */
		std::vector<std::pair<std::string, std::string>> get_unique_server_obis_combinations(std::chrono::system_clock::time_point start
			, std::chrono::system_clock::time_point end
			, cyng::sql::command& cmd
			, cyng::db::statement_ptr stmt);

	private:
		cyng::async::base_task& base_;
		cyng::logging::log_ptr logger_;
		bus::shared_type bus_;
		cyng::db::session_pool pool_;
		const cyng::param_map_t cfg_db_;
		const cyng::param_map_t cfg_clock_day_;
		const cyng::param_map_t cfg_clock_month_;

		const std::string schema_;
		cyng::table::mt_table	meta_map_;

		/**
		 * task state
		 */
		enum {
			TASK_STATE_WAITING_,
			TASK_STATE_OPEN_,
		} state_;

	};
	
}

#endif