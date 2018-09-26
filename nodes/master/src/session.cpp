/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 


#include "session.h"
#include "client.h"
#include "db.h"
#include "tasks/watchdog.h"
#include <NODE_project_info.h>
#include <smf/cluster/generator.h>
#include <smf/ipt/response.hpp>
#include <cyng/vm/domain/log_domain.h>
#include <cyng/vm/domain/store_domain.h>
#include <cyng/io/serializer.h>
#include <cyng/value_cast.hpp>
#include <cyng/object_cast.hpp>
#include <cyng/tuple_cast.hpp>
#include <cyng/dom/reader.h>
#include <cyng/async/task/task_builder.hpp>
#include <cyng/parser/buffer_parser.h>

#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/predef.h>

namespace node 
{
	session::session(cyng::async::mux& mux
		, cyng::logging::log_ptr logger
		, boost::uuids::uuid mtag // master tag
		, cyng::store::db& db
		, std::string const& account
		, std::string const& pwd
		, boost::uuids::uuid stag
		, std::chrono::seconds monitor
		, std::atomic<std::uint64_t>& global_configuration
		, boost::filesystem::path stat_dir )
	: mux_(mux)
		, logger_(logger)
		, mtag_(mtag)
		, db_(db)
		, vm_(mux.get_io_service(), stag)
		, parser_([this](cyng::vector_t&& prg) {
			CYNG_LOG_TRACE(logger_, prg.size() 
				<< " instructions received (including "
				<< cyng::op_counter(prg, cyng::code::INVOKE)
				<< " invoke(s))");
			CYNG_LOG_DEBUG(logger_, "exec: " << cyng::io::to_str(prg));
			vm_.async_run(std::move(prg));
		})
		, account_(account)
		, pwd_(pwd)
		, cluster_monitor_(monitor)
		, seq_(0)
		, client_(mux, logger, db, global_configuration, stat_dir)
		, subscriptions_()
		, tsk_watchdog_(cyng::async::NO_TASK)
		, group_(0)
	{
		//
		//	register logger domain
		//
		cyng::register_logger(logger_, vm_);
		vm_.async_run(cyng::generate_invoke("log.msg.info", "log domain is running"));

		//
		//	increase sequence and set as result value
		//
		vm_.register_function("bus.seq.next", 0, [this](cyng::context& ctx) {
			++this->seq_;
			ctx.push(cyng::make_object(this->seq_));
		});

		//
		//	push last bus sequence number on stack
		//
		vm_.register_function("bus.seq.push", 0, [this](cyng::context& ctx) {
			ctx.push(cyng::make_object(this->seq_));
		});


		vm_.register_function("bus.res.watchdog", 3, std::bind(&session::res_watchdog, this, std::placeholders::_1));

		//
		//	session shutdown - initiated by connection
		//
		vm_.register_function("session.cleanup", 2, std::bind(&session::cleanup, this, std::placeholders::_1));

		//
		//	write statistics
		//
		//vm_.register_function("session.write.stat", 5, std::bind(&client::write_statistics, &client_, std::placeholders::_1));

		//
		//	register request handler
		//
		vm_.register_function("bus.req.login", 11, std::bind(&session::bus_req_login, this, std::placeholders::_1));
		vm_.register_function("bus.req.stop.client", 3, std::bind(&session::bus_req_stop_client_impl, this, std::placeholders::_1));
		vm_.register_function("bus.req.reboot.client", 3, std::bind(&session::bus_req_reboot_client, this, std::placeholders::_1));
		vm_.register_function("bus.insert.msg", 2, std::bind(&session::bus_insert_msg, this, std::placeholders::_1));
		vm_.register_function("bus.req.push.data", 7, std::bind(&session::bus_req_push_data, this, std::placeholders::_1));
		//vm_.register_function("bus.store.req.connection.close", 3, std::bind(&session::bus_store_req_connection_close, this, std::placeholders::_1));

		vm_.register_function("client.req.login", 8, std::bind(&session::client_req_login, this, std::placeholders::_1));
		vm_.register_function("client.req.close", 4, std::bind(&session::client_req_close, this, std::placeholders::_1));
		vm_.register_function("client.res.close", 4, std::bind(&session::client_res_close, this, std::placeholders::_1));
		vm_.register_function("client.req.open.push.channel", 10, std::bind(&session::client_req_open_push_channel, this, std::placeholders::_1));
		vm_.register_function("client.res.open.push.channel", 9, std::bind(&session::client_res_open_push_channel, this, std::placeholders::_1));
		vm_.register_function("client.req.close.push.channel", 5, std::bind(&session::client_req_close_push_channel, this, std::placeholders::_1));
		vm_.register_function("client.req.register.push.target", 5, std::bind(&session::client_req_register_push_target, this, std::placeholders::_1));
		vm_.register_function("client.req.deregister.push.target", 5, std::bind(&session::client_req_deregister_push_target, this, std::placeholders::_1));
		vm_.register_function("client.req.open.connection", 6, std::bind(&session::client_req_open_connection, this, std::placeholders::_1));
		vm_.register_function("client.res.open.connection", 4, std::bind(&session::client_res_open_connection, this, std::placeholders::_1));
		vm_.register_function("client.req.close.connection", 5, std::bind(&session::client_req_close_connection, this, std::placeholders::_1));
		vm_.register_function("client.res.close.connection", 6, std::bind(&session::client_res_close_connection, this, std::placeholders::_1));
		vm_.register_function("client.req.transfer.pushdata", 6, std::bind(&session::client_req_transfer_pushdata, this, std::placeholders::_1));
		

		vm_.register_function("client.req.transmit.data", 5, std::bind(&session::client_req_transmit_data, this, std::placeholders::_1));
		vm_.register_function("client.inc.throughput", 3, std::bind(&session::client_inc_throughput, this, std::placeholders::_1));

		vm_.register_function("client.update.attr", 6, std::bind(&session::client_update_attr, this, std::placeholders::_1));
	}


	std::size_t session::hash() const noexcept
	{
		return vm_.hash();
	}

	void session::stop(cyng::object obj)
	{
		vm_.access(std::bind(&session::stop_cb, this, std::placeholders::_1, obj));
	}

	void session::stop_cb(cyng::vm& vm, cyng::object obj)
	{
		//
		//	object reference is needed to make sure that VM is in valid state.
		//
		vm.run(cyng::generate_invoke("log.msg.info", "fast shutdown"));
		vm.run(cyng::generate_invoke("ip.tcp.socket.shutdown"));
		vm.run(cyng::generate_invoke("ip.tcp.socket.close"));
		vm.run(cyng::vector_t{ cyng::make_object(cyng::code::HALT) });
	}

