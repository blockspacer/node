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
#include "sml_reader.h"
#include <smf/sml/protocol/generator.h>

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
				, node::ipt::master_config_t const& cfg
				, bool
				, std::string account
				, std::string pwd
				, std::string manufacturer
				, std::string model
				, std::uint32_t serial
				, cyng::mac48 mac);

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
			void sml_get_proc_parameter_request(cyng::context& ctx);
			void sml_get_proc_ntp_config(cyng::object trx, cyng::object server);
			//void sml_get_proc_device_time(cyng::context& ctx);
			void sml_get_proc_ipt_state(cyng::object trx, cyng::object server);
			void sml_get_proc_data_collector(cyng::object trx, cyng::object server);
			//void sml_get_proc_0080800000FF(cyng::object trx, cyng::object server);

			void sml_set_proc_push_interval(cyng::context& ctx);
			void sml_set_proc_push_delay(cyng::context& ctx);
			void sml_set_proc_push_name(cyng::context& ctx);

			void sml_set_proc_ipt_param_address(cyng::context& ctx);
			void sml_set_proc_ipt_param_port_target(cyng::context& ctx);
			void sml_set_proc_ipt_param_user(cyng::context& ctx);
			void sml_set_proc_ipt_param_pwd(cyng::context& ctx);

		public:
			/**
			* Global status word
			*/
			status&	status_word_;

		private:
			cyng::logging::log_ptr logger_;

			/**
			* configuration db
			*/
			cyng::store::db& config_db_;
			node::ipt::master_config_t const& cfg_ipt_;

			const bool server_mode_;
			const std::string account_;
			const std::string pwd_;

			//
			//	hardware
			//
			const std::string manufacturer_;
			const std::string model_;
			const std::uint32_t serial_;
			const cyng::buffer_t server_id_;

			/**
			* read SML tree and generate commands
			*/
			sml_reader reader_;

			/**
			 * buffer for current SML message
			 */
			res_generator sml_gen_;

		};

	}	//	sml
}

#endif