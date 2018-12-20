/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_SML_EXPORTER_ABL_SML_H
#define NODE_SML_EXPORTER_ABL_SML_H


#include <smf/sml/defs.h>
#include <smf/sml/units.h>
#include <smf/sml/protocol/readout.h>
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
		boost::filesystem::path get_abl_filename(std::string prefix
			, std::string suffix
			, std::string gw
			, std::string server_id);

		std::string get_manufacturer(cyng::buffer_t const&);

		/**
		 * walk down SML message body recursively, collect data
		 * and write data into SQL database.
		 */
		class abl_exporter
		{
		public:
			abl_exporter(boost::filesystem::path root_dir
				, std::string prefix
				, std::string suffix
				, bool);
			abl_exporter(boost::filesystem::path root_dir
				, std::string prefix
				, std::string suffix
				, bool
				, std::uint32_t source
				, std::uint32_t channel
				, std::string const& target);
			virtual ~abl_exporter();


			/**
			 * reset exporter
			 */
			void reset();

			/**
			 * read incoming input 
			 */
			void read(cyng::tuple_t const&, std::size_t idx);

			/**
			 * Write ABL file
			 */
			//bool write(boost::filesystem::path const&);

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

			void read_period_list(std::ofstream&, std::vector<obis> const&, cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);
			void read_period_entry(std::ofstream&, std::vector<obis> const&, std::size_t, cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);
			void read_param_tree(std::size_t, cyng::tuple_t::const_iterator, cyng::tuple_t::const_iterator);


		private:
			const boost::filesystem::path root_dir_;
			const std::string prefix_;
			const std::string suffix_;
			const std::string eol_;	//	true is DOS, otherwise UNIX
			const std::uint32_t source_;
			const std::uint32_t channel_;
			const std::string target_;
			boost::uuids::random_generator rgn_;
			readout ro_;
		};

	}	//	sml
}

#endif