	void session::cleanup(cyng::context& ctx)
	{
		BOOST_ASSERT(this->vm_.tag() == ctx.tag());

		//
		//	session is being closed.
		//	no further VM calls allowed!
		//

		//	 [<!259:session>,asio.misc:2]
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_WARNING(logger_, "cluster member "
			<< ctx.tag()
			<< " lost");

		//
		//	stop watchdog task
		//
		if (tsk_watchdog_ != cyng::async::NO_TASK)
		{
			CYNG_LOG_INFO(logger_, "stop watchdog task #" << tsk_watchdog_);
			mux_.stop(tsk_watchdog_);
			tsk_watchdog_ = cyng::async::NO_TASK;
		}

		//
		//	remove affected targets
		//
		db_.access([&](cyng::store::table* tbl_target)->void {
			const auto count = cyng::erase(tbl_target, get_targets_by_peer(tbl_target, ctx.tag()), ctx.tag());
			CYNG_LOG_WARNING(logger_, "cluster member "
				<< ctx.tag()
				<< " removed "
				<< count
				<< " targets");
		}, cyng::store::write_access("_Target"));

		//
		//	remove affected channels.
		//	Channels are 2-way: There is an owner session and a target session. Both could be different.
		//
		db_.access([&](cyng::store::table* tbl_channel)->void {
			const auto count = cyng::erase(tbl_channel, get_channels_by_peer(tbl_channel, ctx.tag()), ctx.tag());
			CYNG_LOG_WARNING(logger_, "cluster member "
				<< ctx.tag()
				<< " removed "
				<< count
				<< " channels");
		}, cyng::store::write_access("*Channel"));

		//
		//	remove all open subscriptions
		//
		CYNG_LOG_INFO(logger_, "cluster member "
			<< ctx.tag()
			<< " removes "
			<< subscriptions_.size()
			<< " subscriptions");
		cyng::store::close_subscription(subscriptions_);
		
		//
		//	remove all client sessions of this node and close open connections
		//
		db_.access([&](cyng::store::table* tbl_session, cyng::store::table* tbl_connection, const cyng::store::table* tbl_device)->void {
			cyng::table::key_list_t pks;
			tbl_session->loop([&](cyng::table::record const& rec) -> bool {

				//
				//	build list with all affected session records
				//
				auto local_peer = cyng::object_cast<session>(rec["local"]);
				auto remote_peer = cyng::object_cast<session>(rec["remote"]);
				auto account = cyng::value_cast<std::string>(rec["name"], "");
				auto tag = cyng::value_cast(rec["tag"], boost::uuids::nil_uuid());

				//
				//	Check if the client belongs to this session.
				//
				if (local_peer && (local_peer->vm_.tag() == ctx.tag()))
				{
					pks.push_back(rec.key());
					if (remote_peer)
					{
						//	remote connection
						auto rtag = cyng::value_cast(rec["rtag"], boost::uuids::nil_uuid());

						//
						//	close connection
						//
						if (local_peer->hash() != remote_peer->hash())
						{

							ctx.attach(cyng::generate_invoke("log.msg.warning"
								, "close distinct connection to "
								, rtag));

							//	shutdown mode
							remote_peer->vm_.async_run(client_req_close_connection_forward(rtag
								, tag
								, 0u	//	no sequence number
								, true	//	shutdown mode
								, cyng::param_map_t()
								, cyng::param_map_factory("origin-tag", tag)("local-peer", ctx.tag())("local-connect", true)));
						}

						//
						//	remove from connection table
						//
						connection_erase(tbl_connection, cyng::table::key_generator(tag, rtag), tag);
					}

				}

				//
				//	generate statistics
				//	There is an error in ec serialization/deserialization
				//	
				auto ec = cyng::value_cast(frame.at(1), boost::system::error_code());
				client_.write_stat(tag, account, "shutdown node", ec.message());

				//	continue
				return true;
			});

			//
			//	remove all session records
			//
			//ctx.attach(cyng::generate_invoke("log.msg.info", "session.cleanup", pks.size()));
			cyng::erase(tbl_session, pks, ctx.tag());

		}	, cyng::store::write_access("_Session")
			, cyng::store::write_access("_Connection")
			, cyng::store::read_access("TDevice"));

		//
		//	cluster table
		//
		std::string node_class;
		db_.access([&](cyng::store::table* tbl_cluster)->void {

			//
			//	get node class
			//
			const auto key = cyng::table::key_generator(ctx.tag());
			const auto rec = tbl_cluster->lookup(key);
			if (!rec.empty())
			{
				node_class = cyng::value_cast<std::string>(rec["class"], "");
			}

			//
			//	remove from cluster table
			//
			tbl_cluster->erase(key, ctx.tag());

			//
			//	update master record
			//
			tbl_cluster->modify(cyng::table::key_generator(mtag_), cyng::param_factory("clients", tbl_cluster->size()), ctx.tag());
			

		} , cyng::store::write_access("_Cluster"));

		//
		//	emit a system message
		//
		std::stringstream ss;
		ss
			<< "cluster member "
			<< node_class
			<< ':'
			<< ctx.tag()
			<< " closed"
			;
		insert_msg(db_
			, cyng::logging::severity::LEVEL_WARNING
			, ss.str()
			, ctx.tag());

	}

	void session::bus_req_login(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		//CYNG_LOG_INFO(logger_, "bus.req.login " << cyng::io::to_str(frame));

		//	[root,X0kUj59N,46ca0590-e852-417d-a1c7-54ed17d9148f,setup,0.2,-1:00:0.000000,2018-03-19 10:01:36.40087970,false]
		//
		//	* account
		//	* password
		//	* tag
		//	* class
		//	* version
		//	* plattform (since v0.4)
		//	* timezone delta
		//	* timestamp
		//	* auto config allowed
		//	* group id
		//	* remote ep
		//

		//
		// To be compatible with version prior v0.4 we have first to detect the structure.
		// Prior v0.4 the first element was the account name and version was on index 4.
		//
		if (frame.at(0).get_class().tag() == cyng::TC_STRING
			&& frame.at(4).get_class().tag() == cyng::TC_VERSION)
		{
			//
			//	v0.3
			//
			auto const tpl = cyng::tuple_cast<
				std::string,			//	[0] account
				std::string,			//	[1] password
				boost::uuids::uuid,		//	[2] session tag
				std::string,			//	[3] class
				cyng::version,			//	[4] version
				std::chrono::minutes,	//	[5] delta
				std::chrono::system_clock::time_point,	//	[6] timestamp
				bool,					//	[7] autologin
				std::uint32_t,			//	[8] group
				boost::asio::ip::tcp::endpoint	//	[9] remote ep
			>(frame);

			BOOST_ASSERT_MSG(std::get<4>(tpl) == cyng::version(0, 3), "version 0.3 expected");

			CYNG_LOG_WARNING(logger_, "cluster member "
				<< std::get<0>(tpl)
				<< " login with version "
				<< std::get<4>(tpl));

			bus_req_login_impl(ctx
				, std::get<4>(tpl)
				, std::get<0>(tpl)
				, std::get<1>(tpl)
				, std::get<2>(tpl)
				, std::get<3>(tpl)
				, std::get<5>(tpl)
				, std::get<6>(tpl)
				, std::get<7>(tpl)
				, std::get<8>(tpl)
				, std::get<9>(tpl)
				, "unknown"
				, 0);
		}
		else
		{
			//
			//	v0.4 or higher
			//
			auto const tpl = cyng::tuple_cast<
				cyng::version,			//	[0] version
				std::string,			//	[1] account
				std::string,			//	[2] password
				boost::uuids::uuid,		//	[3] session tag
				std::string,			//	[4] class
				std::chrono::minutes,	//	[5] delta
				std::chrono::system_clock::time_point,	//	[6] timestamp
				bool,					//	[7] autologin
				std::uint32_t,			//	[8] group
				boost::asio::ip::tcp::endpoint,	//	[9] remote ep
				std::string,				//	[10] platform
				boost::process::pid_t		//	[11] process id
			>(frame);

			BOOST_ASSERT_MSG(std::get<0>(tpl) > cyng::version(0, 3), "version 0.4 or higher expected");

			bus_req_login_impl(ctx
				, std::get<0>(tpl)
				, std::get<1>(tpl)
				, std::get<2>(tpl)
				, std::get<3>(tpl)
				, std::get<4>(tpl)
				, std::get<5>(tpl)
				, std::get<6>(tpl)
				, std::get<7>(tpl)
				, std::get<8>(tpl)
				, std::get<9>(tpl)
				, std::get<10>(tpl)
				, std::get<11>(tpl));
		}
	}

