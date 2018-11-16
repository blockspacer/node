/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Sylko Olzscher
 *
 */


#include "db_meta.h"
#include <cyng/table/meta.hpp>

namespace node
{
	cyng::table::meta_table_ptr TSMLMeta()
	{
		return cyng::table::make_meta_table<1, 12>("TSMLMeta",
			{ "pk", "trxID", "msgIdx", "roTime", "actTime", "valTime", "gateway", "server", "status", "source", "channel", "target", "profile" },
			{ cyng::TC_UUID, cyng::TC_STRING, cyng::TC_UINT32, cyng::TC_TIME_POINT, cyng::TC_TIME_POINT, cyng::TC_UINT32, cyng::TC_STRING, cyng::TC_STRING, cyng::TC_UINT32, cyng::TC_UINT32, cyng::TC_UINT32, cyng::TC_STRING, cyng::TC_STRING },
			{ 36, 16, 0, 0, 0, 0, 23, 23, 0, 0, 0, 32, 24 });
	}

	cyng::table::meta_table_ptr TSMLData()
	{
		return cyng::table::make_meta_table<2, 6>("TSMLData",
			{ "pk", "OBIS", "unitCode", "unitName", "dataType", "scaler", "val", "result" },
			{ cyng::TC_UUID, cyng::TC_STRING, cyng::TC_UINT8, cyng::TC_STRING, cyng::TC_STRING, cyng::TC_INT32, cyng::TC_INT64, cyng::TC_STRING },
			{ 36, 24, 0, 64, 16, 0, 0, 512 });
	}

	cyng::table::meta_table_ptr gw_devices()
	{
		return cyng::table::make_meta_table<1, 12>("devices",
			{ "serverID"			//	server ID
			, "lastSeen"	//	last seen - Letzter Datensatz: 20.06.2018 14:34:22"
			, "class"		//	device class (always "---" == 2D 2D 2D)
			, "visible"
			, "active"
			, "descr"
			//	---
			, "status"	//	"Statusinformation: 00"
			, "mask"	//	"Bitmaske: 00 00"
			, "interval"	//	"Zeit zwischen zwei Datens�tzen: 49000"
							//	--- optional data
			, "pubKey"	//	Public Key: 18 01 16 05 E6 1E 0D 02 BF 0C FA 35 7D 9E 77 03"
			, "aes"	//	AES-Schl�ssel: "
			, "user"
			, "pwd"
			},
			{ cyng::TC_BUFFER		//	server ID
			, cyng::TC_TIME_POINT	//	last seen
			, cyng::TC_STRING		//	device class
			, cyng::TC_BOOL			//	visible
			, cyng::TC_BOOL			//	active
			, cyng::TC_STRING		//	description

			, cyng::TC_UINT64		//	status (81 00 60 05 00 00)
			, cyng::TC_BUFFER		//	bit mask (81 81 C7 86 01 FF)
			, cyng::TC_UINT32		//	interval (milliseconds)
			, cyng::TC_BUFFER		//	pubKey
			, cyng::TC_BUFFER		//	aes
			, cyng::TC_STRING		//	user
			, cyng::TC_STRING		//	pwd
			},
			{ 9
			, 0
			, 16	//	device class
			, 0		//	visible
			, 0		//	active
			, 128	//	description

			, 0		//	status
			, 8		//	mask
			, 0		//	interval
			, 16	//	pubKey
			, 32	//	aes
			, 32	//	user
			, 32	//	pwd
			});
	}

	cyng::table::meta_table_ptr gw_push_ops()
	{
		return cyng::table::make_meta_table<2, 6>("push.ops",
			{ "serverID"	//	server ID
			, "idx"			//	index
			//	-- body
			, "interval"	//	seconds
			, "delay"		//	seconds
			, "target"		//	target name
			, "source"		//	push source (profile, installation parameters, list of visible sensors/actors)
			, "profile"		//	"Lastgang"
			, "task"		//	associated task id

			},
			{ cyng::TC_BUFFER		//	server ID
			, cyng::TC_UINT8		//	index
									//	-- body
			, cyng::TC_UINT32		//	interval [seconds]
			, cyng::TC_UINT32		//	delay [seconds]
			, cyng::TC_STRING		//	target
			, cyng::TC_UINT8		//	source
			, cyng::TC_UINT8		//	profile
			, cyng::TC_UINT64		//	task

			},
			{ 9		//	server ID
			, 0		//	index
					//	-- body
			, 0		//	interval [seconds]
			, 0		//	delay
			, 64	//	target
			, 0		//	source
			, 0		//	profile
			, 0		//	task id
			});
	}

	cyng::table::meta_table_ptr gw_op_log()
	{
		return cyng::table::make_meta_table<1, 10>("op.log",
			{ "idx"			//	index
							//	-- body
			, "actTime"		//	actual time
			, "regPeriod"	//	register period
			, "valTime"		//	val time
			, "status"		//	status word
			, "event"		//	event code
			, "peer"		//	peer address
			, "utc"			//	UTC time
			, "serverId"	//	server ID (meter)
			, "target"		//	target name
			, "pushNr"		//	operation number
			},
			{ cyng::TC_UINT64		//	index 
									//	-- body
			, cyng::TC_TIME_POINT	//	actTime
			, cyng::TC_UINT32		//	regPeriod
			, cyng::TC_TIME_POINT	//	valTime
			, cyng::TC_UINT64		//	status
			, cyng::TC_UINT32		//	event
			, cyng::TC_BUFFER		//	peer_address
			, cyng::TC_TIME_POINT	//	UTC time
			, cyng::TC_BUFFER		//	serverId
			, cyng::TC_STRING		//	target
			, cyng::TC_UINT8		//	push_nr

			},
			{ 0		//	index
					//	-- body
			, 0		//	actTime
			, 0		//	regPeriod
			, 0		//	valTime
			, 0		//	status
			, 0		//	event
			, 13	//	peer_address
			, 0		//	utc
			, 23	//	serverId
			, 64	//	target
			, 0		//	push_nr
			});
	}


}



