/*
* The MIT License (MIT)
*
* Copyright (c) 2018 Sylko Olzscher
*
*/

#include <smf/ipt/bus.h>
#include <smf/ipt/scramble_key_io.hpp>
#include <smf/ipt/response.hpp>
#include <NODE_project_info.h>
#include <cyng/vm/domain/log_domain.h>
#include <cyng/vm/domain/asio_domain.h>
#include <cyng/value_cast.hpp>
#include <cyng/io/serializer.h>
#include <boost/uuid/nil_generator.hpp>
#ifdef SMF_IO_DEBUG
#include <cyng/io/hex_dump.hpp>
#endif

namespace node
{
	namespace ipt
	{
		bus::bus(cyng::async::mux& mux, cyng::logging::log_ptr logger, boost::uuids::uuid tag, scramble_key const& sk, std::size_t tsk)
		: vm_(mux.get_io_service(), tag)
			, socket_(mux.get_io_service())
			, buffer_()
			, mux_(mux)
			, logger_(logger)
			, task_(tsk)
			, parser_([this](cyng::vector_t&& prg) {
				CYNG_LOG_INFO(logger_, prg.size() << " instructions received");
				vm_.async_run(std::move(prg));
			}, sk)
			, serializer_(socket_, vm_, sk)
			, state_(STATE_INITIAL_)
		{
			//
			//	register logger domain
			//
			cyng::register_logger(logger_, vm_);
			vm_.async_run(cyng::generate_invoke("log.msg.info", "log domain is running"));

			//
			//	register socket domain
			//
			cyng::register_socket(socket_, vm_);
			vm_.async_run(cyng::generate_invoke("log.msg.info", "ip:tcp:socket domain is running"));

			//
			//	set new scramble key after scrambled login request
			//
			vm_.async_run(cyng::register_function("ipt.set.sk", 1, [this](cyng::context& ctx) {
				const cyng::vector_t frame = ctx.get_frame();
				CYNG_LOG_TRACE(logger_, "set new scramble key "
					<< cyng::value_cast<scramble_key>(frame.at(0), scramble_key::null_scramble_key_));

				//	set new scramble key
				parser_.set_sk(cyng::value_cast<scramble_key>(frame.at(0), scramble_key::null_scramble_key_).key());
			}));

			//
			//	reset parser before reconnect
			//
			vm_.async_run(cyng::register_function("ipt.reset.parser", 1, [this](cyng::context& ctx) {
				const cyng::vector_t frame = ctx.get_frame();
				CYNG_LOG_TRACE(logger_, "reset parser "
					<< cyng::value_cast<scramble_key>(frame.at(0), scramble_key::null_scramble_key_));

				//	set new scramble key
				parser_.reset(cyng::value_cast<scramble_key>(frame.at(0), scramble_key::null_scramble_key_).key());
			}));
			

			//
			//	register ipt request handler
			//
			vm_.async_run(cyng::register_function("ipt.start", 0, [this](cyng::context& ctx) {
				this->start();
			}));

			//
			//	login response
			//
			vm_.async_run(cyng::register_function("ipt.res.login.public", 4, std::bind(&bus::ipt_res_login, this, std::placeholders::_1, false)));
			vm_.async_run(cyng::register_function("ipt.res.login.scrambled", 4, std::bind(&bus::ipt_res_login, this, std::placeholders::_1, true)));

			vm_.async_run(cyng::register_function("ipt.res.register.push.target", 4, std::bind(&bus::ipt_res_register_push_target, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.transmit.data", 1, std::bind(&bus::ipt_req_transmit_data, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.open.connection", 1, std::bind(&bus::ipt_req_open_connection, this, std::placeholders::_1)));

			vm_.async_run(cyng::register_function("ipt.req.protocol.version", 2, std::bind(&bus::ipt_req_protocol_version, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.software.version", 2, std::bind(&bus::ipt_req_software_version, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.device.id", 0, std::bind(&bus::ipt_req_device_id, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.net.stat", 2, std::bind(&bus::ipt_req_net_stat, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.ip.statistics", 2, std::bind(&bus::ipt_req_ip_statistics, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.dev.auth", 2, std::bind(&bus::ipt_req_dev_auth, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.dev.time", 2, std::bind(&bus::ipt_req_dev_time, this, std::placeholders::_1)));
			vm_.async_run(cyng::register_function("ipt.req.transfer.pushdata", 7, std::bind(&bus::ipt_req_transfer_pushdata, this, std::placeholders::_1)));
		}

		void bus::start()
		{
			CYNG_LOG_TRACE(logger_, "start ipt network");
			do_read();
		}

		bool bus::is_online() const
		{
			return state_ == STATE_AUTHORIZED_;
		}

		void bus::do_read()
		{
			auto self(shared_from_this());
			socket_.async_read_some(boost::asio::buffer(buffer_),
				[this, self](boost::system::error_code ec, std::size_t bytes_transferred)
			{
				if (!ec)
				{
					//CYNG_LOG_TRACE(logger_, bytes_transferred << " bytes read");
					vm_.async_run(cyng::generate_invoke("log.msg.trace", "ipt client received", bytes_transferred, "bytes"));
					const auto buffer = parser_.read(buffer_.data(), buffer_.data() + bytes_transferred);

#ifdef SMF_IO_DEBUG
					cyng::io::hex_dump hd;
					std::stringstream ss;
					hd(ss, buffer.begin(), buffer.end());
					CYNG_LOG_TRACE(logger_, "ipt input dump \n" << ss.str());
#endif

					do_read();
				}
				else
				{
					CYNG_LOG_WARNING(logger_, "read <" << ec << ':' << ec.value() << ':' << ec.message() << '>');
					state_ = STATE_ERROR_;

					//
					//	slot [1] - go offline
					//
					mux_.send(task_, 1, cyng::tuple_t());

				}
			});

		}

		void bus::ipt_res_login(cyng::context& ctx, bool scrambled)
		{
			const cyng::vector_t frame = ctx.get_frame();

			//	[8d2c4721-7b0a-4ee4-ae25-63db3d5bc7bd,,30,]
			//CYNG_LOG_INFO(logger_, "ipt.res.login.public " << cyng::io::to_str(frame));

			//
			//	same for public and scrambled
			//
			const auto res = make_login_response(cyng::value_cast(frame.at(1), response_type(0)));
			const std::uint16_t watchdog = cyng::value_cast(frame.at(2), std::uint16_t(23));
			const std::string redirect = cyng::value_cast<std::string>(frame.at(3), "");

			if (scrambled)
			{
				ctx.run(cyng::generate_invoke("log.msg.trace", "ipt.res.login.scrambled", res.get_response_name()));
			}
			else
			{
				ctx.run(cyng::generate_invoke("log.msg.trace", "ipt.res.login.public", res.get_response_name()));
			}

			if (res.is_success())
			{
				ctx.run(cyng::generate_invoke("log.msg.info", "successful authorized"));
				state_ = STATE_AUTHORIZED_;

				//
				//	slot [0]
				//
				mux_.send(task_, 0, cyng::tuple_factory(watchdog, redirect));

			}
			else
			{
				state_ = STATE_ERROR_;

				//
				//	slot [1]
				//
				mux_.send(task_, 1, cyng::tuple_t());
			}
		}

		void bus::ipt_res_register_push_target(cyng::context& ctx)
		{
			//	[79ba1da7-67b3-4c4f-9e1f-c217f5ea25a6,2,2,0]
			//
			//	* session tag
			//	* sequence
			//	* result code
			//	* channel
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.res.register.push.target", frame));

			const response_type res = cyng::value_cast<response_type>(frame.at(2), 0);
			ctrl_res_register_target_policy::get_response_name(res);
			vm_.async_run(cyng::generate_invoke("log.msg.info", "client.res.register.push.target", ctrl_res_register_target_policy::get_response_name(res)));
			//mux_.send(task_, 2, cyng::tuple_factory(channel));

		}

		void bus::ipt_req_transmit_data(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.trace", "ipt.req.transmit.data", frame));
		}

		void bus::ipt_req_open_connection(cyng::context& ctx)
		{
			//	[ipt.req.open.connection,[ec2451b6-67df-4942-a801-9e13ec7a448c,1,1024]]
			//
			//	* session tag
			//	* ipt sequence 
			//	* number
			//
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.open.connection", frame));

			switch (state_)
			{
			case STATE_AUTHORIZED_:
				// forward to session
				mux_.send(task_, 2, cyng::tuple_factory(frame.at(1), frame.at(2)));
				break;
			case STATE_CONNECTED_:
				//	busy
				vm_.async_run(cyng::generate_invoke("res.open.connection", frame.at(1), static_cast<std::uint8_t>(ipt::tp_res_open_connection_policy::BUSY)));
				vm_.async_run(cyng::generate_invoke("stream.flush"));
				break;
			default:
				vm_.async_run(cyng::generate_invoke("res.open.connection", frame.at(1), static_cast<std::uint8_t>(ipt::tp_res_open_connection_policy::DIALUP_FAILED)));
				vm_.async_run(cyng::generate_invoke("stream.flush"));
				break;
			}
		}

		void bus::ipt_req_protocol_version(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.protocol.version", frame));
			vm_.async_run(cyng::generate_invoke("res.protocol.version", frame.at(1), static_cast<std::uint8_t>(1)));
			vm_.async_run(cyng::generate_invoke("stream.flush"));
		}

		void bus::ipt_req_software_version(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.software.version", frame));
			vm_.async_run(cyng::generate_invoke("res.software.version", frame.at(1), NODE_VERSION));
			vm_.async_run(cyng::generate_invoke("stream.flush"));
		}

		void bus::ipt_req_device_id(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.device.id", frame));
			vm_.async_run(cyng::generate_invoke("res.device.id", frame.at(1), "ipt:store"));
			vm_.async_run(cyng::generate_invoke("stream.flush"));
		}

		void bus::ipt_req_net_stat(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.net.stat", frame));
			vm_.async_run(cyng::generate_invoke("res.unknown.command", frame.at(1), static_cast<std::uint16_t>(code::APP_REQ_NETWORK_STATUS)));
			vm_.async_run(cyng::generate_invoke("stream.flush"));
		}

		void bus::ipt_req_ip_statistics(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.ip.statistics", frame));
			vm_.async_run(cyng::generate_invoke("res.unknown.command", frame.at(1), static_cast<std::uint16_t>(code::APP_REQ_IP_STATISTICS)));
			vm_.async_run(cyng::generate_invoke("stream.flush"));
		}

		void bus::ipt_req_dev_auth(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.device.auth", frame));
			//vm_.async_run(cyng::generate_invoke("res.device.auth", frame.at(1)));
			vm_.async_run(cyng::generate_invoke("res.unknown.command", frame.at(1), static_cast<std::uint16_t>(code::APP_REQ_DEVICE_AUTHENTIFICATION)));
			vm_.async_run(cyng::generate_invoke("stream.flush"));
		}

		void bus::ipt_req_dev_time(cyng::context& ctx)
		{
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.device.time", frame));
			vm_.async_run(cyng::generate_invoke("res.device.time", frame.at(1)));
			vm_.async_run(cyng::generate_invoke("stream.flush"));
		}

		void bus::ipt_req_transfer_pushdata(cyng::context& ctx)
		{
			//	[7aee3dff-c81b-414e-a5dc-4e3dbe4122f5,9,fb2a0137,a1e24bba,c1,0,1B1B1B1B010101017606363939373462006200726301017601080500153B0223B3063639393733080500153B0223B3010163C4F400]
			//
			//	* session tag
			//	* sequence
			//	* channel
			//	* source
			//	* status
			//	* block
			//	* data
			const cyng::vector_t frame = ctx.get_frame();
			vm_.async_run(cyng::generate_invoke("log.msg.debug", "ipt.req.transfer.pushdata", frame));
		}

		bus::shared_type bus_factory(cyng::async::mux& mux
			, cyng::logging::log_ptr logger
			, boost::uuids::uuid tag
			, scramble_key const& sk
			, std::size_t tsk)
		{
			return std::make_shared<bus>(mux, logger, tag, sk, tsk);
		}
	}
}
