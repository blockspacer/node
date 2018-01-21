/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#ifndef NODE_IPT_CONFIG_H
#define NODE_IPT_CONFIG_H

#include <NODE_project_info.h>
#include <smf/ipt/scramble_key.h>
#include <cyng/intrinsics/sets.h>
#include <chrono>

namespace node
{
	namespace ipt
	{
		struct master_record
		{
			const std::string host_;
			const std::string service_;
			const std::string account_;
			const std::string pwd_;
			const scramble_key	sk_;
			const bool scrambled_;
			const std::chrono::seconds monitor_;
			master_record();
			master_record(std::string const&, std::string const&, std::string const&, std::string const&, scramble_key const&, bool, int);
		};

		/**
		 * Declare a vector with all cluster configurations.
		 */
		using master_config_t = std::vector<master_record>;

		/**
		 * Convinience function to read cluster configuration
		 */
		master_config_t load_cluster_cfg(cyng::vector_t const& cfg);
		master_record load_cluster_rec(cyng::tuple_t const& cfg);
	}
}

#endif
