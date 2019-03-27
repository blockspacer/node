/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2018 Sylko Olzscher 
 * 
 */ 


#include "cluster.h"
#include "session.h"
#include "db.h"
#include <NODE_project_info.h>
#include <smf/cluster/generator.h>

#include <cyng/tuple_cast.hpp>
#include <cyng/parser/buffer_parser.h>
#include <cyng/dom/algorithm.h>
#include <cyng/io/io_buffer.h>

#include <boost/uuid/nil_generator.hpp>

#ifdef __GNUC__
#  include <features.h>
#  if __GNUC_PREREQ(6,0)
#define SMF_AUTO_TUPLE_ENABLED
#  else
#define SMF_AUTO_TUPLE_DISABLED
#  endif
#else
 //    If not gcc
#define SMF_AUTO_TUPLE_ENABLED
#endif

namespace node 
{
	cluster::cluster(cyng::async::mux& mux
		, cyng::logging::log_ptr logger
		, cyng::store::db& db
		, std::atomic<std::uint64_t>& global_configuration)
	: mux_(mux)
		, logger_(logger)
		, db_(db)
		, global_configuration_(global_configuration)
		, uidgen_()
	{}

	void cluster::register_this(cyng::context& ctx)
	{
		ctx.queue(cyng::register_function("bus.req.gateway.proxy", 7, std::bind(&cluster::bus_req_com_sml, this, std::placeholders::_1)));
		ctx.queue(cyng::register_function("bus.res.gateway.proxy", 9, std::bind(&cluster::bus_res_com_sml, this, std::placeholders::_1)));
		ctx.queue(cyng::register_function("bus.res.attention.code", 7, std::bind(&cluster::bus_res_attention_code, this, std::placeholders::_1)));
		ctx.queue(cyng::register_function("bus.req.com.task", 7, std::bind(&cluster::bus_req_com_task, this, std::placeholders::_1)));
		ctx.queue(cyng::register_function("bus.req.com.node", 7, std::bind(&cluster::bus_req_com_node, this, std::placeholders::_1)));
	}

	void cluster::bus_req_com_sml(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		//CYNG_LOG_INFO(logger_, "bus.req.gateway.proxy " << cyng::io::to_str(frame));
		
		//	[9f773865-e4af-489a-8824-8f78a2311278,8,[430a586d-e96d-477d-a3fe-9b9a6120442a],f21b82e8-a1bd-48c0-aa6e-84f24cfb4efa,get.list.request,[current-data-record],[{("meterId":01-e61e-29436587-bf-03)}]]
		//
		//	* source
		//	* bus sequence
		//	* TGateway/TDevice key
		//	* web-socket tag
		//	* channel
		//	* sections
		//	* params

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] source
			std::uint64_t,			//	[1] sequence
			cyng::vector_t,			//	[2] TGateway/TDevice key
			boost::uuids::uuid,		//	[3] websocket tag
			std::string,			//	[4] channel
			cyng::vector_t,			//	[5] sections (strings)
			cyng::vector_t			//	[6] requests (strings)
		>(frame);

		//
		//	test TGateway key
		//
		BOOST_ASSERT(!std::get<2>(tpl).empty());
		
