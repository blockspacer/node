/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Sylko Olzscher
 *
 */

#include "controller.h"
#include "storage.h"
 //#include "server.h"
#include <NODE_project_info.h>
#include <smf/ipt/config.h>
#include <smf/sml/obis_db.h>
#include <smf/mbus/defs.h>

#include <cyng/factory/set_factory.h>
#include <cyng/io/serializer.h>
#include <cyng/dom/reader.h>
#include <cyng/value_cast.hpp>
#include <cyng/numeric_cast.hpp>
#include <cyng/set_cast.h>
#include <cyng/vector_cast.hpp>
#include <cyng/sys/mac.h>
#include <cyng/store/db.h>
#include <cyng/table/meta.hpp>
#include <cyng/sql.h>
#include <cyng/rnd.h>

#include <cyng/db/connection_types.h>
#include <cyng/db/session.h>
#include <cyng/db/interface_session.h>
#include <cyng/db/sql_table.h>

#include <boost/core/ignore_unused.hpp>

namespace node
{
	controller::controller(unsigned int index
		, unsigned int pool_size
		, std::string const& json_path
		, std::string node_name)
	: ctl(index
		, pool_size
		, json_path
		, node_name)
	{}

	cyng::vector_t controller::create_config(std::fstream& fout, boost::filesystem::path&& tmp, boost::filesystem::path&& cwd) const
	{

		//
		//	random uint32
		//	reconnect to master on different times
		//
		cyng::crypto::rnd_num<int> rnd_monitor(10, 60);

		//
		//	generate a random serial number with a length of
		//	8 characters
		//
		auto rnd_sn = cyng::crypto::make_rnd_num();
		std::stringstream sn_ss(rnd_sn(8));
		std::uint32_t sn{ 0 };
		sn_ss >> sn;
			
		//
		//	get adapter data
		//
		auto macs = cyng::sys::retrieve_mac48();
		if (macs.empty())
		{
			macs.push_back(cyng::generate_random_mac48());
		}

		return cyng::vector_factory({
			cyng::tuple_factory(cyng::param_factory("log-dir", tmp.string())
				, cyng::param_factory("log-level", "INFO")
				, cyng::param_factory("tag", uidgen_())
				, cyng::param_factory("generated", std::chrono::system_clock::now())
				, cyng::param_factory("log-pushdata", false)	//	log file for each channel
				, cyng::param_factory("accept-all-ids", false)	//	accept only the specified MAC id
				, cyng::param_factory("gpio-path", "/sys/class/gpio")	//	accept only the specified MAC id
				, cyng::param_factory("gpio-list", cyng::vector_factory({46, 47, 50, 53}))

				, cyng::param_factory("DB", cyng::tuple_factory(
					cyng::param_factory("type", "SQLite"),
					cyng::param_factory("file-name", (cwd / "segw.database").string()),
					cyng::param_factory("busy-timeout", 12),		//	seconds
					cyng::param_factory("watchdog", 30),		//	for database connection
					cyng::param_factory("pool-size", 1)		//	no pooling for SQLite
				))

				//	on this address the gateway acts as a server
				//	configuration interface
				, cyng::param_factory("server", cyng::tuple_factory(
					cyng::param_factory("address", "0.0.0.0"),
					cyng::param_factory("service", "7259"),
					cyng::param_factory("discover", "5798"),	//	UDP
					cyng::param_factory("account", "operator"),
					cyng::param_factory("pwd", "operator")
				))

				//	hardware
				, cyng::param_factory("hardware", cyng::tuple_factory(
					cyng::param_factory("manufacturer", "solosTec"),	//	manufacturer (81 81 C7 82 03 FF - OBIS_DATA_MANUFACTURER)
					cyng::param_factory("model", "virtual.gateway"),	//	Typenschlüssel (81 81 C7 82 09 FF --> 81 81 C7 82 0A 01)
					cyng::param_factory("serial", sn),	//	Seriennummer (81 81 C7 82 09 FF --> 81 81 C7 82 0A 02)
					cyng::param_factory("class", "129-129:199.130.83*255"),	//	device class (81 81 C7 82 02 FF - OBIS_CODE_DEVICE_CLASS) "2D 2D 2D"
					cyng::param_factory("mac", macs.at(0))	//	take first available MAC to build a server id (05 xx xx ..., 81 81 C7 82 04 FF - OBIS_CODE_SERVER_ID)
				))

				//	wireless M-Bus
				//	stty -F /dev/ttyAPP0 raw
				//	stty -F /dev/ttyAPP0  -echo -echoe -echok
				//	stty -F /dev/ttyAPP0 115200 
				//	cat /dev/ttyAPP0 | hexdump 
				, cyng::param_factory("wireless-LMN", cyng::tuple_factory(
#if BOOST_OS_WINDOWS
					cyng::param_factory("enabled", false),
					cyng::param_factory("port", "COM3"),	//	USB serial port
					//	if port number is greater than 9 the following syntax is required: "\\\\.\\COM12"
#else
					cyng::param_factory("enabled", true),
					cyng::param_factory("port", "/dev/ttyAPP0"),
#endif
					cyng::param_factory("databits", 8),
					cyng::param_factory("parity", "none"),	//	none, odd, even
					cyng::param_factory("flow-control", "none"),	//	none, software, hardware
					cyng::param_factory("stopbits", "one"),	//	one, onepointfive, two
					cyng::param_factory("speed", 115200),

					cyng::param_factory(sml::OBIS_W_MBUS_PROTOCOL.to_str(), static_cast<std::uint8_t>(mbus::MODE_S)),	//	0 = T-Mode, 1 = S-Mode, 2 = S/T Automatic
					cyng::param_factory(sml::OBIS_W_MBUS_MODE_S.to_str(), 30),	//	seconds
					cyng::param_factory(sml::OBIS_W_MBUS_MODE_T.to_str(), 20),	//	seconds
					cyng::param_factory(sml::OBIS_W_MBUS_REBOOT.to_str(), 0),	//	0 = no reboot
					cyng::param_factory(sml::OBIS_W_MBUS_POWER.to_str(), static_cast<std::uint8_t>(mbus::STRONG)),	//	low, basic, average, strong (unused)
					cyng::param_factory(sml::OBIS_W_MBUS_INSTALL_MODE.to_str(), true),	//	install mode

					cyng::param_factory("transparent-mode", false),
					cyng::param_factory("transparent-port", 12001)
				))

				, cyng::param_factory("wired-LMN", cyng::tuple_factory(
#if BOOST_OS_WINDOWS
					cyng::param_factory("enabled", false),
					cyng::param_factory("port", "COM1"),
#else
					cyng::param_factory("enabled", true),
					cyng::param_factory("port", "/dev/ttyAPP1"),
#endif
					cyng::param_factory("databits", 8),
					cyng::param_factory("parity", "none"),	//	none, odd, even
					cyng::param_factory("flow-control", "none"),	//	none, software, hardware
					cyng::param_factory("stopbits", "one"),	//	one, onepointfive, two
					cyng::param_factory("speed", 921600),
					cyng::param_factory("transparent-mode", false),
					cyng::param_factory("transparent-port", 12002)
				))

				, cyng::param_factory("if-1107", cyng::tuple_factory(
#ifdef _DEBUG
					cyng::param_factory(sml::OBIS_CODE_IF_1107_ACTIVE.to_str() + "-descr", "OBIS_CODE_IF_1107_ACTIVE"),	//	active
					cyng::param_factory(sml::OBIS_CODE_IF_1107_LOOP_TIME.to_str() + "-descr", "OBIS_CODE_IF_1107_LOOP_TIME"),	//	loop timeout in seconds
					cyng::param_factory(sml::OBIS_CODE_IF_1107_RETRIES.to_str() + "-descr", "OBIS_CODE_IF_1107_RETRIES"),	//	retries
					cyng::param_factory(sml::OBIS_CODE_IF_1107_MIN_TIMEOUT.to_str() + "-descr", "OBIS_CODE_IF_1107_MIN_TIMEOUT"),	//	min. timeout (milliseconds)
					cyng::param_factory(sml::OBIS_CODE_IF_1107_MAX_TIMEOUT.to_str() + "-descr", "OBIS_CODE_IF_1107_MAX_TIMEOUT"),	//	max. timeout (milliseconds)
					cyng::param_factory(sml::OBIS_CODE_IF_1107_MAX_DATA_RATE.to_str() + "-descr", "OBIS_CODE_IF_1107_MAX_DATA_RATE"),	//	max. databytes
					cyng::param_factory(sml::OBIS_CODE_IF_1107_RS485.to_str() + "-descr", "OBIS_CODE_IF_1107_RS485"),	//	 true = RS485, false = RS232
					cyng::param_factory(sml::OBIS_CODE_IF_1107_PROTOCOL_MODE.to_str() + "-descr", "OBIS_CODE_IF_1107_PROTOCOL_MODE"),	//	protocol mode 0 == A, 1 == B, 2 == C (A...E)
					cyng::param_factory(sml::OBIS_CODE_IF_1107_AUTO_ACTIVATION.to_str() + "-descr", "OBIS_CODE_IF_1107_AUTO_ACTIVATION"),	//	auto activation
					cyng::param_factory(sml::OBIS_CODE_IF_1107_TIME_GRID.to_str() + "-descr", "OBIS_CODE_IF_1107_TIME_GRID"),
					cyng::param_factory(sml::OBIS_CODE_IF_1107_TIME_SYNC.to_str() + "-descr", "OBIS_CODE_IF_1107_TIME_SYNC"),
					cyng::param_factory(sml::OBIS_CODE_IF_1107_MAX_VARIATION.to_str() + "-descr", "OBIS_CODE_IF_1107_MAX_VARIATION"),	//	max. variation in seconds
#endif
					cyng::param_factory(sml::OBIS_CODE_IF_1107_ACTIVE.to_str(), true),	//	active
					cyng::param_factory(sml::OBIS_CODE_IF_1107_LOOP_TIME.to_str(), 60),	//	loop timeout in seconds
					cyng::param_factory(sml::OBIS_CODE_IF_1107_RETRIES.to_str(), 3),	//	retries
					cyng::param_factory(sml::OBIS_CODE_IF_1107_MIN_TIMEOUT.to_str(), 200),	//	min. timeout (milliseconds)
					cyng::param_factory(sml::OBIS_CODE_IF_1107_MAX_TIMEOUT.to_str(), 5000),	//	max. timeout (milliseconds)
					cyng::param_factory(sml::OBIS_CODE_IF_1107_MAX_DATA_RATE.to_str(), 10240),	//	max. databytes
					cyng::param_factory(sml::OBIS_CODE_IF_1107_RS485.to_str(), true),	//	 true = RS485, false = RS232
					cyng::param_factory(sml::OBIS_CODE_IF_1107_PROTOCOL_MODE.to_str(), 2),	//	protocol mode 0 == A, 1 == B, 2 == C (A...E)
					cyng::param_factory(sml::OBIS_CODE_IF_1107_AUTO_ACTIVATION.to_str(), true),	//	auto activation
					cyng::param_factory(sml::OBIS_CODE_IF_1107_TIME_GRID.to_str(), 900),
					cyng::param_factory(sml::OBIS_CODE_IF_1107_TIME_SYNC.to_str(), 14400),
					cyng::param_factory(sml::OBIS_CODE_IF_1107_MAX_VARIATION.to_str(), 9)	//	max. variation in seconds

				))
				, cyng::param_factory("mbus", cyng::tuple_factory(
					cyng::param_factory(sml::OBIS_CLASS_MBUS_RO_INTERVAL.to_str(), 3600),	//	readout interval in seconds
					cyng::param_factory(sml::OBIS_CLASS_MBUS_SEARCH_INTERVAL.to_str(), 0),	//	search interval in seconds
					cyng::param_factory(sml::OBIS_CLASS_MBUS_SEARCH_DEVICE.to_str(), true),	//	search device now and by restart
					cyng::param_factory(sml::OBIS_CLASS_MBUS_AUTO_ACTICATE.to_str(), false),	//	automatic activation of meters 
					cyng::param_factory(sml::OBIS_CLASS_MBUS_BITRATE.to_str(), 82)	//	used baud rates(bitmap)
				))
				, cyng::param_factory("ipt", cyng::vector_factory({
					cyng::tuple_factory(
						cyng::param_factory("host", "127.0.0.1"),
						cyng::param_factory("service", "26862"),
						cyng::param_factory("account", "gateway"),
						cyng::param_factory("pwd", "to-define"),
						cyng::param_factory("def-sk", "0102030405060708090001020304050607080900010203040506070809000001"),	//	scramble key
						cyng::param_factory("scrambled", true),
						cyng::param_factory("monitor", rnd_monitor())),	//	seconds
					cyng::tuple_factory(
						cyng::param_factory("host", "127.0.0.1"),
						cyng::param_factory("service", "26863"),
						cyng::param_factory("account", "gateway"),
						cyng::param_factory("pwd", "to-define"),
						cyng::param_factory("def-sk", "0102030405060708090001020304050607080900010203040506070809000001"),	//	scramble key
						cyng::param_factory("scrambled", false),
						cyng::param_factory("monitor", rnd_monitor()))
					}))

				//	built-in meter
				, cyng::param_factory("virtual-meter", cyng::tuple_factory(
					cyng::param_factory("enabled", false),
					cyng::param_factory("active", true),
					cyng::param_factory("server", "01-d81c-10000001-3c-02"),	//	1CD8
					cyng::param_factory("interval", 26000)
				))
			)
		});
	}

