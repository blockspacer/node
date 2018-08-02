/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_SML_EXPORTER_CSV_H
#define NODE_SML_EXPORTER_CSV_H


#include <smf/sml/defs.h>
#include <smf/sml/intrinsics/obis.h>
#include <smf/sml/units.h>
#include <smf/sml/intrinsics/obis.h>
#include <cyng/intrinsics/sets.h>
#include <cyng/object.h>
#include <fstream>
#include <boost/uuid/random_generator.hpp>
#include <boost/filesystem.hpp>

namespace node
{
	namespace sml
	{
		/**
		 * To build a usefull filename constructor with source,
		 * channel and target info is required,
		 */
		boost::filesystem::path get_filename(std::string prefix
			, std::string suffix
			, std::uint32_t source
			, std::uint32_t channel
			, std::string const& target);

		/**
		 * walk down SML message body recursively, collect data
		 * and write data into SQL database.
		 */
		class csv_exporter
		{
			struct readout
			{
				readout(boost::uuids::uuid);

				void reset(boost::uuids::uuid, std::size_t);

				readout& set_trx(cyng::buffer_t const&);
				readout& set_index(std::size_t);
				readout& set_value(std::string const&, cyng::object);
				cyng::object get_value(std::string const&) const;

				boost::uuids::uuid pk_;
				std::size_t idx_;	//!< message index
				std::string trx_;
				cyng::param_map_t values_;
			};

		public:
			csv_exporter(boost::filesystem::path root_dir
				, std::string prefix
				, std::string suffix
				, bool header);
			csv_exporter(boost::filesystem::path root_dir
				, std::string prefix
				, std::string suffix
				, bool header
				, std::uint32_t source
				, std::uint32_t channel
				, std::string const& target);
			virtual ~csv_exporter();


			/**
			 * reset exporter
			 */
			void reset();

			void write(cyng::tuple_t const&, std::size_t idx);

		private:
			/**
			 * read SML message.
			 */
			void read_msg(cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator, std::size_t idx);
			void read_body(cyng::object, cyng::object);
			void read_public_open_request(cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);
			void read_public_open_response(cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);
			void read_get_profile_list_response(cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);
			void read_get_proc_parameter_response(cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);
			void read_attention_response(cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);

			void read_time(std::string const&, cyng::object);
			std::vector<obis> read_param_tree_path(cyng::object);
			obis read_obis(cyng::object);
			std::uint8_t read_unit(std::string const&, cyng::object);
			std::int8_t read_scaler(cyng::object);
			std::string read_string(std::string const&, cyng::object);

			/**
			 * @return CYNG data type tag
			 */
			std::size_t read_value(obis, std::int8_t, std::uint8_t, cyng::object);
			void read_parameter(obis, cyng::object);
			std::string read_server_id(cyng::object);
			std::string read_client_id(cyng::object);

			void read_period_list(std::vector<obis> const&, cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);
			void read_period_entry(std::vector<obis> const&, std::size_t, cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);
			void read_param_tree(std::size_t, cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);

			/**
			 * Write optional CSV header
			 */
			void write_header();

			//
			//	database functions
			//
			//void store_meta(boost::uuids::uuid pk
			//	, std::string const& trx
			//	, std::size_t idx
			//	, cyng::object ro_ime
			//	, cyng::object act_ime
			//	, cyng::object val_time
			//	, cyng::object client		//	gateway
			//	, cyng::object client_id	//	gateway - formatted
			//	, cyng::object server		//	server
			//	, cyng::object server_id	//	server - formatted
			//	, cyng::object status);

			//void store_data(boost::uuids::uuid pk
			//	, std::string const& trx
			//	, std::size_t idx
			//	, obis const& code
			//	, std::uint8_t unit
			//	, std::string unit_name
			//	, std::string type_name	//	CYNG data type name
			//	, std::int8_t scaler	//	scaler
			//	, cyng::object raw		//	raw value
			//	, cyng::object value);

		private:
			const boost::filesystem::path root_dir_;
			const std::string prefix_;
			const std::string suffix_;
			bool header_;
			const std::uint32_t source_;
			const std::uint32_t channel_;
			const std::string target_;
			boost::uuids::random_generator rgn_;
			readout ro_;
			std::ofstream	ofstream_;
		};

	}	//	sml
}

#endif
