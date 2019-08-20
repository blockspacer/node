/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Sylko Olzscher
 *
 */

#include <smf/mbus/aes.h>
#include <smf/sml/srv_id_io.h>
#include <algorithm>

namespace node
{
	namespace mbus
	{
		cyng::crypto::aes::iv_t build_initial_vector(cyng::buffer_t const& buffer, std::uint8_t access_nr)
		{
			std::array<std::uint8_t, 16> init_vec{ 0 };
			init_vec.fill(access_nr);
			if (node::sml::is_mbus_wired(buffer) || node::sml::is_mbus_radio(buffer)) {
				
				auto pos = buffer.begin();
				++pos;

				//
				//	M-field + A-field
				//
				std::copy(pos, buffer.end(), init_vec.begin());

				//
				//	8 * access_nr 
				//

			}
			return init_vec;
		}

		cyng::buffer_t build_initial_vector(std::array<std::uint8_t, 16> const& arr)
		{
			return cyng::buffer_t(arr.begin(), arr.end());
		}

	}
}


