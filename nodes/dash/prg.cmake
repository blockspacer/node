# top level files
set (node_dash)

set (node_dash_cpp

	nodes/dash/src/main.cpp	
	nodes/dash/src/controller.cpp
)

set (node_dash_h

	nodes/dash/src/controller.h

)

set (node_dash_tasks
	nodes/dash/src/tasks/cluster.h
	nodes/dash/src/tasks/cluster.cpp
	nodes/dash/src/tasks/system.h
	nodes/dash/src/tasks/system.cpp
)

set (node_dash_shared
	nodes/dash_shared/src/sync_db.h
	nodes/dash_shared/src/sync_db.cpp
	nodes/dash_shared/src/dispatcher.h
	nodes/dash_shared/src/dispatcher.cpp
	nodes/dash_shared/src/forwarder.h
	nodes/dash_shared/src/forwarder.cpp
	nodes/dash_shared/src/form_data.h
	nodes/dash_shared/src/form_data.cpp

	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_project_info.h

	nodes/print_build_info.h
	nodes/print_version_info.h
	nodes/set_start_options.h
	nodes/show_ip_address.h

	nodes/print_build_info.cpp
	nodes/print_version_info.cpp
	nodes/set_start_options.cpp
	nodes/show_ip_address.cpp

	src/main/include/smf/shared/ctl.h
	nodes/shared/sys/ctl.cpp

)

if (UNIX)
	list(APPEND node_dash_shared src/main/include/smf/shared/write_pid.h)
	list(APPEND node_dash_shared nodes/shared/sys/write_pid.cpp)
endif(UNIX)


set (node_dash_schemes

	src/main/include/smf/shared/db_schemes.h
	nodes/shared/db/db_schemes.cpp
)
	
if(WIN32)

	set (node_dash_service
		nodes/dash/templates/dash_create_service.cmd.in
		nodes/dash/templates/dash_delete_service.cmd.in
		nodes/dash/templates/dash_restart_service.cmd.in
		nodes/dash/templates/dash.windows.cgf.in
	)
 
 	set (node_dash_res
		${CMAKE_CURRENT_BINARY_DIR}/dash.rc 
		src/main/resources/logo.ico
		nodes/dash/templates/dash.exe.manifest
	)

else()

	set (node_dash_service
		nodes/dash/templates/dash.service.in
		nodes/dash/templates/dash.linux.cgf.in
	)

endif()

source_group("tasks" FILES ${node_dash_tasks})
source_group("service" FILES ${node_dash_service})
source_group("shared" FILES ${node_dash_shared})
source_group("schemes" FILES ${node_dash_schemes})


# define the main program
set (node_dash
  ${node_dash_cpp}
  ${node_dash_h}
  ${node_dash_tasks}
  ${node_dash_service}
  ${node_dash_shared}
  ${node_dash_schemes}
)

if (${PROJECT_NAME}_PUGIXML_INSTALLED)
	set (node_dash_xml
		${PUGIXML_INCLUDE_DIR}/pugixml.hpp
		${PUGIXML_INCLUDE_DIR}/pugixml.cpp
	)
	list(APPEND node_dash ${node_dash_xml})
	source_group("XML" FILES ${node_dash_xml})

endif()

if(WIN32)
	source_group("resources" FILES ${node_dash_res})
	list(APPEND node_dash ${node_dash_res})
endif()
