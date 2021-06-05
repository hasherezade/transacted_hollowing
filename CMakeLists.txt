cmake_minimum_required (VERSION 2.8)
project (transacted_hollowing)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")

set (srcs
	main.cpp
	util.cpp
	ntdll_undoc.cpp
	pe_hdrs_helper.cpp
	hollowing_parts.cpp
)

set (hdrs
	util.h
	ntdll_undoc.h
	kernel32_undoc.h
	ntdll_types.h
	ntddk.h
	pe_hdrs_helper.h
	hollowing_parts.h
)

add_executable (transacted_hollowing ${hdrs} ${srcs})

INSTALL( TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT ${PROJECT_NAME} )
