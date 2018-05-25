/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#ifndef NODE_LIB_SML_MESSAGE_H
#define NODE_LIB_SML_MESSAGE_H

#include <smf/sml/defs.h>
#include <smf/sml/intrinsics/obis.h>
#include <cyng/intrinsics/sets.h>
#include <ostream>
#include <initializer_list>

namespace node
{
	namespace sml
	{
		cyng::tuple_t message(cyng::object trx
			, std::uint8_t groupNo
			, std::uint8_t abortCode
			, sml_messages_enum type
			, cyng::tuple_t value);

		/**
		 * @param codepage optional codepage
		 * @param client_id optional client ID
		 * @param req_file_id request file id
		 * @param server_id server ID
		 * @param ref_time optional reference timepoint (creating this message)
		 * @param version optional SML version
		 */
		cyng::tuple_t open_response(cyng::object codepage
			, cyng::object client_id
			, cyng::object req_file_id
			, cyng::object server_id
			, cyng::object ref_time
			, cyng::object version);

		/**
		 * @param signature optional global signature
		 */
		cyng::tuple_t close_response(cyng::object signature);

		/**
		 * @param server_id
		 * @param code parameter tree path
		 * @param params parameter tree
		 */
		cyng::tuple_t get_proc_parameter_response(cyng::object server_id
			, obis code
			, cyng::tuple_t params);

		/**
		 * Add SML trailer and tail
		 */
		cyng::buffer_t boxing(std::vector<cyng::buffer_t> const&);

		/*
		 * Parameter value and child list are mutual (I haven't seen it otherwise).
		 * @param value typically the result of make_value<T>(...)
		 */
		cyng::tuple_t empty_tree(obis code);
		cyng::tuple_t parameter_tree(obis, cyng::tuple_t value);
		cyng::tuple_t child_list_tree(obis, cyng::tuple_t value);
		cyng::tuple_t child_list_tree(obis, std::initializer_list<cyng::tuple_t> list);

	}
}
#endif