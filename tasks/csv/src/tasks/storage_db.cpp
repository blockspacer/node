/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include "storage_db.h"
#include "clock.h"
#include <NODE_project_info.h>
#include <cyng/async/task/task_builder.hpp>
#include <cyng/dom/reader.h>
#include <cyng/io/serializer.h>
#include <cyng/io/io_chrono.hpp>
#include <cyng/db/connection_types.h>
#include <cyng/db/session.h>
#include <cyng/db/interface_session.h>
#include <cyng/db/sql_table.h>
#include <cyng/value_cast.hpp>
#include <cyng/set_cast.h>
//#include <cyng/sql.h>
#include <cyng/sql/dsl/binary_expr.hpp>
#include <cyng/sql/dsl/list_expr.hpp>
#include <cyng/sql/dsl/operators.hpp>
#include <cyng/sql/dsl/assign.hpp>
#include <cyng/sql/dsl/aggregate.h>
#include <cyng/table/meta.hpp>

#include <boost/uuid/random_generator.hpp>

namespace node
{
	storage_db::storage_db(cyng::async::base_task* btp
		, cyng::logging::log_ptr logger
		, cyng::param_map_t cfg_db
		, cyng::param_map_t cfg_csv)
	: base_(*btp)
		, logger_(logger)
		, pool_(base_.mux_.get_io_service(), cyng::db::get_connection_type(cyng::value_cast<std::string>(cfg_db["type"], "SQLite")))
		, cfg_db_(cfg_db)
		, cfg_csv_(cfg_csv)
		, schema_(cyng::value_cast<std::string>(cfg_db["db-schema"], NODE_SUFFIX))
		, meta_map_(init_meta_map(schema_))
		, state_(TASK_STATE_WAITING_)
	{
		CYNG_LOG_INFO(logger_, "initialize task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< ">");
	}

	cyng::continuation storage_db::run()
	{	
		switch (state_) {
		case TASK_STATE_WAITING_:
			break;
		default:
			CYNG_LOG_INFO(logger_, "storage_db is running");
			break;
		}

		return cyng::continuation::TASK_CONTINUE;
	}

	void storage_db::stop()
	{
		if (state_ == TASK_STATE_OPEN_) {
			//
			//	b.t.w. this should never happens
			//
			pool_.stop();
		}
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> stopped");

	}

	//	slot 0 - merge
	cyng::continuation storage_db::process(std::chrono::system_clock::time_point start
		, std::chrono::system_clock::time_point end
		, std::chrono::minutes interval)
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> generate CSV file from "
			<< cyng::to_str(start)
			<< " to "
			<< cyng::to_str(end)
			<< " stepsize "
			<< cyng::to_str(interval));

		if (!pool_.start(cfg_db_))
		{
			CYNG_LOG_FATAL(logger_, "to start DB connection pool failed");
			return cyng::continuation::TASK_YIELD;
		}
		else
		{
			CYNG_LOG_INFO(logger_, "DB connection pool is running with "
				<< pool_.get_pool_size() 
				<< " connection(s)");

			//
			//	update task state
			//
			state_ = TASK_STATE_OPEN_;

			//
			//	generate CSV files
			//
			generate_csv_files(start, end, interval);

			//
			//	update task state
			//
			pool_.stop();
			state_ = TASK_STATE_WAITING_;

		}

