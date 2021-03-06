/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 

#include <smf/http/srv/auth.h>
#include <cyng/dom/reader.h>
#include <cyng/io/serializer.h>
#include <cyng/value_cast.hpp>
#include <cyng/crypto/base64.h>
#include <cyng/util/split.h>
#include <boost/assert.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace node 
{
	void init(cyng::object obj, auth_dirs& result)
	{
		cyng::vector_t cfg;
		cfg = cyng::value_cast(obj, cfg);
		for (auto d : cfg)
		{
			auto dom = cyng::make_reader(d);

 			result.emplace(std::piecewise_construct,
              std::forward_as_tuple(cyng::io::to_str(dom.get("directory"))),
              std::forward_as_tuple(cyng::io::to_str(dom.get("authType"))
			  , cyng::io::to_str(dom.get("name"))
			  , cyng::io::to_str(dom.get("pwd"))
			  , cyng::io::to_str(dom.get("realm"))
			));
		}
	}	
	
	std::pair<auth, bool> authorization_required(boost::string_view path, auth_dirs const& ad)
	{
		for (auto dir : ad)
		{
			boost::string_view cmp(dir.first.c_str(), dir.first.size());
 			if (boost::algorithm::starts_with(path, cmp))	return std::make_pair(dir.second, true);
		}
		return std::make_pair(auth(), false);
	}
	
	bool authorization_test(boost::string_view value, auth const& ident)
	{
		std::vector<boost::string_view> parts = cyng::split(value, "\t ");
		BOOST_ASSERT_MSG(parts.size() == 2, "invalid authorization field");
		if (parts.size() == 2 && (parts.at(0) == "Basic"))
		{
			return ident.basic_credentials() == parts.at(1);
		}
		return false;
	}
	
	auth::auth()
	: type_()
	, user_()
	, pwd_()
	, realm_()
	{}
	
	auth::auth(std::string const& type, std::string const& user, std::string const& pwd, std::string const& realm)
	: type_(type)
	, user_(user)
	, pwd_(pwd)
	, realm_(realm)
	{}

	std::string auth::basic_credentials() const
	{
		return cyng::crypto::base64_encode(user_ + ":" + pwd_);
	}
	
}

