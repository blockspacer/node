/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_SML_KERNEL_H
#define NODE_SML_KERNEL_H


#include <smf/sml/defs.h>
#include <smf/sml/status.h>
#include <smf/ipt/config.h>
#include <smf/sml/protocol/reader.h>
#include <smf/sml/protocol/generator.h>
#include "data.h"
#include "attention.h"
#include "cfg_ipt.h"
#include "cfg_push.h"

#include <cyng/log.h>
#include <cyng/vm/controller.h>
#include <cyng/store/db.h>

namespace node
{
	namespace sml
	{
		/**
		 * Provide core functions of an SML gateway
		 */
		class kernel
		{
		public:
			kernel(cyng::logging::log_ptr
				, cyng::controller&
				, status&
				, cyng::store::db& config_db
				, node::ipt::redundancy const& cfg
				, bool
				, std::string account
				, std::string pwd
				, std::string manufacturer
				, std::string model
				, std::uint32_t serial
				, cyng::mac48 mac
				, bool accept_all);

			/**
			 * reset kernel
			 */
			void reset();

		private:
			//void append_msg(cyng::tuple_t&&);

			void sml_msg(cyng::context& ctx);
			void sml_eom(cyng::context& ctx);

			void sml_public_open_request(cyng::context& ctx);
			void sml_public_close_request(cyng::context& ctx);
			void sml_public_close_response(cyng::context& ctx);
			void sml_get_proc_parameter_request(cyng::context& ctx);
			void sml_get_profile_list_request(cyng::context& ctx);
			void sml_get_proc_ntp_config(cyng::object trx, cyng::object server);
			//void sml_get_proc_device_time(cyng::context& ctx);
			void sml_get_proc_ipt_state(cyng::object trx, cyng::object server);
			void sml_get_proc_data_collector(cyng::object trx, cyng::object server);
			//void sml_get_proc_0080800000FF(cyng::object trx, cyng::object server);


			void sml_set_proc_activate(cyng::context& ctx);
			void sml_set_proc_deactivate(cyng::context& ctx);
			void sml_set_proc_delete(cyng::context& ctx);

			void sml_set_proc_if1107_param(cyng::context& ctx);
			void sml_set_proc_if1107_device(cyng::context& ctx);

			void sml_set_proc_mbus_param(cyng::context& ctx);
			void sml_set_proc_sensor(cyng::context& ctx);
			//void sml_set_proc_mbus_install_mode(cyng::context& ctx);
			//void sml_set_proc_mbus_smode(cyng::context& ctx);
			//void sml_set_proc_mbus_tmode(cyng::context& ctx);
			//void sml_set_proc_mbus_protocol(cyng::context& ctx);


		public:
			/**
			 * global state
			 */
			status&	status_word_;

		private:
			cyng::logging::log_ptr logger_;

			/**
			 * configuration db
			 */
			cyng::store::db& config_db_;

			bool const server_mode_;
			std::string const account_;
			std::string const pwd_;

			//
			//	hardware
			//
			std::string const manufacturer_;
			std::string const model_;
			std::uint32_t const serial_;
			cyng::buffer_t const server_id_;
			bool const accept_all_;

			/**
			 * read SML tree and generate commands
			 */
			reader reader_;

			/**
			 * buffer for current SML message
			 */
			res_generator sml_gen_;

			/**
			 * process incoming data
			 */
			data data_;
			attention attention_;
			cfg_ipt ipt_;
			cfg_push push_;
		};

	}	//	sml
}

#endif