		return cyng::continuation::TASK_CONTINUE;
	}

	void storage_db::generate_csv_files(std::chrono::system_clock::time_point start
		, std::chrono::system_clock::time_point end
		, std::chrono::minutes interval)
	{
		//
		//	get all available meters in the specified time range
		//
		//	SELECT server FROM TSMLMeta WHERE ((actTime > julianday('2018-08-06 00:00:00')) AND (actTime < julianday('2018-08-07 00:00:00'))) GROUP BY server;

		auto pos = meta_map_.find("TSMLMeta");
		if (pos != meta_map_.end())
		{
			auto s = pool_.get_session();
			cyng::table::meta_table_ptr meta = (*pos).second;
			cyng::sql::command cmd(meta, s.get_dialect());
			auto stmt = s.create_statement();

			//
			//	get all server ID in this time frame
			//
			std::vector<std::string> server_ids = get_server_ids(start, end, cmd, stmt);
			for (auto id : server_ids) {

				//
				//	get all used OBIS codes from this server in this time frame
				//
				std::vector<std::string> obis_codes = get_obis_codes(start, end, id, cmd, stmt);

				//
				//	open output file
				//
				auto file = open_file(start, end, id, obis_codes);

				collect_data(file, start, end, cmd, stmt, id, obis_codes);
				file.close();
			}

			stmt->close();
		}
	}

	void storage_db::collect_data(std::ofstream& file
		, std::chrono::system_clock::time_point start
		, std::chrono::system_clock::time_point end
		, cyng::sql::command& cmd
		, cyng::db::statement_ptr stmt
		, std::string const& id
		, std::vector<std::string> const& obis_code)
	{
		//
		//	get join result
		//
		//	select * from TSMLMeta INNER JOIN TSMLData ON TSMLMeta.pk = TSMLData.pk WHERE ((actTime > julianday('2018-08-06 00:00:00')) AND (actTime < julianday('2018-08-07 00:00:00')) AND server = '01-e61e-13090016-3c-07') ORDER BY trxID
		//	select * from TSMLMeta INNER JOIN TSMLData ON TSMLMeta.pk = TSMLData.pk WHERE ((actTime > julianday(?)) AND (actTime < julianday(?)) AND server = ?) ORDER BY trxID
		auto sql = "select TSMLMeta.pk, trxID, msgIdx, datetime(roTime), datetime(actTime), valTime, gateway, server, status, source, channel, target, OBIS, unitCode, unitName, dataType, scaler, val, result FROM TSMLMeta INNER JOIN TSMLData ON TSMLMeta.pk = TSMLData.pk WHERE ((actTime > julianday(?)) AND (actTime < julianday(?)) AND server = ?) ORDER BY actTime";
		auto r = stmt->prepare(sql);
		if (r.second) {
			stmt->push(cyng::make_object(start), 0)
				.push(cyng::make_object(end), 0)
				.push(cyng::make_object(id), 23)
				;

			//
			//	map to collect results
			//
			std::map<std::string, std::pair<std::string, std::string>>	value_map;

			//
			//	running tansaction
			//
			std::string trx;
			while (auto res = stmt->get_result()) {
				//	pk|trxID|msgIdx|roTime|actTime|valTime|gateway|server|status|source|channel|target|pk|OBIS|unitCode|unitName|dataType|scaler|val|result
				//	43a2a6bb-f45f-48e3-a2b7-74762f1752c1|41091175|1|2458330.95813657|2458330.97916667|118162556|00:15:3b:02:17:74|01-e61e-29436587-bf-03|0|0|0|Gas2|43a2a6bb-f45f-48e3-a2b7-74762f1752c1|0000616100ff|255|counter|u8|0|0|0

				//
				//	detect new lines in CSV file by changed trx
				//
				auto new_trx = cyng::value_cast<std::string>(res->get(2, cyng::TC_STRING, 0), "TRX");
				auto ro_time = cyng::value_cast(res->get(4, cyng::TC_TIME_POINT, 0), std::chrono::system_clock::now());
				auto act_time = cyng::value_cast(res->get(5, cyng::TC_TIME_POINT, 0), std::chrono::system_clock::now());
				auto code = cyng::value_cast<std::string>(res->get(13, cyng::TC_STRING, 24), "OBIS");
				auto result = cyng::value_cast<std::string>(res->get(19, cyng::TC_STRING, 512), "result");
				auto unit = cyng::value_cast<std::string>(res->get(15, cyng::TC_STRING, 512), "unitName");
				CYNG_LOG_TRACE(logger_, id
					<< ", "
					<< new_trx
					<< ", "
					//<< cyng::to_str(ro_time)
					//<< ", "
					<< cyng::to_str(act_time)
					<< ", "
					<< code
					<< ", "
					<< result
					<< ", "
					<< unit
				);


				//
				//	detect new line
				//
				if (trx != new_trx) {

					//
					//	start a new line except for the first value
					//
					if (!trx.empty()) {

						//
						//	append values
						//
						for (auto const& code : obis_code) {
							file << ';';
							auto pos = value_map.find(code);
							if (pos != value_map.end()) {
								file << pos->second.first;
							}
						}

						//
						//	add new line
						//
						file << std::endl;

					}

					//
					//	clear value map
					//
					value_map.clear();


					//
					//	new transaction
					//
					trx = new_trx;

					//
					//	write first values
					//
					file
						<< id
						<< ';'
						<< cyng::to_str(act_time)
						<< ';'
						<< trx
						;

				}

				value_map.emplace(code, std::make_pair(result, unit));

			}

			//
			//	append values
			//
			for (auto const& code : obis_code) {
				file << ';';
				auto pos = value_map.find(code);
				if (pos != value_map.end()) {
					file << pos->second.first;
				}
			}

			//
			//	add new line
			//
			file << std::endl;

			//
			//	read for next query
			//
			stmt->clear();

		}
		else {
			CYNG_LOG_ERROR(logger_, "prepare failed with: " << sql);
		}
	}


	std::vector<std::string> storage_db::get_server_ids(std::chrono::system_clock::time_point start
		, std::chrono::system_clock::time_point end
		, cyng::sql::command& cmd
		, cyng::db::statement_ptr stmt)
	{
		//
		//	example:
		//	SELECT server FROM TSMLMeta WHERE ((actTime > julianday('2018-08-08 00:00:00')) AND (actTime < julianday('2018-08-09 00:00:00'))) GROUP BY server
		//
		cmd.select()[cyng::sql::column(8)].where(cyng::sql::column(5) > cyng::sql::make_constant(start) && cyng::sql::column(5) < cyng::sql::make_constant(end)).group_by(cyng::sql::column(8));
		std::string sql = cmd.to_str();
		CYNG_LOG_TRACE(logger_, sql);	//	select ... from name

		std::pair<int, bool> r = stmt->prepare(sql);
		std::vector<std::string> result;
		BOOST_ASSERT(r.second);
		if (r.second) {
			while (auto res = stmt->get_result()) {

				//
				//	convert SQL result
				//
				result.push_back(cyng::value_cast<std::string>(res->get(1, cyng::TC_STRING, 23), "server"));
				CYNG_LOG_TRACE(logger_, result.back());
			}

			//
			//	close result set
			//
			stmt->close();
		}
		return result;
	}

	std::vector<std::string> storage_db::get_obis_codes(std::chrono::system_clock::time_point start
		, std::chrono::system_clock::time_point end
		, std::string const& id
		, cyng::sql::command& cmd
		, cyng::db::statement_ptr stmt)
	{
		//
		//	example:
		//	SELECT TSMLData.OBIS FROM TSMLMeta INNER JOIN TSMLData ON TSMLMeta.pk = TSMLData.pk WHERE ((actTime > julianday(?)) AND (actTime < julianday(?)) AND server = ?) GROUP BY TSMLData.OBIS;

		//
		cmd.select()[cyng::sql::column(8)].where(cyng::sql::column(5) > cyng::sql::make_constant(start) && cyng::sql::column(5) < cyng::sql::make_constant(end)).group_by(cyng::sql::column(8));
		std::string sql = "SELECT TSMLData.OBIS FROM TSMLMeta INNER JOIN TSMLData ON TSMLMeta.pk = TSMLData.pk WHERE ((actTime > julianday(?)) AND (actTime < julianday(?)) AND server = ?) GROUP BY TSMLData.OBIS";
		CYNG_LOG_TRACE(logger_, sql);	//	select ... from name

		std::pair<int, bool> r = stmt->prepare(sql);
		std::vector<std::string> result;
		BOOST_ASSERT(r.second);
		if (r.second) {

			stmt->push(cyng::make_object(start), 0)
				.push(cyng::make_object(end), 0)
				.push(cyng::make_object(id), 23);

			while (auto res = stmt->get_result()) {

				//
				//	convert SQL result
				//
				result.push_back(cyng::value_cast<std::string>(res->get(1, cyng::TC_STRING, 23), "TSMLData.OBIS"));
				CYNG_LOG_TRACE(logger_, result.back());
			}

			//
			//	close result set
			//
			stmt->clear();
		}
		return result;
	}

	std::ofstream storage_db::open_file(std::chrono::system_clock::time_point start
		, std::chrono::system_clock::time_point end
		, std::string const& id
		, std::vector<std::string> const& codes)
	{
		auto tt_start = std::chrono::system_clock::to_time_t(start);
		std::tm time_start = cyng::chrono::convert_utc(tt_start);

		auto tt_end = std::chrono::system_clock::to_time_t(end);
		std::tm time_end = cyng::chrono::convert_utc(tt_end);

		std::stringstream ss;
		ss
			<< std::setfill('0')
			<< id
			<< '_'
			<< cyng::chrono::year(time_start)
			<< '-'
			<< std::setw(2)
			<< cyng::chrono::month(time_start)
			<< '-'
			<< std::setw(2)
			<< cyng::chrono::day(time_start)
			<< '-'
			<< std::setw(2)
			<< cyng::chrono::month(time_end)
			<< '-'
			<< std::setw(2)
			<< cyng::chrono::day(time_end)
			<< ".csv"
			;
		CYNG_LOG_TRACE(logger_, "open "<< ss.str());
		std::ofstream f(ss.str());
		f << "server;time;trx;";
		for (auto const& c : codes) {
			f << c << ';';
		}
		f << std::endl;
		return f;
	}

	cyng::table::mt_table storage_db::init_meta_map(std::string const& ver)
	{
		//
		//	SQL table schemes
		//
		std::map<std::string, cyng::table::meta_table_ptr> meta_map;

		if (boost::algorithm::equals(ver, "v4.0"))
		{
			meta_map.emplace("TSMLMeta", cyng::table::make_meta_table<1, 11>("TSMLMeta",
				{ "ident"
				, "trxID"	//	unique for every message
				, "midx"	//	message index
				, "roTime"	//	readout time
				, "actTime"	//	timestamp
				, "valTime"	//	seconds index
				, "gateway"
				, "server"
				, "meter"
				, "status"	//	M-Bus status
				, "source"
				, "channel" },
				{ cyng::TC_UUID		//	ident
				, cyng::TC_STRING	//	trxID
				, cyng::TC_UINT32	//	midx
				, cyng::TC_TIME_POINT	//	roTime
				, cyng::TC_TIME_POINT	//	actTime
				, cyng::TC_UINT32	//	valTime
				, cyng::TC_STRING	//	gateway
				, cyng::TC_STRING	//	server
				, cyng::TC_STRING	//	meter
				, cyng::TC_UINT32	//	status
				, cyng::TC_UINT32	//	source
				, cyng::TC_UINT32 },	//	channel
				{ 36	//	ident
				, 16	//	trxID
				, 0		//	midx
				, 0		//	roTime
				, 0		//	actTime
				, 0		//	valTime
				, 23	//	gateway
				, 23	//	server
				, 10	//	meter
				, 0		//	status
				, 0		//	source
				, 0 }));	//	channel

			meta_map.emplace("TSMLData", cyng::table::make_meta_table<2, 6>("TSMLData",
				{ "ident"
				, "OBIS"
				, "unitCode"
				, "unitName"
				, "dataType"
				, "scaler"
				, "val"		//	changed to val (since value is an SQL keyword)
				, "result" },
				{ cyng::TC_UUID		//	ident
				, cyng::TC_STRING	//	OBIS
				, cyng::TC_UINT8	//	unitCode
				, cyng::TC_STRING	//	unitName
				, cyng::TC_STRING	//	dataType
				, cyng::TC_INT32	//	scaler
				, cyng::TC_INT64	//	val
				, cyng::TC_STRING },	//	result
				{ 36	//	ident
				, 24	//	OBIS
				, 0		//	unitCode
				, 64	//	unitName
				, 16	//	dataType
				, 0		//	scaler
				, 0		//	val
				, 512 }));	//	result

		}
		else
		{

			//
			//	trxID - unique for every message
			//	msgIdx - message index
			//	status - M-Bus status
			//
			meta_map.emplace("TSMLMeta", cyng::table::make_meta_table<1, 11>("TSMLMeta",
				{ "pk", "trxID", "msgIdx", "roTime", "actTime", "valTime", "gateway", "server", "status", "source", "channel", "target" },
				{ cyng::TC_UUID, cyng::TC_STRING, cyng::TC_UINT32, cyng::TC_TIME_POINT, cyng::TC_TIME_POINT, cyng::TC_UINT32, cyng::TC_STRING, cyng::TC_STRING, cyng::TC_UINT32, cyng::TC_UINT32, cyng::TC_UINT32, cyng::TC_STRING },
				{ 36, 16, 0, 0, 0, 0, 23, 23, 0, 0, 0, 32 }));

			//
			//	unitCode - physical unit
			//	unitName - descriptiv
			//	
			meta_map.emplace("TSMLData", cyng::table::make_meta_table<2, 6>("TSMLData",
				{ "pk", "OBIS", "unitCode", "unitName", "dataType", "scaler", "val", "result" },
				{ cyng::TC_UUID, cyng::TC_STRING, cyng::TC_UINT8, cyng::TC_STRING, cyng::TC_STRING, cyng::TC_INT32, cyng::TC_INT64, cyng::TC_STRING },
				{ 36, 24, 0, 64, 16, 0, 0, 512 }));
		}
		return meta_map;
	}


}