	bool controller::start(cyng::async::mux& mux
		, cyng::logging::log_ptr logger
		, cyng::reader<cyng::object> const& cfg
		, boost::uuids::uuid tag)
	{
		//
		//  control push data logging
		//
		auto const log_pushdata = cyng::value_cast(cfg.get("log-pushdata"), false);
		boost::ignore_unused(log_pushdata);

		//
		//	SML login
		//
		auto const accept_all = cyng::value_cast(cfg.get("accept-all-ids"), false);
		if (accept_all) {
			CYNG_LOG_WARNING(logger, "Accepts all server IDs");
		}

		//
		//	map all available GPIO paths
		//
		auto const gpio_path = cyng::value_cast<std::string>(cfg.get("gpio-path"), "/sys/class/gpio");
		CYNG_LOG_INFO(logger, "gpio path: " << gpio_path);

		auto const gpio_list = cyng::vector_cast<int>(cfg.get("gpio-list"), 0);
		std::map<int, std::string> gpio_paths;
		for (auto const gpio : gpio_list) {
			std::stringstream ss;
			ss
				<< gpio_path
				<< "/gpio"
				<< gpio
				;
			auto const p = ss.str();
			CYNG_LOG_TRACE(logger, "gpio: " << p);
			gpio_paths.emplace(gpio, p);
		}

		if (gpio_paths.size() != 4) {
			CYNG_LOG_WARNING(logger, "invalid count of gpios: " << gpio_paths.size());
		}
		

		//
		//	get configuration type
		//
		auto const config_types = cyng::vector_cast<std::string>(cfg.get("output"), "");

		//
		//	get IP-T configuration
		//
		cyng::vector_t vec;
		vec = cyng::value_cast(cfg.get("ipt"), vec);
		auto cfg_ipt = ipt::load_cluster_cfg(vec);

		//
		//	get wireless-LMN configuration
		//
		cyng::tuple_t cfg_wireless_lmn;
		cfg_wireless_lmn = cyng::value_cast(cfg.get("wireless-LMN"), cfg_wireless_lmn);

		//
		//	get wired-LMN configuration
		//
		cyng::tuple_t cfg_wired_lmn;
		cfg_wired_lmn = cyng::value_cast(cfg.get("wired-LMN"), cfg_wired_lmn);

		//
		//	get virtual meter
		//
		cyng::tuple_t cfg_virtual_meter;
		cfg_virtual_meter = cyng::value_cast(cfg.get("virtual-meter"), cfg_virtual_meter);

		/**
		 * global data cache
		 */
		cyng::store::db config_db;
		//init_config(logger
		//	, config_db
		//	, tag
		//	, cfg);

		//
		//	connect to ipt master
		//
		cyng::vector_t tmp;
		//join_network(mux
		//	, logger
		//	, config_db
		//	, tag
		//	, cfg_ipt
		//	, cfg_wireless_lmn
		//	, cfg_wired_lmn
		//	, cfg_virtual_meter
		//	, cyng::value_cast<std::string>(cfg["server"].get("account"), "")
		//	, cyng::value_cast<std::string>(cfg["server"].get("pwd"), "")
		//	, accept_all
		//	, gpio_paths);

		//
		//	create server
		//
		//server srv(mux
		//	, logger
		//	, config_db
		//	, tag
		//	, cfg_ipt
		//	, cyng::value_cast<std::string>(cfg["server"].get("account"), "")
		//	, cyng::value_cast<std::string>(cfg["server"].get("pwd"), "")
		//	, accept_all);

		//
		//	server runtime configuration
		//
		const auto address = cyng::io::to_str(cfg["server"].get("address"));
		const auto service = cyng::io::to_str(cfg["server"].get("service"));

		CYNG_LOG_INFO(logger, "listener address: " << address);
		CYNG_LOG_INFO(logger, "listener service: " << service);
		//srv.run(address, service);

		//
		//	wait for system signals
		//
		bool const shutdown = wait(logger);

		//
		//	close acceptor
		//
		CYNG_LOG_INFO(logger, "close acceptor");
		//srv.close();

		return shutdown;
	}

