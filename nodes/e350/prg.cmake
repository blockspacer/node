# top level files
set (node_e350)

set (node_e350_cpp

	nodes/e350/src/main.cpp	
	nodes/e350/src/controller.cpp
	nodes/shared/net/server_stub.cpp
	nodes/shared/net/session_stub.cpp
	nodes/e350/src/server.cpp
	nodes/e350/src/session.cpp
#	nodes/e350/src/connection.cpp
)

set (node_e350_h

	nodes/e350/src/controller.h
	src/main/include/smf/cluster/server_stub.h
	nodes/e350/src/server.h
#	nodes/e350/src/connection.h
#	src/main/include/smf/cluster/connection_stub.hpp
	src/main/include/smf/cluster/session_stub.h
	nodes/e350/src/session.h
)

set (node_e350_info
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_project_info.h

	nodes/print_build_info.h
	nodes/print_version_info.h
	nodes/set_start_options.h
	nodes/show_ip_address.h
	nodes/write_pid.h

	nodes/print_build_info.cpp
	nodes/print_version_info.cpp
	nodes/set_start_options.cpp
	nodes/show_ip_address.cpp
	nodes/write_pid.cpp
)

set (node_e350_tasks
	nodes/e350/src/tasks/cluster.h
	nodes/e350/src/tasks/cluster.cpp
#	nodes/e350/src/tasks/gatekeeper.h
#	nodes/e350/src/tasks/gatekeeper.cpp
	nodes/shared/tasks/gatekeeper.h
	nodes/shared/tasks/gatekeeper.cpp
)
	
if(WIN32)

	set (node_e350_service
		nodes/e350/templates/e350_create_service.cmd.in
		nodes/e350/templates/e350_delete_service.cmd.in
		nodes/e350/templates/e350_restart_service.cmd.in
		nodes/e350/templates/e350.windows.cgf.in
	)

 	set (node_e350_res
		${CMAKE_CURRENT_BINARY_DIR}/e350.rc 
		src/main/resources/logo.ico
		nodes/e350/templates/e350.exe.manifest
	)

else()

	set (node_e350_service
		nodes/e350/templates/e350.service.in
		nodes/e350/templates/e350.linux.cgf.in
	)

endif()

source_group("tasks" FILES ${node_e350_tasks})
source_group("service" FILES ${node_e350_service})
source_group("info" FILES ${node_e350_info})


# define the main program
set (node_e350
  ${node_e350_cpp}
  ${node_e350_h}
  ${node_e350_info}
  ${node_e350_tasks}
  ${node_e350_res}
  ${node_e350_service}
)

if(WIN32)
	source_group("resources" FILES ${node_e350_res})
	list(APPEND node_e350 ${node_e350_res})
endif()