	void session::bus_req_login_impl(cyng::context& ctx
		, cyng::version ver
		, std::string const& account
		, std::string const& pwd
		, boost::uuids::uuid tag
		, std::string const& node_class	
		, std::chrono::minutes delta
		, std::chrono::system_clock::time_point ts
		, bool autologin
		, std::uint32_t group
		, boost::asio::ip::tcp::endpoint ep
		, std::string platform
		, boost::process::pid_t pid)
	{
		if ((account == account_) && (pwd == pwd_))
		{
			CYNG_LOG_INFO(logger_, "cluster member "
				<< ctx.tag()
				<< " - "
				<< node_class
				<< '@'
				<< ep
				<< " successful authorized");

			//
			//	register additional request handler for database access
			//
			cyng::register_store(this->db_, ctx);

			//
			//	subscribe/unsubscribe
			//
			ctx.attach(cyng::register_function("bus.req.subscribe", 3, std::bind(&session::bus_req_subscribe, this, std::placeholders::_1)));
			ctx.attach(cyng::register_function("bus.req.unsubscribe", 2, std::bind(&session::bus_req_unsubscribe, this, std::placeholders::_1)));
			ctx.attach(cyng::register_function("bus.start.watchdog", 7, std::bind(&session::bus_start_watchdog, this, std::placeholders::_1)));
			ctx.attach(cyng::register_function("bus.req.query.srv.visible", 4, std::bind(&session::bus_req_query_srv_visible, this, std::placeholders::_1)));
			ctx.attach(cyng::register_function("bus.req.query.srv.active", 4, std::bind(&session::bus_req_query_srv_active, this, std::placeholders::_1)));
			ctx.attach(cyng::register_function("bus.req.query.firmware", 4, std::bind(&session::bus_req_query_firmware, this, std::placeholders::_1)));
			ctx.attach(cyng::register_function("bus.res.query.srv.visible", 8, std::bind(&session::bus_res_query_srv_visible, this, std::placeholders::_1)));
			ctx.attach(cyng::register_function("bus.res.query.srv.active", 8, std::bind(&session::bus_res_query_srv_active, this, std::placeholders::_1)));
			ctx.attach(cyng::register_function("bus.res.query.firmware", 8, std::bind(&session::bus_res_query_firmware, this, std::placeholders::_1)));

			//
			//	set node class and group id
			//
			group_ = group;
			client_.set_class(node_class);

			//
			//	insert into cluster table
			//
			std::chrono::microseconds ping = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - ts);
			ctx.attach(cyng::generate_invoke("bus.start.watchdog"
				, node_class
				, ts
				, ver
				, ping
				, cyng::invoke("push.session")
				, ep
				, pid));

			//
			//	send reply
			//
			ctx.attach(reply(ts, true));

		}
		else
		{
			CYNG_LOG_WARNING(logger_, "cluster member login failed: " 
				<< ctx.tag()
				<< " - "
				<< account
				<< ':'
				<< pwd);

			//
			//	send reply
			//
			ctx.attach(reply(ts, false));

			//
			//	emit system message
			//
			std::stringstream ss;
			ss
				<< "cluster member login failed: " 
				<< node_class
				<< '@'
				<< ep
				;
			insert_msg(db_
				, cyng::logging::severity::LEVEL_ERROR
				, ss.str()
				, ctx.tag());

		}
	}

	void session::bus_req_stop_client_impl(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.req.stop.client " << cyng::io::to_str(frame));

		//	[2,[1ca8529b-9b95-4a3a-ba2f-26c7280aa878],ac70b95e-76b9-463a-a961-bb02f70e86c8]
		//
		//	* bus sequence
		//	* session key
		//	* source

		auto const tpl = cyng::tuple_cast<
			std::uint64_t,			//	[0] sequence
			cyng::vector_t,			//	[1] session key
			boost::uuids::uuid		//	[2] source
		>(frame);

		//	stop a client session
		//
		db_.access([&](const cyng::store::table* tbl_session)->void {
			cyng::table::record rec = tbl_session->lookup(std::get<1>(tpl));
			if (!rec.empty())
			{
				auto peer = cyng::object_cast<session>(rec["local"]);
				BOOST_ASSERT(peer->vm_.tag() != ctx.tag());
				if (peer && (peer->vm_.tag() != ctx.tag()) && (std::get<1>(tpl).size() == 1))
				{ 
					//
					//	node will send a client close response
					//
					auto tag = cyng::value_cast(std::get<1>(tpl).at(0), boost::uuids::nil_uuid());
					peer->vm_.async_run(node::client_req_close(tag, 0));
				}
			}
			else
			{
				CYNG_LOG_WARNING(logger_, "bus.req.stop.client not found " << cyng::io::to_str(frame));

			}
		}, cyng::store::read_access("_Session"));
	}

	std::tuple<session const*, cyng::table::record, cyng::buffer_t, boost::uuids::uuid> session::find_peer(cyng::table::key_type const& key_session
		, const cyng::store::table* tbl_session
		, const cyng::store::table* tbl_gw)
	{
		auto rec_gw = tbl_gw->lookup(key_session);
		auto rec_session = tbl_session->find_first(cyng::param_t("device", key_session.at(0)));
		if (!rec_gw.empty() && !rec_session.empty())
		{
			auto peer = cyng::object_cast<session>(rec_session["local"]);
			if (peer && (key_session.size() == 1))
			{
				//
				//	get remote session tag
				//
				auto tag = cyng::value_cast(rec_session.key().at(0), boost::uuids::nil_uuid());
				//BOOST_ASSERT(tag == peer->vm_.tag());

				//
				//	get login parameters
				//
				const auto server = cyng::value_cast<std::string>(rec_gw["serverId"], "05000000000000");
				const auto name = cyng::value_cast<std::string>(rec_gw["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(rec_gw["userPwd"], "");

				const auto id = cyng::parse_hex_string(server);
				if (id.second)
				{
					CYNG_LOG_TRACE(logger_, "gateway "
						<< cyng::io::to_str(rec_session["name"])
						<< ": "
						<< server
						<< ", user: "
						<< name
						<< ", pwd: "
						<< pwd
						<< " has peer: "
						<< peer->vm_.tag());

					return std::make_tuple(peer, rec_gw, id.first, tag);
				}
				else
				{
					CYNG_LOG_WARNING(logger_, "client "
						<< tag
						<< " has an invalid server id: "
						<< server);
				}
			}
		}

		//
		//	not found - peer is a null pointer
		//
		return std::make_tuple(nullptr, rec_gw, cyng::buffer_t{}, boost::uuids::nil_uuid());
	}

	void session::bus_req_reboot_client(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.req.reboot.client " << cyng::io::to_str(frame));

		//	bus.req.reboot.client not found 
		//	[1,[dca135f3-ff2b-4bf7-8371-a9904c74522b],430681d9-a735-47be-a26c-c9cac94d6729]
		//
		//	* bus sequence
		//	* session key
		//	* source

		auto const tpl = cyng::tuple_cast<
			std::uint64_t,			//	[0] sequence
			cyng::table::key_type,	//	[1] session key
			boost::uuids::uuid		//	[2] source
		>(frame);

		//
		//	test session key
		//
		BOOST_ASSERT(!std::get<1>(tpl).empty());

		//	reboot a gateway (client)
		//
		db_.access([&](const cyng::store::table* tbl_session, const cyng::store::table* tbl_gw)->void {

#if _MSVC || (__GNUC__ > 6)
			//
			//	C++17 feature: structured binding
			//
			//	peer - 
			//	tag - target session tag
			auto [peer, rec, server, tag] = find_peer(std::get<1>(tpl), tbl_session, tbl_gw);
			if (peer != nullptr) {
				const auto name = cyng::value_cast<std::string>(rec["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(rec["userPwd"], "");
				peer->vm_.async_run(client_req_reboot(std::get<2>(tpl), server, name, pwd));
			}
#else
			auto res = find_peer(std::get<1>(tpl), tbl_session, tbl_gw);
			if (std::get<0>(res) != nullptr) {
				const auto name = cyng::value_cast<std::string>(std::get<1>(res)["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(std::get<1>(res)["userPwd"], "");
				std::get<0>(res)->vm_.async_run(client_req_reboot(std::get<2>(tpl), std::get<2>(res), name, pwd));
			}
#endif


		}	, cyng::store::read_access("_Session")
			, cyng::store::read_access("TGateway"));
	}

	void session::bus_req_query_srv_visible(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.req.query.srv.visible " << cyng::io::to_str(frame));

		//	[6,[a584c1cc-7ae1-455b-99d1-de0f9483520d],a49574ae-2fc1-445c-8746-652cc4fb760c]
		//
		//	* bus sequence
		//	* session key
		//	* source
		//	* tag_ws

		auto const tpl = cyng::tuple_cast<
			std::uint64_t,			//	[0] sequence
			cyng::vector_t,			//	[1] session key
			boost::uuids::uuid,		//	[2] source
			boost::uuids::uuid		//	[3] websocket tag
		>(frame);

		//
		//	test session key
		//
		BOOST_ASSERT(!std::get<1>(tpl).empty());

		//	reboot a gateway (client)
		//
		db_.access([&](const cyng::store::table* tbl_session, const cyng::store::table* tbl_gw)->void {

#if _MSVC || (__GNUC__ > 6)
			
			//
			//	C++17 feature
			//
			auto[peer, rec, server, tag] = find_peer(std::get<1>(tpl), tbl_session, tbl_gw);

			if (peer != nullptr) {
				const auto name = cyng::value_cast<std::string>(rec["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(rec["userPwd"], "");
				peer->vm_.async_run(node::client_req_query_srv_visible(tag	//	(ipt) session tag
					, ctx.tag()			//	source peer
					, std::get<0>(tpl)	//	cluster sequence
					, std::get<3>(tpl)	//	ws tag
					, server
					, name
					, pwd));
			}
#else
		auto res = find_peer(std::get<1>(tpl), tbl_session, tbl_gw);
		if (std::get<0>(res) != nullptr) {
			
			const auto name = cyng::value_cast<std::string>(std::get<1>(res)["userName"], "");
			const auto pwd = cyng::value_cast<std::string>(std::get<1>(res)["userPwd"], "");
			
			std::get<0>(res)->vm_.async_run(node::client_req_query_srv_visible(std::get<3>(res)	//	(ipt) session tag
			, ctx.tag()			//	source peer
			, std::get<0>(tpl)	//	cluster sequence
			, std::get<3>(tpl)	//	ws tag
			, std::get<2>(res)
			, name
			, pwd));
		}
#endif

		}	, cyng::store::read_access("_Session")
			, cyng::store::read_access("TGateway"));
	}

	void session::bus_req_query_srv_active(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.req.query.srv.active " << cyng::io::to_str(frame));

		//	[6,[a584c1cc-7ae1-455b-99d1-de0f9483520d],a49574ae-2fc1-445c-8746-652cc4fb760c]
		//
		//	* bus sequence
		//	* session key
		//	* source
		//	* tag_ws

		auto const tpl = cyng::tuple_cast<
			std::uint64_t,			//	[0] sequence
			cyng::vector_t,			//	[1] session key
			boost::uuids::uuid,		//	[2] source
			boost::uuids::uuid		//	[3] websocket tag
		>(frame);

		//
		//	test session key
		//
		BOOST_ASSERT(!std::get<1>(tpl).empty());

		//	reboot a gateway (client)
		//
		db_.access([&](const cyng::store::table* tbl_session, const cyng::store::table* tbl_gw)->void {

#if _MSVC || (__GNUC__ > 6)
			
			//
			//	C++17 feature
			//
			auto[peer, rec, server, tag] = find_peer(std::get<1>(tpl), tbl_session, tbl_gw);
			if (peer != nullptr) {
				const auto name = cyng::value_cast<std::string>(rec["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(rec["userPwd"], "");
				peer->vm_.async_run(node::client_req_query_srv_active(tag
				, ctx.tag()			//	source peer
				, std::get<0>(tpl)	//	cluster sequence
				, std::get<3>(tpl)	//	ws tag
				, server
				, name
				, pwd));
#else
			auto res = find_peer(std::get<1>(tpl), tbl_session, tbl_gw);
			if (std::get<0>(res) != nullptr) {
				const auto name = cyng::value_cast<std::string>(std::get<1>(res)["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(std::get<1>(res)["userPwd"], "");
				std::get<0>(res)->vm_.async_run(node::client_req_query_srv_active(std::get<3>(res)
				, ctx.tag()			//	source peer
				, std::get<0>(tpl)	//	cluster sequence
				, std::get<3>(tpl)	//	ws tag
				, std::get<2>(res)
				, name
				, pwd));
				
#endif
			
			}

		}	, cyng::store::read_access("_Session")
			, cyng::store::read_access("TGateway"));
	}

	void session::bus_req_query_firmware(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.req.query.firmware " << cyng::io::to_str(frame));

		//	[6,[a584c1cc-7ae1-455b-99d1-de0f9483520d],a49574ae-2fc1-445c-8746-652cc4fb760c]
		//
		//	* bus sequence
		//	* session key
		//	* source
		//	* tag_ws

		auto const tpl = cyng::tuple_cast<
			std::uint64_t,			//	[0] sequence
			cyng::vector_t,			//	[1] session key
			boost::uuids::uuid,		//	[2] source
			boost::uuids::uuid		//	[3] websocket tag
		>(frame);

		//
		//	test session key
		//
		BOOST_ASSERT(!std::get<1>(tpl).empty());

		//	reboot a gateway (client)
		//
		db_.access([&](const cyng::store::table* tbl_session, const cyng::store::table* tbl_gw)->void {

#if _MSVC || (__GNUC__ > 6)			
			//
			//	C++17 feature
			//
			auto[peer, rec, server, tag] = find_peer(std::get<1>(tpl), tbl_session, tbl_gw);
			if (peer != nullptr) {
				const auto name = cyng::value_cast<std::string>(rec["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(rec["userPwd"], "");
				peer->vm_.async_run(node::client_req_query_firmware(tag
				, ctx.tag()			//	source peer
				, std::get<0>(tpl)	//	cluster sequence
				, std::get<3>(tpl)	//	ws tag
				, server
				, name
				, pwd));
			}
#else
			auto res = find_peer(std::get<1>(tpl), tbl_session, tbl_gw);
			if (std::get<0>(res) != nullptr) {
				const auto name = cyng::value_cast<std::string>(std::get<1>(res)["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(std::get<1>(res)["userPwd"], "");
				std::get<0>(res)->vm_.async_run(node::client_req_query_firmware(std::get<3>(res)
				, ctx.tag()			//	source peer
				, std::get<0>(tpl)	//	cluster sequence
				, std::get<3>(tpl)	//	ws tag
				, std::get<2>(res)
				, name
				, pwd));
			}
#endif
			

		}	, cyng::store::read_access("_Session")
			, cyng::store::read_access("TGateway"));
	}

	void session::bus_res_query_srv_visible(cyng::context& ctx)
	{
		//	[6b9c001a-4390-40b5-9abc-25c6180cc8b9,21,00:15:3b:02:29:7e,0005,01-e61e-29436587-bf-03,---,2018-08-29 19:26:29.00000000]
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.res.query.srv.visible " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] source
			std::uint64_t,			//	[1] sequence
			boost::uuids::uuid,		//	[2] websocket tag
			std::uint16_t,			//	[3] list number
			std::string,			//	[4] server id (key)
			std::string,			//	[5] meter
			std::string,			//	[6] device class
			std::chrono::system_clock::time_point,	//	[7] last status update
			std::uint32_t			//	[8] server type
		>(frame);


		db_.access([&](const cyng::store::table* tbl_cluster)->void {

			auto rec = tbl_cluster->lookup(cyng::table::key_generator(std::get<0>(tpl)));
			if (!rec.empty()) {
				auto peer = cyng::object_cast<session>(rec["self"]);
				if (peer != nullptr) {

					CYNG_LOG_INFO(logger_, "bus.res.query.srv.visible - send to peer " 
						<< cyng::value_cast<std::string>(rec["class"], "")
						<< '@'
						<< peer->vm_.tag());

					//
					//	forward data
					//
					peer->vm_.async_run(node::bus_res_query_srv_visible(std::get<0>(tpl)
						, std::get<1>(tpl)		//	sequence
						, std::get<2>(tpl)		//	websocket tag
						, std::get<3>(tpl)		//	list number
						, std::get<4>(tpl)		//	server id
						, std::get<5>(tpl)		//	meter
						, std::get<6>(tpl)		//	device class
						, std::get<7>(tpl)		//	last status update
						, std::get<8>(tpl)));	
				}
			}
			else {
				CYNG_LOG_WARNING(logger_, "bus.res.query.srv.visible - peer not found " << std::get<0>(tpl));
			}
		}, cyng::store::read_access("_Cluster"));

	}

	void session::bus_res_query_srv_active(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.res.query.srv.active " << cyng::io::to_str(frame));
		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] source
			std::uint64_t,			//	[1] sequence
			boost::uuids::uuid,		//	[2] websocket tag
			std::uint16_t,			//	[3] list number
			std::string,			//	[4] server id (key)
			std::string,			//	[5] meter
			std::string,			//	[6] device class
			std::chrono::system_clock::time_point,	//	[7] last status update
			std::uint32_t			//	[8] server type
		>(frame);


		db_.access([&](const cyng::store::table* tbl_cluster)->void {
			auto rec = tbl_cluster->lookup(cyng::table::key_generator(std::get<0>(tpl)));
			if (!rec.empty()) {
				auto peer = cyng::object_cast<session>(rec["self"]);
				if (peer != nullptr) {

					CYNG_LOG_INFO(logger_, "bus.res.query.srv.active - send to peer "
						<< cyng::value_cast<std::string>(rec["class"], "")
						<< '@'
						<< peer->vm_.tag());

					//
					//	forward data
					//
					peer->vm_.async_run(node::bus_res_query_srv_active(std::get<0>(tpl)
						, std::get<1>(tpl)	//	sequence
						, std::get<2>(tpl)		//	websocket tag
						, std::get<3>(tpl)		//	list number
						, std::get<4>(tpl)		//	server id
						, std::get<5>(tpl)		//	meter
						, std::get<6>(tpl)		//	device class
						, std::get<7>(tpl)		//	last status update
						, std::get<8>(tpl)));
				}
			}
			else {
				CYNG_LOG_WARNING(logger_, "bus.res.query.srv.active - peer not found " << std::get<0>(tpl));
			}
		}, cyng::store::read_access("_Cluster"));
	}

	void session::bus_res_query_firmware(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.res.query.firmware " << cyng::io::to_str(frame));
		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] source
			std::uint64_t,			//	[1] sequence
			boost::uuids::uuid,		//	[2] websocket tag
			std::uint32_t,			//	[3] list number
			std::string,			//	[4] server id (key)
			std::string,			//	[5] section
			std::string,			//	[6] version
			bool					//	[7] active/inactive
		>(frame);


		db_.access([&](const cyng::store::table* tbl_cluster)->void {
			auto rec = tbl_cluster->lookup(cyng::table::key_generator(std::get<0>(tpl)));
			if (!rec.empty()) {
				auto peer = cyng::object_cast<session>(rec["self"]);
				if (peer != nullptr) {

					CYNG_LOG_INFO(logger_, "bus.res.query.firmware - send to peer "
						<< cyng::value_cast<std::string>(rec["class"], "")
						<< '@'
						<< peer->vm_.tag()
						<< " - "
						<< std::get<6>(tpl));

					//
					//	forward data
					//
					peer->vm_.async_run(node::bus_res_query_firmware(std::get<0>(tpl)
						, std::get<1>(tpl)	//	sequence
						, std::get<2>(tpl)		//	websocket tag
						, std::get<3>(tpl)		//	list number
						, std::get<4>(tpl)		//	server id
						, std::get<5>(tpl)		//	section
						, std::get<6>(tpl)		//	version
						, std::get<7>(tpl)));	//	active/inactive
				}
			}
			else {
				CYNG_LOG_WARNING(logger_, "bus.res.query.srv.active - peer not found " << std::get<0>(tpl));
			}
		}, cyng::store::read_access("_Cluster"));
	}

	void session::bus_start_watchdog(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.start.watchdog " << cyng::io::to_str(frame));

		//	[setup,2018-05-02 09:55:02.49315500,0.4,00:00:0.034161,<!259:session>,127.0.0.1:53116,10984]
		//
		//	* class
		//	* login
		//	* version
		//	* ping
		//	* session object
		//	* ep
		//	* pid
		
		//
		//	insert into cluster table
		//
		//
		//	cluster table
		//
		db_.access([&](cyng::store::table* tbl_cluster)->void {

			//
			//	insert into cluster table
			//
			if (!tbl_cluster->insert(cyng::table::key_generator(ctx.tag())
				, cyng::table::data_generator(frame.at(0), frame.at(1), frame.at(2), 0u, frame.at(3), frame.at(5), frame.at(6), frame.at(4))
				, 0u, ctx.tag()))
			{
				CYNG_LOG_ERROR(logger_, "bus.start.watchdog - Cluster table insert failed" << cyng::io::to_str(frame));

			}

			//
			//	update master record
			//
			tbl_cluster->modify(cyng::table::key_generator(mtag_), cyng::param_factory("clients", tbl_cluster->size()), ctx.tag());


		}, cyng::store::write_access("_Cluster"));

		//
		//	build message strings
		//
		std::stringstream ss;

		//
		//	start watchdog task
		//
		if (cluster_monitor_.count() > 5)
		{
			tsk_watchdog_ = cyng::async::start_task_delayed<watchdog>(mux_, std::chrono::seconds(30)
				, logger_
				, db_
				, frame.at(4)
				, cluster_monitor_).first;

			ss
				<< "start watchdog task #"
				<< tsk_watchdog_
				<< " for cluster member "
				<< cyng::value_cast<std::string>(frame.at(0), "")
				<< ':'
				<< ctx.tag()
				<< " with "
				<< cluster_monitor_.count()
				<< " seconds"
				;
			insert_msg(db_
				, cyng::logging::severity::LEVEL_INFO
				, ss.str()
				, ctx.tag());

		}
		else
		{
			ss
				<< "do not start watchdog task for cluster member "
				<< cyng::value_cast<std::string>(frame.at(0), "")
				<< ':'
				<< ctx.tag()
				<< " - watchdog timer is "
				<< cluster_monitor_.count()
				<< " second(s)"
				;
			insert_msg(db_
				, cyng::logging::severity::LEVEL_WARNING
				, ss.str()
				, ctx.tag());
		}

		//
		//	print system message
		//	cluster member modem:c97ff026-768d-4c71-b9bc-7bb2ad186c34 joined
		//
		ss.str("");
		ss
			<< "cluster member "
			<< cyng::value_cast<std::string>(frame.at(0), "")
			<< ':'
			<< ctx.tag()
			<< " joined"
			;
		insert_msg(db_
			, cyng::logging::severity::LEVEL_INFO
			, ss.str()
			, ctx.tag());


	}

	void session::res_watchdog(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		//CYNG_LOG_INFO(logger_, "bus.res.watchdog " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] session tag
			std::uint64_t,			//	[1] sequence
			std::chrono::system_clock::time_point	//	[2] timestamp
		>(frame);

		//
		//	calculate ping time
		//
		auto ping = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - std::get<2>(tpl));
		CYNG_LOG_INFO(logger_, "bus.res.watchdog - ping time " << ping.count() << " microsec");

		db_.modify("_Cluster"
			, cyng::table::key_generator(ctx.tag())
			, cyng::param_factory("ping", ping)
			, ctx.tag());
	}


	void session::bus_req_subscribe(cyng::context& ctx)
	{
		//
		//	[TDevice,cfa27fa4-3164-4d2a-80cc-504541c673db,3]
		//
		//	* table to subscribe
		//	* remote session id
		//	* optional task id (to redistribute by receiver)
		//	
		const cyng::vector_t frame = ctx.get_frame();

		auto const tpl = cyng::tuple_cast<
			std::string,			//	[0] table name
			boost::uuids::uuid,		//	[1] remote session tag
			std::size_t				//	[2] task id
		>(frame);

		ctx.attach(cyng::generate_invoke("log.msg.info", "bus.req.subscribe", std::get<0>(tpl), std::get<1>(tpl)));

		db_.access([&](cyng::store::table const* tbl)->void {

			CYNG_LOG_INFO(logger_, tbl->meta().get_name() << "->size(" << tbl->size() << ")");
			tbl->loop([&](cyng::table::record const& rec) -> bool {

				ctx.run(bus_res_subscribe(tbl->meta().get_name()
					, rec.key()
					, rec.data()
					, rec.get_generation()
					, std::get<1>(tpl)		//	session tag
					, std::get<2>(tpl)));	//	task id (optional)

				//	continue loop
				return true;
			});

		}, cyng::store::read_access(std::get<0>(tpl)));

		db_.access([&](cyng::store::table* tbl)->void {
			//
			//	One set of callbacks for multiple tables.
			//	Store connections array for clean disconnect
			//
			cyng::store::add_subscription(subscriptions_
				, std::get<0>(tpl)
				, tbl->get_listener(std::bind(&session::sig_ins, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)
					, std::bind(&session::sig_del, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
					, std::bind(&session::sig_clr, this, std::placeholders::_1, std::placeholders::_2)
					, std::bind(&session::sig_mod, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)));
		}, cyng::store::write_access(std::get<0>(tpl)));

	}

	void session::sig_ins(cyng::store::table const* tbl
		, cyng::table::key_type const& key
		, cyng::table::data_type const& data
		, std::uint64_t gen
		, boost::uuids::uuid source)
	{
#ifdef _DEBUG
		if (boost::algorithm::equals(tbl->meta().get_name(), "TDevice"))
		{
			vm_.async_run(cyng::generate_invoke("log.msg.debug"	
				, "sig.ins"
				, source
				, tbl->meta().get_name()
				, ((source != vm_.tag()) ? "req" : "res")
				, key
				, data
			));
		}
		else
		{
			vm_.async_run(cyng::generate_invoke("log.msg.debug"
				, "sig.ins"
				, source
				, tbl->meta().get_name()
				, ((source != vm_.tag()) ? "req" : "res")));
		}
#else
		vm_.async_run(cyng::generate_invoke("log.msg.debug", "sig.ins", source, tbl->meta().get_name()));
#endif

		//
		//	don't send data back to sender
		//
		if (source != vm_.tag())
		{
			//	forward request
			vm_.async_run(bus_req_db_insert(tbl->meta().get_name()
				, key
				, data
				, gen
				, source));
		}
		else
		{
			//	send response
			vm_.async_run(bus_res_db_insert(tbl->meta().get_name()
				, key
				, data
				, gen));
		}
	}

	void session::sig_del(cyng::store::table const* tbl, cyng::table::key_type const& key, boost::uuids::uuid source)
	{
		vm_.async_run(cyng::generate_invoke("log.msg.debug", "sig.del", tbl->meta().get_name(), source));

		//
		//	don't send data back to sender
		//
		if (source != vm_.tag())
		{
			vm_.async_run(bus_req_db_remove(tbl->meta().get_name(), key, source));
		}
		else
		{
			vm_.async_run(bus_res_db_remove(tbl->meta().get_name(), key));

		}
	}

	void session::sig_clr(cyng::store::table const* tbl, boost::uuids::uuid source)
	{
		//
		//	don't send data back to sender
		//
		if (source != vm_.tag())
		{
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "sig.clr", tbl->meta().get_name(), source));
			vm_.async_run(bus_db_clear(tbl->meta().get_name()));
		}
	}

	void session::sig_mod(cyng::store::table const* tbl
		, cyng::table::key_type const& key
		, cyng::attr_t const& attr
		, std::uint64_t gen
		, boost::uuids::uuid source)
	{
		vm_.async_run(cyng::generate_invoke("log.msg.debug", "sig.mod", tbl->meta().get_name(), source));

		if (source != vm_.tag())
		{
			//
			//	forward modify request with parameter value
			//
			vm_.async_run(bus_req_db_modify(tbl->meta().get_name(), key, tbl->meta().to_param(attr), gen, source));
		}
		else
		{
			//	send response
			vm_.async_run(bus_res_db_modify(tbl->meta().get_name(), key, attr, gen));
			
			if (boost::algorithm::equals(tbl->meta().get_name(), "_Config"))
			{
				if (!key.empty() && boost::algorithm::equals(cyng::value_cast<std::string>(key.at(0), "?"), "connection-auto-login"))
				{
					client_.set_connection_auto_login(attr.second);
				}
				else if (!key.empty() && boost::algorithm::equals(cyng::value_cast<std::string>(key.at(0), "?"), "connection-auto-enabled"))
				{
					client_.set_connection_auto_enabled(attr.second);
				}
				else if (!key.empty() && boost::algorithm::equals(cyng::value_cast<std::string>(key.at(0), "?"), "connection-superseed"))
				{
					client_.set_connection_superseed(attr.second);
				}
				else if (!key.empty() && boost::algorithm::equals(cyng::value_cast<std::string>(key.at(0), "?"), "generate-time-series"))
				{
					client_.set_generate_time_series(attr.second);

					//
					//	write a line	
					//
					//
					if (client_.is_generate_time_series()) {
						db_.access([&](cyng::store::table const* tbl_session)->void {

							const auto size = tbl_session->size();
							tbl_session->loop([&](cyng::table::record const& rec) -> bool {
								const auto tag = cyng::value_cast(rec["tag"], boost::uuids::nil_uuid());
								const auto name = cyng::value_cast<std::string>(rec["name"], "");
								auto local_peer = cyng::object_cast<session>(rec["local"]);

								if (local_peer != nullptr) {
									const_cast<session*>(local_peer)->client_.write_stat(tag, name, "start recording...", size);
								}
								return true;
							});
						}, cyng::store::read_access("_Session"));
					}
				}
			}
		}
	}

	void session::bus_req_unsubscribe(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "unsubscribe " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			std::string,			//	[0] table name
			boost::uuids::uuid		//	[1] remote session tag
		>(frame);

		cyng::store::close_subscription(subscriptions_, std::get<0>(tpl));
	}

	cyng::vector_t session::reply(std::chrono::system_clock::time_point ts, bool success)
	{
		cyng::vector_t prg;
		prg << cyng::generate_invoke_unwinded("stream.serialize"
			, cyng::generate_invoke_remote_unwinded("bus.res.login"
				, success
				, cyng::code::IDENT
				, cyng::version(NODE_VERSION_MAJOR, NODE_VERSION_MINOR)
				, ts	//	timestamp of sender
				, std::chrono::system_clock::now()));
		prg
			<< cyng::generate_invoke_unwinded("stream.flush")
			;
		CYNG_LOG_TRACE(logger_, "reply: " << cyng::io::to_str(prg));
		return prg;
	}


	void session::client_req_login(cyng::context& ctx)
	{
		//	[1cea6ddf-5044-478b-9fba-67fa23997ba6,65d9eb67-2187-481b-8770-5ce41801eaa6,1,data-store,secret,plain,%(("security":scrambled),("tp-layer":ipt)),<!259:session>]
		//
		//	* remote client tag
		//	* remote peer tag
		//	* sequence number (should be continually incremented)
		//	* name
		//	* pwd/credentials
		//	* authorization scheme
		//	* bag (to send back)
		//	* session object
		//
		const cyng::vector_t frame = ctx.get_frame();
		//CYNG_LOG_INFO(logger_, "client.req.login " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] remote client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			std::string,			//	[3] name
			std::string,			//	[4] pwd/credential
			std::string,			//	[5] authorization scheme
			cyng::param_map_t		//	[6] bag
		>(frame);

		client_.req_login(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl)
			, std::get<5>(tpl)
			, std::get<6>(tpl)
			, frame.at(7));
	}

	void session::client_req_close(cyng::context& ctx)
	{
		//	[d673954a-741d-41c1-944c-d0036be5353f,47e74fa2-1410-4f65-b4fb-b8bce05d8061,16,10054]
		//
		//	* remote client tag
		//	* remote peer tag
		//	* sequence number (should be continuously incremented)
		//	* error code value
		//
		const cyng::vector_t frame = ctx.get_frame();
		//CYNG_LOG_INFO(logger_, "client.req.close " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] remote client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			int						//	[3] error code
		>(frame);

		client_.req_close(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, ctx.tag()
			, true);	//	request

	}

	void session::client_res_close(cyng::context& ctx)
	{
		//	[d673954a-741d-41c1-944c-d0036be5353f,47e74fa2-1410-4f65-b4fb-b8bce05d8061,16,10054]
		//
		//	* remote client tag
		//	* remote peer tag
		//	* sequence number (should be continuously incremented)
		//	* [bool] success flag
		//
		const cyng::vector_t frame = ctx.get_frame();

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] remote client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			bool					//	[3] success flag
		>(frame);

		if (std::get<3>(tpl))
		{
			CYNG_LOG_INFO(logger_, "client.req.close " << cyng::io::to_str(frame));
			client_.req_close(ctx
				, std::get<0>(tpl)
				, std::get<1>(tpl)
				, std::get<2>(tpl)
				, ctx.tag()
				, false);	//	resonse
		}
		else
		{
			CYNG_LOG_WARNING(logger_, "client.req.close " 
				<< cyng::io::to_str(frame)
				<< " failed");
		}
	}
	void session::client_req_open_push_channel(cyng::context& ctx)
	{
		//	[00517ac4-e4cb-4746-83c9-51396043678a,0c2058e2-0c7d-4974-98bd-bf1b14c7ba39,12,LSM_Events,,,,,003c,%(("seq":52),("tp-layer":ipt))]
		//
		//	* client tag
		//	* peer
		//	* bus sequence
		//	* target name
		//	* device name
		//	* device number
		//	* device software version
		//	* device id
		//	* timeout
		//	* bag
		//
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "client.req.open.push.channel " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] remote client tag
			boost::uuids::uuid,		//	[1] remote peer tag
			std::uint64_t,			//	[2] sequence number
			std::string,			//	[3] target name
			std::string,			//	[4] device name
			std::string,			//	[5] device number
			std::string,			//	[6] device software version
			std::string,			//	[7] device id
			std::chrono::seconds,	//	[8] timeout
			cyng::param_map_t		//	[9] bag
		>(frame);

		client_.req_open_push_channel(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl)
			, std::get<5>(tpl)
			, std::get<6>(tpl)
			, std::get<7>(tpl)
			, std::get<8>(tpl)
			, std::get<9>(tpl)
			, ctx.tag());
	}

	void session::client_res_open_push_channel(cyng::context& ctx)
	{
		//	 [5e065593-d587-4b1b-a5c0-e79bd0fb2eb0,ee82b721-1411-46f5-8df7-7c797842e4f3,0,false,00000000,00000000,00000000,%(),%(("seq":1),("tp-layer":ipt))]
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_WARNING(logger_, "client.res.open.push.channel " << cyng::io::to_str(frame));

		//
		//	There is a bug in the EMH VSM to respond to connection close request with an "open channel response".
		//	So this message can be partly ignored.
		//	Additionally the VSM don't send a connection open request the first time after a connection was cleared from
		//	the remote side.
		//

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] tag
			boost::uuids::uuid,		//	[1] peer
			std::uint64_t,			//	[2] cluster sequence
			bool,					//	[3] success flag
			std::uint32_t,			//	[4] channel
			std::uint32_t,			//	[5] source
			std::uint32_t,			//	[6] count
			cyng::param_map_t,		//	[7] options
			cyng::param_map_t		//	[8] bag
		>(frame);

		boost::ignore_unused(tpl);
	}

	void session::client_req_close_push_channel(cyng::context& ctx)
	{
		//	[b7fcf460-84e4-493e-910e-2f9736774351,fbda6a25-d406-4d22-aca0-0ba3a8cd589a,682,2fd6e208,%(("seq":d7),("tp-layer":ipt))]
		//
		//	* remote session tag
		//	* remote peer
		//	* cluster sequence
		//	* channel
		//	* bag
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "client.req.close.push.channel " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] remote client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			std::uint32_t,			//	[3] channel
			cyng::param_map_t		//	[4] bag
		>(frame);

		client_.req_close_push_channel(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl));
	}

	void session::client_req_register_push_target(cyng::context& ctx)
	{
		//	[4e5dc7e8-37e7-4267-a3b6-d68a9425816f,76fbb814-3e74-419b-8d43-2b7b59dab7f1,67,data.sink.2,%(("pSize":65535),("seq":2),("tp-layer":ipt),("wSize":1))]
		//
		//	* remote client tag
		//	* remote peer
		//	* bus sequence
		//	* target
		//	* bag
		//
		const cyng::vector_t frame = ctx.get_frame();
		ctx.run(cyng::generate_invoke("log.msg.info", "client.req.register.push.target", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] remote client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			std::string,			//	[3] target name
			cyng::param_map_t		//	[4] bag
		>(frame);

		client_.req_register_push_target(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl)
			, ctx.tag());
	}

	void session::client_req_deregister_push_target(cyng::context& ctx)
	{
		//	[4e5dc7e8-37e7-4267-a3b6-d68a9425816f,76fbb814-3e74-419b-8d43-2b7b59dab7f1,67,power@solostec,%(("seq":2),("tp-layer":ipt))]
		//
		//	* remote client tag
		//	* remote peer
		//	* bus sequence
		//	* target
		//	* bag
		//
		const cyng::vector_t frame = ctx.get_frame();
		ctx.run(cyng::generate_invoke("log.msg.info", "client.req.deregister.push.target", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] remote client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			std::string,			//	[3] target name
			cyng::param_map_t		//	[4] bag
		>(frame);

		client_.req_deregister_push_target(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl)
			, ctx.tag());
	}

	void session::client_req_open_connection(cyng::context& ctx)
	{
		//	[client.req.open.connection,[29feef99-3f33-4c90-b0db-8c2c50d3d098,90732ba2-1c8a-4587-a19f-caf27752c65a,3,LSMTest1,%(("seq":1),("tp-layer":ipt))]]
		//
		//	* client tag
		//	* peer
		//	* bus sequence
		//	* number
		//	* bag
		//	* this session object
		//
		const cyng::vector_t frame = ctx.get_frame();
		ctx.run(cyng::generate_invoke("log.msg.info", "client.req.open.connection", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] remote client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			std::string,			//	[3] number
			cyng::param_map_t		//	[4] bag
		>(frame);

		client_.req_open_connection(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl)
			, frame.at(5));
	}

	void session::client_res_open_connection(cyng::context& ctx)
	{
		//	[client.res.open.connection,
		//	[232e356c-7188-4c2b-bc4b-ee3247061904,b81f32c5-eecd-43f2-9183-cacc255ad03b,0,false,
		//	%(("seq":1),("tp-layer":ipt))]]
		//
		//	* origin session tag
		//	* peer (origin)
		//	* cluster sequence
		//	* success
		//	* options
		//	* bag (origin)
		const cyng::vector_t frame = ctx.get_frame();
		//ctx.run(cyng::generate_invoke("log.msg.info", "client.res.open.connection", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] origin client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			bool,					//	[3] success
			cyng::param_map_t,		//	[4] options
			cyng::param_map_t		//	[5] bag
		>(frame);

		client_.res_open_connection(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl)
			, std::get<5>(tpl));

	}

	void session::client_req_close_connection(cyng::context& ctx)
	{
		//
		//	[5afa7628-caa3-484d-b1de-a4730b53a656,bdc31cf8-e18e-4d95-ad31-ad821661e857,false,8,
		//	%(("tp-layer":modem))]
		//
		//	* remote session tag
		//	* remote peer
		//	* shutdown
		//	* cluster sequence
		//	* bag
		//
		const cyng::vector_t frame = ctx.get_frame();

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] origin client tag
			boost::uuids::uuid,		//	[1] peer tag
			bool,					//	[2] shutdown
			std::uint64_t,			//	[3] sequence number
			cyng::param_map_t		//	[4] bag
		>(frame);

		ctx.run(cyng::generate_invoke("log.msg.info", "client.req.close.connection", frame));

		client_.req_close_connection(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)	//	shutdown mode
			, std::get<3>(tpl)
			, std::get<4>(tpl));

	}

	void session::client_res_close_connection(cyng::context& ctx)
	{
		//	[906add5a-403d-4237-8275-478ba9efec4b,4386c0d0-4307-4160-bb76-6fb8ae4d5c4b,2,true,%(("local-connect":false),("local-peer":08b3ef0a-c492-4317-be22-d34b95651e56),("origin-tag":906add5a-403d-4237-8275-478ba9efec4b),("send-response":false)),%()]
		//	, tag
		//	, cyng::code::IDENT
		//	, seq
		//	, success
		//	, options
		//	, bag))

		const cyng::vector_t frame = ctx.get_frame();
		ctx.run(cyng::generate_invoke("log.msg.info", "client.res.close.connection", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] origin client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			bool,					//	[3] success
			cyng::param_map_t,		//	[4] options
			cyng::param_map_t		//	[5] bag
		>(frame);

		client_.res_close_connection(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl)
			, std::get<5>(tpl));

	}

	void session::client_req_transfer_pushdata(cyng::context& ctx)
	{
		//	[89fd8b7b-ceae-4803-bdd9-e88dfa07cbf0,1bbcd541-5d5f-46fa-b6a8-2e8c5fa25d26,17,4ee4092b,f807b7df,%(("block":0),("seq":a3),("status":c1),("tp-layer":ipt)),1B1B1B1B010101017608343131323237...53B01EC46010163FA7D00]
		//
		//	* session tag
		//	* peer
		//	* cluster seq
		//	* channel
		//	* source
		//	* bag
		//	* data
		const cyng::vector_t frame = ctx.get_frame();
		//ctx.run(cyng::generate_invoke("log.msg.info", "client.req.transfer.pushdata", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] origin client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			std::uint32_t,			//	[3] channel
			std::uint32_t,			//	[4] source
			cyng::param_map_t		//	[5] bag
		>(frame);

		client_.req_transfer_pushdata(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, std::get<4>(tpl)
			, std::get<5>(tpl)
			, frame.at(6));

	}

	void session::client_req_transmit_data(cyng::context& ctx)
	{
		//	 [client.req.transmit.data,
		//	[540a2bda-5891-44a6-909b-89bfdc494a02,767a51e3-ded3-4eb3-bc61-8bfd931afc75,7,
		//	%(("tp-layer":ipt))
		//	&&1B1B1B1B010101017681063138...8C000000001B1B1B1B1A03E6DD]]
		const cyng::vector_t frame = ctx.get_frame();
		ctx.run(cyng::generate_invoke("log.msg.trace", "client.req.transmit.data", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] origin client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			cyng::param_map_t		//	[3] bag
		>(frame);

		client_.req_transmit_data(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, frame.at(4));
	}

	void session::client_inc_throughput(cyng::context& ctx)
	{
		//	[31481e4a-7952-40a7-9e13-6bd9255ac84b,812a8baa-41aa-4633-9379-0cb9a562ce62,112]
		const cyng::vector_t frame = ctx.get_frame();
		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] origin tag
			boost::uuids::uuid,		//	[1] target tag
			boost::uuids::uuid,		//	[2] peer
			std::uint64_t			//	[3] bytes transferred
		>(frame);

		db_.access([&](cyng::store::table* tbl_session, cyng::store::table* tbl_connection)->void {

			cyng::table::record rec_origin = tbl_session->lookup(cyng::table::key_generator(std::get<0>(tpl)));
			if (!rec_origin.empty())
			{
				const std::uint64_t rx = cyng::value_cast<std::uint64_t>(rec_origin["rx"], 0u);
				tbl_session->modify(rec_origin.key(), cyng::param_factory("rx", static_cast<std::uint64_t>(rx + std::get<3>(tpl))), std::get<2>(tpl));
			}

			cyng::table::record rec_target = tbl_session->lookup(cyng::table::key_generator(std::get<1>(tpl)));
			if (!rec_target.empty())
			{
				const std::uint64_t sx = cyng::value_cast<std::uint64_t>(rec_target["sx"], 0u);
				tbl_session->modify(rec_target.key(), cyng::param_factory("sx", static_cast<std::uint64_t>(sx + std::get<3>(tpl))), std::get<2>(tpl));
			}

			cyng::table::record rec_conn = connection_lookup(tbl_connection, cyng::table::key_generator(std::get<0>(tpl), std::get<1>(tpl)));
			if (!rec_conn.empty())
			{
				const std::uint64_t throughput = cyng::value_cast<std::uint64_t>(rec_conn["throughput"], 0u);
				tbl_connection->modify(rec_conn.key(), cyng::param_factory("throughput", static_cast<std::uint64_t>(throughput + std::get<3>(tpl))), std::get<2>(tpl));
			}
		}	, cyng::store::write_access("_Session")
			, cyng::store::write_access("_Connection"));

	}
	
	void session::client_update_attr(cyng::context& ctx)
	{
		//	[a960e209-c2bb-427f-83a5-b0c0c2e75251,944d1f66-c6dd-4ed8-a96b-fbcc2d07b82a,13,software.version,0.2.2018030,%(("seq":2),("tp-layer":ipt))]
		//
		//	* session tag
		//	* peer
		//	* cluster seq
		//	* attribute name
		//	* value
		//	* bag
		const cyng::vector_t frame = ctx.get_frame();
		ctx.run(cyng::generate_invoke("log.msg.trace", "client.update.attr", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] origin client tag
			boost::uuids::uuid,		//	[1] peer tag
			std::uint64_t,			//	[2] sequence number
			std::string,			//	[3] name
			cyng::param_map_t		//	[4] bag
		>(frame);

		client_.update_attr(ctx
			, std::get<0>(tpl)
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, std::get<3>(tpl)
			, frame.at(5)
			, std::get<4>(tpl));
	}

	void session::bus_insert_msg(cyng::context& ctx)
	{
		//	[2308f3d1-4a68-45ae-b5fa-cedf019e29b7,TRACE,http.upload.progress: 72%]
		//
		//	* session tag
		//	* severity
		//	* message
		const cyng::vector_t frame = ctx.get_frame();
		ctx.run(cyng::generate_invoke("log.msg.trace", "client.insert.msg", frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,			//	[0] origin client tag
			cyng::logging::severity,	//	[1] level
			std::string					//	[2] msg
		>(frame);

		insert_msg(db_
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, ctx.tag());
	}

	void session::bus_req_push_data(cyng::context& ctx)
	{
		//	[1,store,TLoraMeta,false
		//	[7df1353e-8968-47b7-81bd-1c3097dcc878],
		//	[5,52,100000728,29000071,1,LC2,G1,12,7,-99,444eefd3,32332e31342c2032332e32302c20313031342c2035352e3538,148,0,1,1,0001,2015-10-22 13:39:59.48900000,F03D291000001180],
		//	9228e436-7a13-4140-82f1-2c38229d0f7a]
		//
		//	* bus sequene
		//	* class name
		//	* channel/table name
		//	* distribution (single=false/all=true)
		//	* key
		//	* data
		//	* source
		const cyng::vector_t frame = ctx.get_frame();
		ctx.run(cyng::generate_invoke("log.msg.trace", "bus.req.push.data", frame));

		auto const tpl = cyng::tuple_cast<
			std::uint64_t,		//	[0] sequence number
			std::string,		//	[1] class
			std::string,		//	[2] channel
			bool,				//	[3] distribution
			cyng::vector_t,		//	[4] key
			cyng::vector_t,		//	[5] data
			boost::uuids::uuid	//	[6] source
		>(frame);

		//
		//	search for requested class
		//
		std::size_t counter{ 0 };
		db_.access([&](cyng::store::table const* tbl_cluster)->void {
			tbl_cluster->loop([&](cyng::table::record const& rec) -> bool {

				//
				//	search class
				//
				//CYNG_LOG_TRACE(logger_, "search node class " << std::get<1>(tpl) << " ?= " << cyng::value_cast<std::string>(rec["class"], ""));
				if (boost::algorithm::equals(cyng::value_cast<std::string>(rec["class"], ""), std::get<1>(tpl)))
				{
					//
					//	required class found
					//
					CYNG_LOG_TRACE(logger_, "node class " 
						<< std::get<1>(tpl) 
						<< " found: "
						<< cyng::value_cast<>(rec["ep"], boost::asio::ip::tcp::endpoint()));

					auto peer = cyng::object_cast<session>(rec["self"]);
					if (peer)
					{
						counter++;
						peer->vm_.async_run(node::bus_req_push_data(std::get<0>(tpl), std::get<2>(tpl), std::get<4>(tpl), std::get<5>(tpl), std::get<6>(tpl)));
					}

					//
					//	single=false / all=true
					//
					return std::get<3>(tpl);
				}
				//	continue
				return true;
			});
		}, cyng::store::read_access("_Cluster"));

		ctx.attach(node::bus_res_push_data(std::get<0>(tpl)	//	seq
			, std::get<1>(tpl)
			, std::get<2>(tpl)
			, counter));
	}

	cyng::object make_session(cyng::async::mux& mux
		, cyng::logging::log_ptr logger
		, boost::uuids::uuid mtag
		, cyng::store::db& db
		, std::string const& account
		, std::string const& pwd
		, boost::uuids::uuid stag
		, std::chrono::seconds monitor //	cluster watchdog
		, std::atomic<std::uint64_t>& global_configuration
		, boost::filesystem::path stat_dir)
	{
		return cyng::make_object<session>(mux, logger, mtag, db, account, pwd, stag, monitor
			, global_configuration, stat_dir);
	}

}

namespace cyng
{
	namespace traits
	{

#if defined(CYNG_LEGACY_MODE_ON)
		const char type_tag<node::session>::name[] = "session";
#endif
	}	// traits	

}


