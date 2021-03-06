/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Sylko Olzscher
 *
 */

#ifndef NODE_TSDB_TASK_MULTIPLE_H
#define NODE_TSDB_TASK_MULTIPLE_H

#include <cyng/log.h>
#include <cyng/async/mux.h>
#include <cyng/async/policy.h>
#include <cyng/intrinsics/version.h>

namespace node
{

	class multiple
	{
	public:
		using msg_0 = std::tuple<std::string
			, std::size_t
			, std::chrono::system_clock::time_point	//	time stamp
			, boost::uuids::uuid	//	session tag
			, std::string	// device name
			, std::string	// event
			, std::string	// description
		>;
		using signatures_t = std::tuple<msg_0>;

	public:
		multiple(cyng::async::base_task* bt
			, cyng::logging::log_ptr
			, boost::filesystem::path out
			, std::string prefix
			, std::string suffix
			, std::chrono::seconds period);

		cyng::continuation run();
		void stop();

		/**
		 * @brief slot [0]
		 *
		 * insert
		 */
		cyng::continuation process(std::string
			, std::size_t
			, std::chrono::system_clock::time_point	//	time stamp
			, boost::uuids::uuid	//	session tag
			, std::string	// device name
			, std::string	// event
			, std::string);


	private:

	private:
		cyng::async::base_task& base_;
		cyng::logging::log_ptr logger_;
		boost::filesystem::path const out_;
		std::chrono::seconds const period_;
	};
}

#endif