	int controller::prepare_config_db(cyng::param_map_t&& cfg)
	{
		auto con_type = cyng::db::get_connection_type(cyng::value_cast<std::string>(cfg["type"], "SQLite"));
		cyng::db::session s(con_type);
		auto r = s.connect(cfg);
		if (r.second) {

			//
			//	connect string
			//
#ifdef _DEBUG
			std::cout 
				<< "connect string: [" 
				<< r.first 
				<< "]"
				<< std::endl
				;
#endif

			auto meta_db = create_db_meta_data();
			for (auto tbl : meta_db)
			{
#ifdef _DEBUG
				std::cout
					<< "create table  : ["
					<< tbl->get_name()
					<< "]"
					<< std::endl
					;
#endif
				cyng::sql::command cmd(tbl, s.get_dialect());
				cmd.create();
				std::string sql = cmd.to_str();
#ifdef _DEBUG
				std::cout 
					<< sql 
					<< std::endl;
#endif
				s.execute(sql);

				return EXIT_SUCCESS;
			}
		}
		else {
			std::cerr
				<< "connect ["
				<< r.first
				<< "] failed"
				<< std::endl
				;
		}
		return EXIT_FAILURE;
	}

	int controller::transfer_config()
	{
		//
		//	read configuration file
		//
		auto const r = read_config_section(json_path_, config_index_);
		if (r.second) {

			//
			//	get a DOM reader
			//
			auto const dom = cyng::make_reader(r.first);

			//
			//	get database configuration and connect
			//
			cyng::tuple_t tpl;
			tpl = cyng::value_cast(dom.get("DB"), tpl);
			auto db_cfg = cyng::to_param_map(tpl);

			auto con_type = cyng::db::get_connection_type(cyng::value_cast<std::string>(db_cfg["type"], "SQLite"));
			cyng::db::session s(con_type);
			auto r = s.connect(db_cfg);
			if (r.second) {

				//
				//	IP-T configuration
				//
				cyng::vector_t vec;
				vec = cyng::value_cast(dom.get("ipt"), vec);
				auto const cfg_ipt = ipt::load_cluster_cfg(vec);
				for (auto const rec : cfg_ipt) {

				}
			}
			else {
				std::cerr
					<< "connect ["
					<< r.first
					<< "] failed"
					<< std::endl
					;
			}
			return EXIT_SUCCESS;
		}
		else
		{
			std::cout
				<< "configuration file ["
				<< json_path_
				<< "] not found or index ["
				<< config_index_
				<< "] is out of range"
				<< std::endl;
		}
		return EXIT_FAILURE;
	}

}
