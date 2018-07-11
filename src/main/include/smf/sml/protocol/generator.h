/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_LIB_SML_GENERATOR_H
#define NODE_LIB_SML_GENERATOR_H

#include <smf/sml/defs.h>
#include <smf/sml/intrinsics/obis.h>
#include <cyng/intrinsics/sets.h>
#include <cyng/intrinsics/mac.h>
#include <cyng/store/table.h>
#include <random>
#include <chrono>

namespace node
{
	namespace sml
	{
		/**
		 * Generate SML transaction is with the following format:
		 * @code
		 * [7 random numbers] [1 number in ascending order].
		 * nnnnnnnnnnnnnnnnn-m
		 * @endcode
		 *
		 * The numbers as the ASCII codes from 48 up to 57.
		 * 
		 * In a former version the following format was used:
		 * @code
		 * [17 random numbers] ['-'] [1 number in ascending order].
		 * nnnnnnnnnnnnnnnnn-m
		 * @endcode
		 *
		 * This is to generate a series of continuous transaction IDs
		 * and after reshuffling to start a new series.
		 */
		class trx
		{
		public:
			trx();
			trx(trx const&);

			/**
			 *	generate new transaction id (reshuffling)
			 */
			void shuffle(std::size_t = 7);

			/**
			 * Increase internal transaction number. 
			 */
			trx& operator++();
			trx operator++(int);

			/**
			 *	@return value of last generated (shuffled) transaction id
			 *	as string.
			 */
			std::string operator*() const;


		private:
			/**
			 * Generate a random string
			 */
			std::random_device rng_;
			std::mt19937 gen_;

			/**
			 * The fixed part of a series
			 */
			std::string		value_;

			/**
			 * Ascending number
			 */
			std::uint16_t	num_;

		};

		/**
		 * Base class for all SML generators
		 */
		class generator
		{
		public:
			generator();

			/**
			 * @return a file id generated from current time stamp
			 */
			static std::string gen_file_id();

			/**
			 * Clear message buffer
			 */
			void reset();

			/**
			 * Produce complete SML message from message buffer
			 */
			cyng::buffer_t boxing() const;

		//protected:
			std::size_t append_msg(cyng::tuple_t&&);

		protected:
			/**
			 * buffer for current SML message
			 */
			std::vector<cyng::buffer_t>	msg_;
			std::uint8_t	group_no_;

		};

		/**
		 * Generate SML requests
		 */
		class req_generator : public generator
		{
		public:
			req_generator();

			std::size_t public_open(cyng::mac48 client_id
				, cyng::buffer_t const& server_id
				, std::string const& name
				, std::string const& pwd);

			std::size_t public_close();

			/**
			 * Restart system - 81 81 C7 83 82 01
			 */
			std::size_t set_proc_parameter_restart(cyng::buffer_t const& server_id
				, std::string const& username
				, std::string const& password);

		private:
			trx	trx_;
		};

		/**
		 * Generate SML responses
		 */
		class res_generator : public generator
		{
		public:
			res_generator();

			std::size_t public_open(cyng::object trx
				, cyng::object	//	client id
				, cyng::object	//	req file id
				, cyng::object	//	server id
			);

			std::size_t public_close(cyng::object trx);

			/**
			 * Generate an empty response with the specified tree path
			 */
			std::size_t empty(cyng::object trx
				, cyng::object server_id
				, obis);

			/**
			 * OBIS_CLASS_OP_LOG_STATUS_WORD
			 */
			std::size_t get_proc_parameter_status_word(cyng::object trx
				, cyng::object server_id
				, std::uint64_t);

			/**
			 * OBIS_CODE_ROOT_DEVICE_IDENT - 81 81 C7 82 01 FF
			 */
			std::size_t get_proc_parameter_device_id(cyng::object trx
				, cyng::object server_id
				, std::string const& manufacturer
				, cyng::buffer_t const& server_id2
				, std::string const& model_code
				, std::string const& serial);

			/**
			 * OBIS_CODE_ROOT_MEMORY_USAGE - 00 80 80 00 10 FF
			 */
			std::size_t get_proc_mem_usage(cyng::object trx
				, cyng::object server_id
				, std::uint8_t
				, std::uint8_t);

			std::size_t get_proc_device_time(cyng::object trx
				, cyng::object server_id
				, std::chrono::system_clock::time_point
				, std::int32_t
				, bool);

			std::size_t get_proc_active_devices(cyng::object trx
				, cyng::object server_id
				, const cyng::store::table*);

			std::size_t get_proc_visible_devices(cyng::object trx
				, cyng::object server_id
				, const cyng::store::table*);

			std::size_t get_proc_sensor_property(cyng::object trx
				, cyng::object server_id
				, const cyng::table::record&);

			std::size_t get_proc_push_ops(cyng::object trx
				, cyng::object server_id
				, const cyng::store::table*);

		};

	}
}
#endif