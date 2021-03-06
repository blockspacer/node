/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017 Sylko Olzscher 
 * 
 */ 

#include <boost/asio.hpp>
#include "show_ip_address.h"
#include <cyng/sys/mac.h>
#include <cyng/sys/info.h>
#include <cyng/io/serializer.h>

namespace node 
{
	int show_ip_address(std::ostream& os)
	{
		const std::string host = boost::asio::ip::host_name();
		os
			<< "host name: "
			<< host
			<< std::endl
			<< "effective OS: "
			<< cyng::sys::get_os_name()
			<< std::endl
			;


		try
		{
			boost::asio::io_service io_service;
			boost::asio::ip::tcp::resolver resolver(io_service);
			boost::asio::ip::tcp::resolver::query query(host, "");
			boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
			boost::asio::ip::tcp::resolver::iterator end; // End marker.
			while (iter != end)
			{
				boost::asio::ip::tcp::endpoint ep = *iter++;
				os
					<< (ep.address().is_v4() ? "IPv4: " : "IPv6: ")
					<< ep
					<< std::endl
					;
			}

			auto macs = cyng::sys::retrieve_mac48();
			if (!macs.empty())
			{
				std::cout << macs.size() << " physical address(es)" << std::endl;
				for (auto const& m : macs) {
					using cyng::io::operator<<;
					std::cout
						<< m
						<< std::endl;
				}
			}

			return EXIT_SUCCESS;
		}
		catch (std::exception const& ex)
		{
			os
				<< "***Error: "
				<< ex.what()
				<< std::endl
				;
		}
		return EXIT_FAILURE;
	}
}

