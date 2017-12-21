/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017 Sylko Olzscher 
 * 
 */ 
#ifndef NODE_HTTP_AUTH_H
#define NODE_HTTP_AUTH_H

#include <cyng/object.h>
#include <map>
#include <string>
// #include <boost/filesystem.hpp>
#include <boost/utility/string_view.hpp>

namespace node 
{
	struct auth
	{
		//	directory: /
		//	authType:
		//	user:
		//	pwd:
		std::string type_;
		std::string user_;
		std::string pwd_;
		std::string realm_;
		auth();
		auth(std::string const&, std::string const&, std::string const&, std::string const&);
		
		/*
		 * Concatenate the user name and the password with a colon and generate
		 * the base64 encoded string.
		 */
		std::string basic_credentials() const;
	};
	
	using auth_dirs = std::map<std::string, auth>;
// 	using auth_dirs = std::map<boost::filesystem::path, auth>;
	
	void init(cyng::object, auth_dirs&);
	
	std::pair<auth, bool> authorization_required(boost::string_view, auth_dirs const&);
	
	bool authorization_test(boost::string_view, auth const&);
}

#endif
