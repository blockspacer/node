/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 


#include "server.h"
#include "connection.h"
#include <smf/ipt/scramble_key_io.hpp>
#include <smf/cluster/generator.h>
#include <cyng/vm/generator.h>
#include <cyng/dom/reader.h>
#include <cyng/object_cast.hpp>
#include <cyng/tuple_cast.hpp>
#include <cyng/factory/set_factory.h>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace node 
{
	namespace ipt
	{
		server::server(cyng::async::mux& mux
			, cyng::logging::log_ptr logger
			, bus::shared_type bus
			, scramble_key const& sk
			, uint16_t watchdog
			, int timeout)
		: mux_(mux)
			, logger_(logger)
			, bus_(bus)
			, sk_(sk)
			, watchdog_(watchdog)
			, timeout_(timeout)
			, acceptor_(mux.get_io_service())
#if (BOOST_VERSION < 106600)
            , socket_(mux_.get_io_service())
#endif
			, rnd_()
			, client_map_()
			, connection_map_()
		{
			//
			//	connection management
			//
			bus_->vm_.register_function("push.connection", 1, std::bind(&server::push_connection, this, std::placeholders::_1));
			bus_->vm_.register_function("push.ep.local", 1, std::bind(&server::push_ep_local, this, std::placeholders::_1));
			bus_->vm_.register_function("push.ep.remote", 1, std::bind(&server::push_ep_remote, this, std::placeholders::_1));
			bus_->vm_.register_function("server.insert.connection", 2, std::bind(&server::insert_connection, this, std::placeholders::_1));
			bus_->vm_.register_function("server.close.connection", 3, std::bind(&server::close_connection, this, std::placeholders::_1));
			bus_->vm_.register_function("server.transmit.data", 2, std::bind(&server::transmit_data, this, std::placeholders::_1));

			//
			//	client responses
			//
			bus_->vm_.register_function("client.res.login", 7, std::bind(&server::client_res_login, this, std::placeholders::_1));
			bus_->vm_.register_function("client.res.close", 3, std::bind(&server::client_res_close_impl, this, std::placeholders::_1));
			bus_->vm_.register_function("client.req.close", 4, std::bind(&server::client_req_close_impl, this, std::placeholders::_1));
			bus_->vm_.register_function("client.req.reboot", 6, std::bind(&server::client_req_reboot, this, std::placeholders::_1));
			bus_->vm_.register_function("client.req.query.srv.visible", 6, std::bind(&server::client_req_query_srv_visible, this, std::placeholders::_1));
			bus_->vm_.register_function("client.req.query.srv.active", 6, std::bind(&server::client_req_query_srv_active, this, std::placeholders::_1));

			bus_->vm_.register_function("client.res.open.push.channel", 7, std::bind(&server::client_res_open_push_channel, this, std::placeholders::_1));
			bus_->vm_.register_function("client.res.register.push.target", 1, std::bind(&server::client_res_register_push_target, this, std::placeholders::_1));
			bus_->vm_.register_function("client.res.deregister.push.target", 6, std::bind(&server::client_res_deregister_push_target, this, std::placeholders::_1));

			bus_->vm_.register_function("client.res.open.connection", 6, std::bind(&server::client_res_open_connection, this, std::placeholders::_1));
			bus_->vm_.register_function("client.req.open.connection.forward", 6, std::bind(&server::client_req_open_connection_forward, this, std::placeholders::_1));
			bus_->vm_.register_function("client.res.open.connection.forward", 6, std::bind(&server::client_res_open_connection_forward, this, std::placeholders::_1));
			bus_->vm_.register_function("client.req.transmit.data.forward", 5, std::bind(&server::client_req_transmit_data_forward, this, std::placeholders::_1));
			bus_->vm_.register_function("client.res.transfer.pushdata", 7, std::bind(&server::client_res_transfer_pushdata, this, std::placeholders::_1));
			bus_->vm_.register_function("client.req.transfer.pushdata.forward", 7, std::bind(&server::client_req_transfer_pushdata_forward, this, std::placeholders::_1));
			bus_->vm_.register_function("client.res.close.push.channel", 6, std::bind(&server::client_res_close_push_channel, this, std::placeholders::_1));
			bus_->vm_.register_function("client.req.close.connection.forward", 7, std::bind(&server::client_req_close_connection_forward, this, std::placeholders::_1));
			bus_->vm_.register_function("client.res.close.connection.forward", 6, std::bind(&server::client_res_close_connection_forward, this, std::placeholders::_1));

		}

		void server::run(std::string const& address, std::string const& service)
		{
			//CYNG_LOG_TRACE(logger_, "listen " 
			//	<< address
			//	<< ':'
			//	<< service);

			// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
			boost::asio::ip::tcp::resolver resolver(mux_.get_io_service());
#if (BOOST_VERSION >= 106600)
			boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(address, service).begin();
#else
			boost::asio::ip::tcp::resolver::query query(address, service);
			boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
#endif
			acceptor_.open(endpoint.protocol());
			acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
			acceptor_.bind(endpoint);
			acceptor_.listen();

			CYNG_LOG_INFO(logger_, "listen "
				<< endpoint.address().to_string()
				<< ':'
				<< endpoint.port());

			do_accept();
		}

		void server::do_accept()
		{
#if (BOOST_VERSION >= 106600)
			acceptor_.async_accept(
				[this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
				// Check whether the server was stopped by a signal before this
				// completion handler had a chance to run.
				if (!acceptor_.is_open()) 
				{
					return;
				}

				if (!ec) 
				{
					CYNG_LOG_TRACE(logger_, "accept " << socket.remote_endpoint());

					//
					//	create new connection/session
					//
					auto tag = rnd_();
					auto conn = make_connection(std::move(socket)
						, mux_
						, logger_
						, bus_
						, tag
						, sk_
						, watchdog_
						, timeout_);

					//
					//	bus is synchronizing access to client_map_
					//
					bus_->vm_.async_run(cyng::generate_invoke("server.insert.connection", tag, conn));

					//
					//
					//	continue accepting
					//
					do_accept();
				}

			});
#else
			acceptor_.async_accept(socket_, [=](boost::system::error_code const& ec) {
				// Check whether the server was stopped by a signal before this
				// completion handler had a chance to run.
				if (acceptor_.is_open() && !ec) {

					CYNG_LOG_TRACE(logger_, "accept " << socket_.remote_endpoint());
					//
					//	There is no connection manager or list of open connections. 
					//	Connections are managed by there own and are controlled
					//	by a maintenance task.
					//
					//std::make_shared<connection>(std::move(socket), mux_, logger_, db_)->start();
					do_accept();
				}

				else 
				{
					//on_error(ec);
					//	shutdown server
				}

			});
#endif

		}

		void server::close()
		{
            //
            //	close acceptor
            //
            CYNG_LOG_INFO(logger_, "close acceptor");

            // The server is stopped by cancelling all outstanding asynchronous
			// operations. Once all operations have finished the io_context::run()
			// call will exit.
			acceptor_.close();

            //
            // close all clients
            //
            CYNG_LOG_INFO(logger_, "close "
                << client_map_.size()
                << " ipt client(s)");

            for(auto& conn : client_map_)
            {
                const_cast<connection*>(cyng::object_cast<connection>(conn.second))->stop(conn.second);
            }

		}

		void server::insert_connection(cyng::context& ctx)
		{
			BOOST_ASSERT(bus_->vm_.tag() == ctx.tag());

			//	[a95f46e9-eccd-4b47-b02c-17d5172218af,<!260:ipt::connection>]
			const cyng::vector_t frame = ctx.get_frame();

			auto tag = cyng::value_cast(frame.at(0), boost::uuids::nil_uuid());
			auto r = client_map_.emplace(tag, frame.at(1));
			if (r.second)
			{
				ctx.attach(cyng::generate_invoke("log.msg.trace", "server.insert.connection", frame));
				const_cast<connection*>(cyng::object_cast<connection>((*r.first).second))->start();
			}
			else
			{
				ctx.attach(cyng::generate_invoke("log.msg.error", "server.insert.connection - failed", frame));
			}
		}

		//	"server.close.connection"
		void server::close_connection(cyng::context& ctx)
		{
			BOOST_ASSERT(bus_->vm_.tag() == ctx.tag());

			//	[d1d7f8ef-fa12-4cf7-84ad-2e87bd73f19e,<!261:ipt::connection>,system:10054]
			//
			//	* client tag
			//	* connection object
			//	* error code
			//
			const cyng::vector_t frame = ctx.get_frame();
			//CYNG_LOG_TRACE(logger_, "server.close.connection " << cyng::io::to_str(frame));
			auto tag = cyng::value_cast(frame.at(0), boost::uuids::nil_uuid());
			auto ec = cyng::value_cast(frame.at(2), boost::system::error_code());

			//
			//	tell master to remove this client session
			//
			ctx.attach(client_req_close(tag, ec.value()));

			//
			//	If session is in an connection a "client.req.close.connection.forward" command
			//	will be send to remote party.
			//
			//	Master will send a "client.res.close"
			//
		}

		bool server::clear_connection_map(boost::uuids::uuid tag)
		{
			auto pos = connection_map_.find(tag);
			if (pos != connection_map_.end())
			{
				auto receiver = pos->second;

				CYNG_LOG_INFO(logger_, "clean up local connection: "
					<< tag
					<< " <==> "
					<< receiver);


				//
				//	remove both entries
				//
				connection_map_.erase(pos);
				connection_map_.erase(receiver);

				BOOST_ASSERT((connection_map_.size() % 2) == 0);

				return true;
			}

			return false;
		}

		void server::transmit_data(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			//
			//	[8113a678-903f-4975-8f11-49c1bf63a3c6,1B1B1B1B010101017681063138303430...000001B1B1B1B1A039946]
			//
			//	* sender
			//	* data
			//
			//ctx.attach(cyng::generate_invoke("log.msg.debug", "server.transmit.data", frame));
			auto tag = cyng::value_cast(frame.at(0), boost::uuids::nil_uuid());
			auto pos = connection_map_.find(tag);
			if (pos != connection_map_.end())
			{
				auto receiver = pos->second;

				CYNG_LOG_TRACE(logger_, "transfer data local "
					<< tag
					<< " ==> "
					<< receiver);

				propagate("ipt.transfer.data", receiver, cyng::vector_t({ frame.at(1) }));
				propagate("stream.flush", receiver, cyng::vector_t({}));

				//
				//	update meta data
				//
				auto dp = cyng::object_cast<cyng::buffer_t>(frame.at(1));
				if (dp != nullptr)
				{
					ctx.attach(client_inc_throughput(tag
						, receiver
						, dp->size()));
				}
			}
			else
			{
				CYNG_LOG_ERROR(logger_, "transmit data failed: "
					<< tag
					<< " is not member of connection map");

			}
		}

		//void server::req_stop_client(cyng::context& ctx)
		//{
		//	const cyng::vector_t frame = ctx.get_frame();

		//	//	[2,[1ca8529b-9b95-4a3a-ba2f-26c7280aa878],ac70b95e-76b9-463a-a961-bb02f70e86c8]
		//	//
		//	//	* bus sequence
		//	//	* session key
		//	//	* source

		//	auto const tpl = cyng::tuple_cast<
		//		std::uint64_t,			//	[0] sequence
		//		cyng::vector_t,			//	[1] session key
		//		boost::uuids::uuid		//	[2] source
		//	>(frame);

		//	//
		//	//	slot [2] - stop session
		//	//
		//	if (std::get<1>(tpl).size() == 1)
		//	{
		//		CYNG_LOG_INFO(logger_, "bus.req.stop.client " << cyng::io::to_str(frame));
		//		//mux_.post(task_, 2, cyng::tuple_factory(std::get<1>(tpl)[0]));
		//	}
		//	else
		//	{
		//		CYNG_LOG_ERROR(logger_, "bus.req.stop.client " << cyng::io::to_str(frame));
		//	}
		//}

		void server::push_connection(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			auto tag = cyng::value_cast(frame.at(0), boost::uuids::nil_uuid());
			auto pos = client_map_.find(tag);
			if (pos != client_map_.end())
			{
				CYNG_LOG_TRACE(logger_, "push.connection "
					<< tag
					<< " on stack");
				ctx.push(pos->second);
			}
			else
			{
				CYNG_LOG_FATAL(logger_, "push.connection "
					<< tag
					<< " not found");
				ctx.push(cyng::make_object());
			}
		}

		void server::push_ep_local(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			auto tag = cyng::value_cast(frame.at(0), boost::uuids::nil_uuid());
			auto pos = client_map_.find(tag);
			if (pos != client_map_.end())
			{
				auto ep = const_cast<connection*>(cyng::object_cast<connection>(pos->second))->socket_.local_endpoint();
				CYNG_LOG_TRACE(logger_, "push.ep.local "
					<< tag
					<< ": "
					<< ep
					<< " on stack");

				ctx.push(cyng::make_object(ep));
			}
			else
			{
				CYNG_LOG_FATAL(logger_, "push.ep.local "
					<< tag
					<< " not found");
				ctx.push(cyng::make_object());
			}
		}

		void server::push_ep_remote(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			auto tag = cyng::value_cast(frame.at(0), boost::uuids::nil_uuid());
			auto pos = client_map_.find(tag);
			if (pos != client_map_.end())
			{
				auto ep = const_cast<connection*>(cyng::object_cast<connection>(pos->second))->socket_.remote_endpoint();
				CYNG_LOG_TRACE(logger_, "push.ep.remote "
					<< tag
					<< ": "
					<< ep
					<< " on stack");

				ctx.push(cyng::make_object(ep));
			}
			else
			{
				CYNG_LOG_FATAL(logger_, "push.ep.remote "
					<< tag
					<< " not found");
				ctx.push(cyng::make_object());
			}
		}


		void server::client_res_login(cyng::context& ctx)
		{
			//	[068544fb-9513-4cbe-9007-c9dd9892aff6,d03ff1a5-838a-4d71-91a1-fc8880b157a6,17,true,OK,%(("tp-layer":ipt))]
			//
			//	* client tag
			//	* peer
			//	* sequence
			//	* response (bool)
			//	* name
			//	* msg (optional)
			//	* query
			//	* bag
			//	
			propagate("client.res.login", ctx.get_frame());
		}

		void server::client_res_close_impl(cyng::context& ctx)
		{
			//	[9f6585e8-c7c6-4f93-9b99-e21986dec2bb,3ed440a2-958f-4863-9f86-ff8b1a0dc2f1,19]
			//
			//	* client tag
			//	* peer
			//	* sequence
			//
			const cyng::vector_t frame = ctx.get_frame();
			ctx.attach(cyng::generate_invoke("log.msg.info", "client.res.close", frame));

			const boost::uuids::uuid tag = cyng::value_cast(frame.at(0), boost::uuids::nil_uuid());
			if (client_map_.erase(tag) != 0)
			{
				CYNG_LOG_INFO(logger_, "connection "
					<< tag
					<< " removed from session pool");

				//
				//	clean up connection_map_
				//
				clear_connection_map(tag);
			}
			else
			{
				ctx.attach(cyng::generate_invoke("log.msg.error", "client.res.close - failed", frame));
			}
		}

		void server::client_req_close_impl(cyng::context& ctx)
		{
			//
			//	Master requests to close a specific session. Typical reason is a login with an account
			//	that is already online. In supersede mode the running session will be canceled in favor  
			//	the new session.
			//

			//	[1d166271-ca0b-4875-93a3-0cec92dbe34d,ef013e5f-50b0-4afe-ae7b-4a2f1c83f287,1,0]
			//
			//	* client tag
			//	* peer
			//	* cluster sequence
			//
			const cyng::vector_t frame = ctx.get_frame();
			const auto seq = cyng::value_cast<std::uint64_t>(frame.at(2), 0u);
			const boost::uuids::uuid tag = cyng::value_cast(frame.at(0), boost::uuids::nil_uuid());
			auto pos = client_map_.find(tag);
			if (pos != client_map_.end())
			{
				auto cp = cyng::object_cast<connection>(pos->second);
				if (cp != nullptr)
				{
					ctx.attach(cyng::generate_invoke("log.msg.trace", "client.req.close", frame));
					const_cast<connection*>(cp)->stop(pos->second);
				}

				//
				//	remove from client list
				//
				client_map_.erase(pos);
				clear_connection_map(tag);

				//
				//	acknowledge that session was closed
				//
				ctx.attach(client_res_close(tag, seq, true));
			}
			else
			{
				ctx.attach(cyng::generate_invoke("log.msg.error", "client.req.close - failed", frame));
				ctx.attach(client_res_close(tag, seq, false));
			}
		}

		void server::client_req_reboot(cyng::context& ctx)
		{
			//
			//	Master requests to send a reboot sequence to a specific session.
			//
			//	[dca135f3-ff2b-4bf7-8371-a9904c74522b,4ef534ba-8ae3-4c83-92ef-2f978df104cb,25,05000000000011,operator,operator]
			const cyng::vector_t frame = ctx.get_frame();
			CYNG_LOG_TRACE(logger_, "client.req.reboot " << cyng::io::to_str(frame));
			propagate("client.req.reboot", ctx.get_frame());

		}

		void server::client_req_query_srv_visible(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			CYNG_LOG_TRACE(logger_, "client.req.query.srv.visible " << cyng::io::to_str(frame));
			propagate("client.req.query.srv.visible", ctx.get_frame());
		}

		void server::client_req_query_srv_active(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			CYNG_LOG_TRACE(logger_, "client.req.query.srv.active " << cyng::io::to_str(frame));
			propagate("client.req.query.srv.active", ctx.get_frame());
		}


		void server::propagate(std::string fun, cyng::vector_t const& msg)
		{
			if (!msg.empty())
			{
				auto pos = msg.begin();
				BOOST_ASSERT(pos != msg.end());
				auto tag = cyng::value_cast(*pos, boost::uuids::nil_uuid());

				//
				//	remove duplicate information
				//
				propagate(fun, tag, cyng::vector_t(++pos, msg.end()));
			}
			else
			{
				CYNG_LOG_FATAL(logger_, "empty function "
					<< fun);
			}
		}

		void server::propagate(std::string fun, boost::uuids::uuid tag, cyng::vector_t&& msg)
		{
			auto pos = client_map_.find(tag);
			if (pos != client_map_.end())
			{
				cyng::vector_t prg;
				prg
					<< cyng::code::ESBA
					<< cyng::unwinder(std::move(msg))
					<< cyng::invoke(fun)
					<< cyng::code::REBA
					;
				cyng::object_cast<connection>((*pos).second)->session_.vm_.async_run(std::move(prg));
			}
			else
			{
				if (boost::algorithm::equals(fun, "client.req.close.connection.forward"))
				{
					CYNG_LOG_WARNING(logger_, "session "
						<< tag
						<< " not found - clean up connection map");

					//
					//	clean up connection_map_
					//
					clear_connection_map(tag);
				}
				else
				{
					CYNG_LOG_ERROR(logger_, "session "
						<< tag
						<< " not found");

#ifdef _DEBUG
					for (auto client : client_map_)
					{
						CYNG_LOG_TRACE(logger_, client.first);
					}
#endif
				}
			}
		}

		void server::client_res_open_push_channel(cyng::context& ctx)
		{
			propagate("client.res.open.push.channel", ctx.get_frame());
		}

		void server::client_res_register_push_target(cyng::context& ctx)
		{
			propagate("client.res.register.push.target", ctx.get_frame());
		}

		void server::client_res_deregister_push_target(cyng::context& ctx)
		{
			propagate("client.res.deregister.push.target", ctx.get_frame());
		}

		void server::client_res_open_connection(cyng::context& ctx)
		{
			propagate("client.res.open.connection", ctx.get_frame());
		}

		void server::client_req_open_connection_forward(cyng::context& ctx)
		{
			propagate("client.req.open.connection.forward", ctx.get_frame());
		}

		void server::client_res_open_connection_forward(cyng::context& ctx)
		{
			cyng::vector_t frame = ctx.get_frame();

			//
			//	[bff22fb4-f410-4239-83ae-40027fb2609e,f07f47e1-7f4d-4398-8fb5-ae1d8942a50e,16,true,
			//	%(("device-name":LSMTest4),("local-connect":true),("local-peer":3d5994f1-60df-4410-b6ba-c2934f8bfd5e),("origin-tag":bff22fb4-f410-4239-83ae-40027fb2609e),("remote-peer":3d5994f1-60df-4410-b6ba-c2934f8bfd5e),("remote-tag":3f972487-e9a9-4e03-92d4-e2df8f6d30c5)),
			//	%(("seq":1),("tp-layer":ipt))]
			//
			//	* session tag
			//	* peer
			//	* cluster sequence
			//	* success
			//	* options
			//	* bag - original data
			//ctx.run(cyng::generate_invoke("log.msg.trace", "client.res.open.connection.forward", frame));

			auto const tpl = cyng::tuple_cast<
				boost::uuids::uuid,		//	[0] tag
				boost::uuids::uuid,		//	[1] peer
				std::uint64_t,			//	[2] cluster sequence
				bool,					//	[3] success
				cyng::param_map_t,		//	[4] options
				cyng::param_map_t		//	[5] bag
			>(frame);


			const bool success = std::get<3>(tpl);

			//
			//	dom reader
			//
			auto dom = cyng::make_reader(std::get<4>(tpl));
			auto local_connect = cyng::value_cast(dom.get("local-connect"), false);
			if (success && local_connect)
			{
				auto origin_tag = cyng::value_cast(dom.get("origin-tag"), boost::uuids::nil_uuid());
				auto remote_tag = cyng::value_cast(dom.get("remote-tag"), boost::uuids::nil_uuid());
				BOOST_ASSERT(origin_tag == std::get<0>(tpl));
				BOOST_ASSERT(origin_tag != remote_tag);

				//
				//	create antry in local connection list
				//
				ctx.run(cyng::generate_invoke("log.msg.trace", "establish local connection", origin_tag, remote_tag));

				//
				//	insert both directions
				//
				connection_map_.emplace(origin_tag, remote_tag);
				connection_map_.emplace(remote_tag, origin_tag);
				BOOST_ASSERT((connection_map_.size() % 2) == 0);

				//
				//	update connection state of remote session
				//
				cyng::vector_t prg;
				prg
					<< origin_tag
					<< local_connect	//	"true" is the only logical value
					;

				propagate("session.update.connection.state", remote_tag, std::move(prg));
			}

			//
			//	forward to session
			//
			propagate("client.res.open.connection.forward", std::move(frame));
		}

		void server::client_req_transmit_data_forward(cyng::context& ctx)
		{
			propagate("client.req.transmit.data.forward", ctx.get_frame());
		}

		void server::client_res_transfer_pushdata(cyng::context& ctx)
		{
			propagate("client.res.transfer.pushdata", ctx.get_frame());
		}

		void server::client_req_transfer_pushdata_forward(cyng::context& ctx)
		{
			propagate("client.req.transfer.pushdata.forward", ctx.get_frame());
		}

		void server::client_res_close_push_channel(cyng::context& ctx)
		{
			propagate("client.res.close.push.channel", ctx.get_frame());
		}

		void server::client_req_close_connection_forward(cyng::context& ctx)
		{
			propagate("client.req.close.connection.forward", ctx.get_frame());
		}

		void server::client_res_close_connection_forward(cyng::context& ctx)
		{
			propagate("client.res.close.connection.forward", ctx.get_frame());

		}

	}
}



