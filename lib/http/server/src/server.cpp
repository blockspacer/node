/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 

#include <smf/http/srv/server.h>
#include <smf/http/srv/session.h>

namespace node
{
	namespace http
	{

		server::server(cyng::logging::log_ptr logger
			, boost::asio::io_context& ioc
			, boost::asio::ip::tcp::endpoint endpoint
			, std::string const& doc_root
			, node::bus::shared_type bus
			, cyng::store::db& cache)
		: acceptor_(ioc)
			, socket_(ioc)
			, logger_(logger)
			, doc_root_(doc_root)
			, bus_(bus)
			, cache_(cache)
			, connection_manager_(logger)
			, is_listening_(false)
			, shutdown_complete_()
			, mutex_()
			, rgn_()
		{
			boost::system::error_code ec;

			// Open the acceptor
			acceptor_.open(endpoint.protocol(), ec);
			if (ec)
			{
				CYNG_LOG_FATAL(logger_, "open: " << ec.message());
				return;
			}

			// Bind to the server address
			acceptor_.bind(endpoint, ec);
			if (ec)
			{
				CYNG_LOG_FATAL(logger_, "bind: " << ec.message());
				return;
			}

			// Start listening for connections
			acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
			if (ec)
			{
				CYNG_LOG_FATAL(logger_, "listen: " << ec.message());
				return;
			}
		}

		// Start accepting incoming connections
		bool server::run()
		{
			//
			//	set listener flag immediately to prevent
			//	a concurrent call of this same
			//	method
			//
			if (acceptor_.is_open() && !is_listening_.exchange(true))
			{
				do_accept();
			}
			else
			{
				CYNG_LOG_WARNING(logger_, "acceptor closed");
			}
			return is_listening_;
		}

		void server::do_accept()
		{
			acceptor_.async_accept(
				socket_,
				std::bind(
					&server::on_accept,
					this,
					std::placeholders::_1));
		}

		void server::on_accept(boost::system::error_code ec)
		{
			if (ec)
			{
				CYNG_LOG_ERROR(logger_, "accept: " << ec.message());

				//
				//	remove listener flag
				//
				is_listening_.exchange(false);

				//
				//	notify
				//
				cyng::async::lock_guard<cyng::async::mutex> lk(mutex_);
				shutdown_complete_.notify_all();

				return;
			}
			else
			{
				CYNG_LOG_TRACE(logger_, "accept: " << socket_.remote_endpoint());

				// Create the http_session and run it
				connection_manager_.start(std::make_shared<session>(logger_
					, connection_manager_
					, std::move(socket_)
					, doc_root_
					, bus_
					, cache_
					, rgn_()));
			}

			// Accept another connection
			do_accept();
		}

		void server::close()
		{
			if (is_listening_)
			{
				acceptor_.cancel();
				acceptor_.close();

				cyng::async::unique_lock<cyng::async::mutex> lock(mutex_);

				CYNG_LOG_TRACE(logger_, "listener is waiting for cancellation");
				//	wait for cancellation of accept operation
				shutdown_complete_.wait(lock, [this] {
					return !is_listening_.load();
				});
				CYNG_LOG_TRACE(logger_, "listener cancellation complete");

				//
				//	stop all connections
				//
				CYNG_LOG_INFO(logger_, "stop all connections");
				connection_manager_.stop_all();

			}
		}

		void server::send_ws(boost::uuids::uuid tag, cyng::vector_t&& prg)
		{
			connection_manager_.send_ws(tag, std::move(prg));
		}

	}
}