		//
		//	send process parameter request to an gateway
		//
		db_.access([&](const cyng::store::table* tbl_session, const cyng::store::table* tbl_gw)->void {

			//
			//	Since TGateway and TDevice share the same primary key, we can search for a session 
			//	of that device/gateway. Additionally we get the required access data like
			//	server ID, user and password.
			//
#if defined SMF_AUTO_TUPLE_ENABLED
			auto const[peer, rec, server, tag] = find_peer(std::get<2>(tpl), tbl_session, tbl_gw);
#else            
            std::tuple<session const*, cyng::table::record, cyng::buffer_t, boost::uuids::uuid> r = find_peer(std::get<2>(tpl), tbl_session, tbl_gw);
            session const* peer = std::get<0>(r);
            cyng::table::record const rec = std::get<1>(r);
            cyng::buffer_t const server = std::get<2>(r);
            boost::uuids::uuid const tag =  std::get<3>(r);
#endif

			if (peer != nullptr && !rec.empty()) {

				CYNG_LOG_INFO(logger_, "bus.req.gateway.proxy " << cyng::io::to_str(frame));

				const auto name = cyng::value_cast<std::string>(rec["userName"], "operator");
				const auto pwd = cyng::value_cast<std::string>(rec["userPwd"], "operator");

				peer->vm_.async_run(node::client_req_gateway_proxy(tag	//	ipt session tag
					, std::get<0>(tpl)	//	source tag
					, std::get<1>(tpl)	//	cluster sequence
					, std::get<2>(tpl)	//	TGateway key
					, std::get<3>(tpl)	//	websocket tag
					, std::get<4>(tpl)	//	channel
					, std::get<5>(tpl)	//	sections
					, std::get<6>(tpl)	//	requests
					, server			//	server
					, name
					, pwd));
			}
			else {

				//
				//	peer/node not found
				//
				CYNG_LOG_WARNING(logger_, "bus.req.gateway.proxy - peer/node not found " 
					<< cyng::io::to_str(std::get<2>(tpl))
					<< ", channel: "
					<< std::get<4>(tpl));
			}

		}	, cyng::store::read_access("_Session")
			, cyng::store::read_access("TGateway"));

	}

	void cluster::bus_res_com_sml(cyng::context& ctx)
	{
		//
		//	[21c95367-9d92-40d7-ba79-f2eee35dddc2,9f773865-e4af-489a-8824-8f78a2311278,23,[ec563e58-f0d6-4d6a-8a13-63f639f6c9a7],dd44ad9e-8995-48f0-81aa-eec454cf0625,
		//	get.proc.param,00:15:3b:02:23:b3,root-active-devices,
		//	%(("class":---),("ident":0ac00001),("maker":),("meter":00000000),("meterId":0AC00001),("number":0005),("timestamp":2014-11-25 15:41:29.00000000),("type":0000000a))]
		//
		//	* [uuid] ident tag
		//	* [uuid] source tag
		//	* [u64] cluster seq
		//	* [vec] gateway key
		//	* [uuid] web-socket tag
		//	* [str] channel
		//	* [str] server id
		//	* [str] "OBIS code" as text
		//	* [param_map_t] params
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, "bus.res.gateway.proxy " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] ident
			boost::uuids::uuid,		//	[1] source
			std::uint64_t,			//	[2] sequence
			cyng::vector_t,			//	[3] gw key
			boost::uuids::uuid,		//	[4] websocket tag
			std::string,			//	[5] channel
			std::string,			//	[6] server id
			std::string,			//	[7] "OBIS code" as text
			cyng::param_map_t		//	[8] params
		>(frame);

		//
		//	test TGateway key
		//
		BOOST_ASSERT_MSG(!std::get<3>(tpl).empty(), "no TGateway key");

		//
		//	routing back
		//
		db_.access([&](const cyng::store::table* tbl_cluster)->void {

			auto rec = tbl_cluster->lookup(cyng::table::key_generator(std::get<1>(tpl)));
			if (!rec.empty()) {
				auto peer = cyng::object_cast<session>(rec["self"]);
				if (peer != nullptr) {

					CYNG_LOG_INFO(logger_, "bus.res.gateway.proxy - send to peer "
						<< cyng::value_cast<std::string>(rec["class"], "")
						<< '@'
						<< peer->vm_.tag());

					//
					//	forward data
					//
					peer->vm_.async_run(node::bus_res_com_sml(
						  std::get<0>(tpl)		//	ident tag
						, std::get<1>(tpl)		//	source tag
						, std::get<2>(tpl)		//	cluster sequence
						, std::get<3>(tpl)		//	TGateway key
						, std::get<4>(tpl)		//	websocket tag
						, std::get<5>(tpl)		//	channel
						, std::get<6>(tpl)		//	server id
						, std::get<7>(tpl)		//	section part
						, std::get<8>(tpl)));	//	params						
				}
			}
			else {
				CYNG_LOG_WARNING(logger_, "bus.res.query.gateway - peer not found " << std::get<1>(tpl));
			}
		}, cyng::store::read_access("_Cluster"));


		//
		//	update specific data in TGateway and TMeter table
		//
		if (boost::algorithm::equals(std::get<7>(tpl), "root-active-devices")) {

			//
			//	Use the incoming active meter devices to populate TMeter table
			//

			//	%(("class":---),("ident":01-e61e-13090016-3c-07),("number":0009),("timestamp":2018-11-07 12:04:46.00000000),("type":00000000))
			const auto ident = cyng::find_value<std::string>(std::get<8>(tpl), "ident", "");
			const auto meter = cyng::find_value<std::string>(std::get<8>(tpl), "meter", ident);
			const auto maker = cyng::find_value<std::string>(std::get<8>(tpl), "maker", "");
			const auto class_name = cyng::find_value<std::string>(std::get<8>(tpl), "class", "A");
			const auto type = cyng::find_value<std::uint32_t>(std::get<8>(tpl), "type", 0u);
			const auto age = cyng::find_value(std::get<8>(tpl), "timestamp", std::chrono::system_clock::now());

			//
			//	preconditions are
			//
			//	* configuration flag for auto insert meters is true
			//	* meter type is SRV_MBUS or SRV_SERIAL - see node::sml::get_srv_type()
			//	* TGateway key has correct size (and is not empty)
			//
			//	Additionally:
			//	* meters from gateways are unique by meter number ("meter") AND gateway (gw)
			//
			if (is_catch_meters(global_configuration_)	//	auto insert meters is true
				&& (type < 2)	//	 meter type is SRV_MBUS or SRV_SERIAL
				&& std::get<3>(tpl).size() == 1)	//	TGateway key has correct size (and is not empty)
			{
				CYNG_LOG_TRACE(logger_, "search for "
					<< ident 
					<< " in TMeter table with " 
					<< db_.size("TMeter") 
					<< " record(s)");

				//
				//	get the tag from the TGateway table key
				//
				auto const gw_tag = cyng::value_cast(std::get<3>(tpl).at(0), boost::uuids::nil_uuid());

				db_.access([&](cyng::store::table* tbl_meter, cyng::store::table const* tbl_cfg)->void {
					bool found{ false };
					tbl_meter->loop([&](cyng::table::record const& rec) -> bool {

						const auto rec_ident = cyng::value_cast<std::string>(rec["ident"], "");
						//CYNG_LOG_DEBUG(logger_, "compare " << ident << " / " << rec_ident);
						if (boost::algorithm::equals(ident, rec_ident)) {
							//	abort loop if the same gw key is used
							const auto meter_gw_tag = cyng::value_cast(rec["gw"], boost::uuids::nil_uuid());
							CYNG_LOG_DEBUG(logger_, ident << " compare " << meter_gw_tag << " / " << gw_tag);
							found = (meter_gw_tag == gw_tag);
						}

						//	continue loop
						return !found;
					});

					//
					//	insert meter if not found
					//
					if (!found) {
						const auto tag = uidgen_();

						//
						//	generate meter code
						//
						std::stringstream ss;
						ss
							<< cyng::value_cast<std::string>(get_config(tbl_cfg, "country-code"), "AU")
							<< std::setfill('0')
							//<< std::setw(11)
							<< 0	//	1
							<< 0	//	2
							<< 0	//	3
							<< 0	//	4
							<< 0	//	5
							<< +tag.data[15]	//	6
							<< +tag.data[14]	//	7
							<< +tag.data[13]	//	8
							<< +tag.data[12]	//	9
							<< +tag.data[11]	//	10
							<< +tag.data[10]	//	11
							<< std::setw(20)
							<< meter
							;
						const auto mc = ss.str();
						CYNG_LOG_INFO(logger_, "insert TMeter " << tag << " : " << meter << " mc: " << mc);

						tbl_meter->insert(cyng::table::key_generator(tag)
							, cyng::table::data_generator(ident, meter, mc, maker, age, "", "", "", "", class_name, std::get<3>(tpl).front())
							, 0
							, std::get<1>(tpl));
					}
				}	, cyng::store::write_access("TMeter")
					, cyng::store::read_access("_Config"));
			}
		}
		else if (boost::algorithm::equals(std::get<4>(tpl), "root-wMBus-status")) {

			//
			//	update TGateway table
			//
			//	"id" contains the "mbus" ID
			cyng::buffer_t tmp;
			const auto id = cyng::find_value(std::get<8>(tpl), "id", tmp);
			CYNG_LOG_INFO(logger_, "Update M-Bus ID: " << cyng::io::to_hex(id));

			db_.access([&](cyng::store::table* tbl_gw)->void {
				//	update "mbus" W-Mbus ID
				tbl_gw->modify(std::get<3>(tpl), cyng::param_factory("mbus", id), std::get<1>(tpl));
			}, cyng::store::write_access("TGateway"));
		}
	}

	void cluster::bus_res_attention_code(cyng::context& ctx)
	{
		//	[da673931-9743-41b9-8a46-6ce946c9fa6c,9f773865-e4af-489a-8824-8f78a2311278,4,5c200bdf-22c0-41bd-bc93-d879d935889e,00:15:3b:02:29:81,8181C7C7FE03,NO SERVER ID]
		//
		//	* [uuid] ident
		//	* [uuid] source
		//	* [u64] cluster seq
		//	* [uuid] ws tag
		//	* [string] server id
		//	* [buffer] attention code
		//	* [string] message
		//
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, ctx.get_name() << " - " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] ident
			boost::uuids::uuid,		//	[1] source
			std::uint64_t,			//	[2] sequence
			boost::uuids::uuid,		//	[3] ws tag
			std::string,			//	[4] server id
			cyng::buffer_t,			//	[5] attention code (OBIS)
			std::string				//	[6] msg
		>(frame);

		//
		//	emit system message
		//
		insert_msg(db_
			, cyng::logging::severity::LEVEL_INFO
			, ("attention message from " + std::get<4>(tpl) + ": " + std::get<6>(tpl))
			, std::get<1>(tpl));

		//
		//	routing back - forward to receiver
		//
		db_.access([&](const cyng::store::table* tbl_cluster)->void {

			auto rec = tbl_cluster->lookup(cyng::table::key_generator(std::get<1>(tpl)));
			if (!rec.empty()) {
				auto peer = cyng::object_cast<session>(rec["self"]);
				if (peer != nullptr) {

					CYNG_LOG_INFO(logger_, "bus.res.attention.code - send to peer "
						<< cyng::value_cast<std::string>(rec["class"], "")
						<< '@'
						<< peer->vm_.tag());

					//
					//	forward data
					//
					peer->vm_.async_run(node::bus_res_attention_code(
						std::get<0>(tpl)		//	ident tag
						, std::get<1>(tpl)		//	source tag
						, std::get<2>(tpl)		//	cluster sequence
						, std::get<3>(tpl)		//	websocket tag
						, std::get<4>(tpl)		//	server id
						, std::get<5>(tpl)		//	attention code (OBIS)
						, std::get<6>(tpl)));	//	msg
				}
			}
			else {
				CYNG_LOG_WARNING(logger_, "bus.res.attention.code - peer not found " << std::get<1>(tpl));
			}
		}, cyng::store::read_access("_Cluster"));

	}

	void cluster::bus_req_com_task(cyng::context& ctx)
	{
		//	[9f773865-e4af-489a-8824-8f78a2311278,3,7a7f89be-5440-4e57-94f1-88483c022d19,09be403a-aa78-4946-9cc0-0f510441717c,req.config,[monitor,influxdb,lineProtocol,db,multiple,single],[tsdb]]
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, ctx.get_name() << " - " << cyng::io::to_str(frame));

		auto const tpl = cyng::tuple_cast<
			boost::uuids::uuid,		//	[0] ident
			std::uint64_t,			//	[1] cluster sequence
			boost::uuids::uuid,		//	[2] task key
			boost::uuids::uuid,		//	[3] source (web socket)
			std::string,			//	[4] channel
			cyng::vector_t,			//	[5] sections
			cyng::vector_t			//	[6] params
		>(frame);

		//
		//	ToDo: search session and forward request
		//
	}

	void cluster::bus_req_com_node(cyng::context& ctx)
	{
		const cyng::vector_t frame = ctx.get_frame();
		CYNG_LOG_INFO(logger_, ctx.get_name() << " - " << cyng::io::to_str(frame));

		//
		//	ToDo: search session and forward request
		//
	}


	std::tuple<session const*, cyng::table::record, cyng::buffer_t, boost::uuids::uuid> cluster::find_peer(cyng::table::key_type const& key_gw
		, const cyng::store::table* tbl_session
		, const cyng::store::table* tbl_gw)
	{
		//
		//	get gateway record
		//
		auto rec_gw = tbl_gw->lookup(key_gw);

		//
		//	gateway and (associated) device share the same key
		//
		auto rec_session = tbl_session->find_first(cyng::param_t("device", key_gw.at(0)));
		if (!rec_gw.empty() && !rec_session.empty())
		{
			auto peer = cyng::object_cast<session>(rec_session["local"]);
			if (peer && (key_gw.size() == 1))
			{
				//
				//	get remote session tag
				//
				auto tag = cyng::value_cast(rec_session.key().at(0), boost::uuids::nil_uuid());
				//BOOST_ASSERT(tag == peer->vm_.tag());

				//
				//	get login parameters
				//
				const auto server = cyng::value_cast<std::string>(rec_gw["serverId"], "05000000000000");
				const auto name = cyng::value_cast<std::string>(rec_gw["userName"], "");
				const auto pwd = cyng::value_cast<std::string>(rec_gw["userPwd"], "");

				const auto id = cyng::parse_hex_string(server);
				if (id.second)
				{
					CYNG_LOG_TRACE(logger_, "gateway "
						<< cyng::io::to_str(rec_session["name"])
						<< ": "
						<< server
						<< ", user: "
						<< name
						<< ", pwd: "
						<< pwd
						<< " has peer: "
						<< peer->vm_.tag());

					return std::make_tuple(peer, rec_gw, id.first, tag);
				}
				else
				{
					CYNG_LOG_WARNING(logger_, "client "
						<< tag
						<< " has an invalid server id: "
						<< server);
				}
			}
		}

		//
		//	not found - peer is a null pointer
		//
		return std::make_tuple(nullptr, rec_gw, cyng::buffer_t{}, boost::uuids::nil_uuid());
	}


}
