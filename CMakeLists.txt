cmake_minimum_required (VERSION 2.8)
project (transacted_hollowing)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")

option( GHOSTING "Use a delete-pending file instead of a transacted file (like in Process Ghosting)" OFF )
if(GHOSTING)
    add_compile_definitions(GHOSTING)
endif()

set (srcs
	main.cpp
	util.cpp
	pe_hdrs_helper.cpp
	hollowing_parts.cpp
)

set (hdrs
	util.h
	kernel32_undoc.h
	ntddk.h
	pe_hdrs_helper.h
	hollowing_parts.h
)

if(GHOSTING)
	set (srcs
		${srcs}
		delete_pending_file.cpp
	)
	set (hdrs
		${hdrs}
		delete_pending_file.h
	)
else()
	set (srcs
		${srcs}
		transacted_file.cpp
	)
	set (hdrs
		${hdrs}
		transacted_file.h
	)
endif()


add_executable (transacted_hollowing ${hdrs} ${srcs})

INSTALL( TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT ${PROJECT_NAME} )
