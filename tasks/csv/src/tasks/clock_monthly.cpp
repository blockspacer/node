/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */

#include "clock_monthly.h"
#include <cyng/chrono.h>
#include <cyng/io/io_chrono.hpp>
#include <cyng/async/task/base_task.h>
#include <cyng/factory/set_factory.h>
#include <cyng/dom/algorithm.h>

namespace node
{
	clock_monthly::clock_monthly(cyng::async::base_task* btp
		, cyng::logging::log_ptr logger
		, std::size_t tsk_db
		, cyng::param_map_t cfg_trigger)
	: base_(*btp)
		, logger_(logger)
		, tsk_db_(tsk_db)
		, offset_(cyng::find_value(cfg_trigger, "offset", 8))
		, state_(TASK_STATE_INITIAL_)
		//
		//	calculate trigger time
		//
		, next_trigger_tp_(calculate_trigger_tp())
	{
		CYNG_LOG_INFO(logger_, "initialize task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> next trigger time: "
			<< cyng::to_str(next_trigger_tp_+ offset_));
	}

	cyng::continuation clock_monthly::run()
	{	
		switch (state_) {
		case TASK_STATE_INITIAL_:

			//
			//	trigger generation of last time period
			//
			generate_last_period();


			//
			//	update task state
			//
			state_ = TASK_STATE_RUNNING_;
			break;
		default:
			//
			//	trigger generation of current period
			//
			//generate_current_period();
			next_trigger_tp_ += std::chrono::hours(24);
			break;
		}

		//
		//	(re)start timer
		//
		base_.suspend_until(next_trigger_tp_ + offset_);

		return cyng::continuation::TASK_CONTINUE;
	}

	void clock_monthly::stop()
	{
		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> stopped");
	}

	//	slot 0
	cyng::continuation clock_monthly::process()
	{	//	unused
		return cyng::continuation::TASK_CONTINUE;
	}

	std::chrono::system_clock::time_point clock_monthly::calculate_trigger_tp()
	{
		std::tm tmp = cyng::chrono::make_utc_tm(std::chrono::system_clock::now());

		return cyng::chrono::init_tp(cyng::chrono::year(tmp)
			, cyng::chrono::month(tmp)
			, cyng::chrono::day(tmp)
			, 0	//	hour
			, 0	//	minute
			, 0.0) //	this day
			+ std::chrono::hours(24)	//	next day
			;
	}

	void clock_monthly::generate_last_period()
	{
		std::tm tmp = cyng::chrono::make_utc_tm(next_trigger_tp_);
		if (tmp.tm_mday < 12) {

			//
			//	generate a file for the last month during the first 10 days of the new month
			//

			//
			//	calculate last day of last month
			//
			auto tp = next_trigger_tp_ - std::chrono::hours(tmp.tm_mday * 24);
			auto d = cyng::chrono::days_of_month(tp);

			CYNG_LOG_INFO(logger_, "task #"
				<< base_.get_id()
				<< " <"
				<< base_.get_class_name()
				<< "> generate report for last month with "
				<< d.count()
				<< " days" );

			base_.mux_.post(tsk_db_, 1, cyng::tuple_factory(tp, static_cast<std::int32_t>(d.count())));

		}
	}

	void clock_monthly::generate_current_period()
	{
		auto d = cyng::chrono::days_of_month(next_trigger_tp_);

		CYNG_LOG_INFO(logger_, "task #"
			<< base_.get_id()
			<< " <"
			<< base_.get_class_name()
			<< "> generate report for this month with "
			<< d.count()
			<< " days");

		base_.mux_.post(tsk_db_, 1, cyng::tuple_factory(next_trigger_tp_, d.count()));

	}

}
