/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include <smf/cluster/config.h>
#include <cyng/dom/reader.h>

namespace node
{
	cluster_record load_cluster_rec(cyng::tuple_t const& cfg)
	{
		auto dom = cyng::make_reader(cfg);
		return cluster_record(cyng::value_cast<std::string>(dom.get("host"), "")
			, cyng::value_cast<std::string>(dom.get("service"), "")
			, cyng::value_cast<std::string>(dom.get("account"), "")
			, cyng::value_cast<std::string>(dom.get("pwd"), "")
			, cyng::value_cast(dom.get("salt"), 1)
			, cyng::value_cast(dom.get("monitor"), 60)
			, cyng::value_cast(dom.get("auto-config"), false)
			, cyng::value_cast(dom.get("group"), 0));
	}

	cluster_config_t load_cluster_cfg(cyng::vector_t const& cfg)
	{
		cluster_config_t cluster_cfg;
		cluster_cfg.reserve(cfg.size());

		std::for_each(cfg.begin(), cfg.end(), [&cluster_cfg](cyng::object const& obj) {
			cyng::tuple_t tmp;
			cluster_cfg.push_back(load_cluster_rec(cyng::value_cast(obj, tmp)));
		});

		return cluster_cfg;
	}

	cluster_record::cluster_record()
		: host_()
		, service_()
		, account_()
		, pwd_()
		, salt_(1)
		, monitor_(0)
		, auto_config_(false)
		, group_(0)
	{}

	cluster_record::cluster_record(std::string const& host
		, std::string const& service
		, std::string const& account
		, std::string const& pwd
		, int salt
		, int monitor
		, bool auto_config
		, int group)
	: host_(host)
		, service_(service)
		, account_(account)
		, pwd_(pwd)
		, salt_(salt)
		, monitor_(monitor)
		, auto_config_(auto_config)
		, group_(group)
	{}

	cluster_redundancy::cluster_redundancy(cluster_config_t const& cfg)
		: config_(cfg)
		, master_(0)
	{
		BOOST_ASSERT(!config_.empty());
	}

	bool cluster_redundancy::next() const
	{
		if (!config_.empty())
		{
			master_++;
			if (master_ == config_.size())
			{
				master_ = 0;
			}
			return true;
		}
		return false;
	}

	cluster_record const& cluster_redundancy::get() const
	{
		return config_.at(master_);
	}


}
