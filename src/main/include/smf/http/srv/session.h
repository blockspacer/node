/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_LIB_HTTP_SRV_SESSION_H
#define NODE_LIB_HTTP_SRV_SESSION_H

#include <memory>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>

namespace node
{
	class http_session : public std::enable_shared_from_this<http_session>
	{
		// This queue is used for HTTP pipelining.
		class queue
		{
			enum
			{
				// Maximum number of responses we will queue
				limit = 8
			};

			// The type-erased, saved work item
			struct work
			{
				virtual ~work() = default;
				virtual void operator()() = 0;
			};

			http_session& self_;
			std::vector<std::unique_ptr<work>> items_;

		public:
			explicit queue(http_session& self);

			// Returns `true` if we have reached the queue limit
			bool is_full() const;

			// Called when a message finishes sending
			// Returns `true` if the caller should initiate a read
			bool on_write();

			// Called by the HTTP handler to send a response.
			template<bool isRequest, class Body, class Fields>
			void operator()(boost::beast::http::message<isRequest, Body, Fields>&& msg)
			{
				// This holds a work item
				struct work_impl : work
				{
					http_session& self_;
					boost::beast::http::message<isRequest, Body, Fields> msg_;

					work_impl(http_session& self,
						boost::beast::http::message<isRequest, Body, Fields>&& msg)
						: self_(self)
						, msg_(std::move(msg))
					{
					}

					void operator()()
					{
						boost::beast::http::async_write(
							self_.socket_,
							msg_,
							boost::asio::bind_executor(
								self_.strand_,
								std::bind(
									&http_session::on_write,
									self_.shared_from_this(),
									std::placeholders::_1,
									msg_.need_eof())));
					}
				};

				// Allocate and store the work
				items_.emplace_back(new work_impl(self_, std::move(msg)));

				// If there was no previous work, start this one
				if (items_.size() == 1)
					(*items_.front())();
			}
		};

		boost::asio::ip::tcp::socket socket_;
		boost::asio::strand<boost::asio::io_context::executor_type> strand_;
		boost::asio::steady_timer timer_;
		boost::beast::flat_buffer buffer_;
		std::string const& doc_root_;
		boost::beast::http::request<boost::beast::http::string_body> req_;
		queue queue_;

	public:
		// Take ownership of the socket
		explicit http_session(boost::asio::ip::tcp::socket socket,
			std::string const& doc_root);

		// Start the asynchronous operation
		void run();
		void do_read();

		// Called when the timer expires.
		void on_timer(boost::system::error_code ec);

		void on_read(boost::system::error_code ec);
		void on_write(boost::system::error_code ec, bool close);
		void do_close();
		
	};

}

#endif
