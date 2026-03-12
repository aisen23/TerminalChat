set(SRC
	include/Message.h
	include/Connection.h
	include/ClientInterface.h

	src/net.cpp

    pch.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_library(net SHARED ${SRC})

target_include_directories(net
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${BOOST_DIR}
)

target_link_libraries(net PRIVATE common)

if(MSVC)
    target_compile_options(net PRIVATE
		/W4 /WX
        /permissive-
        /Zc:__cplusplus
    )
endif()

set_target_properties(net
    PROPERTIES
	OUTPUT_NAME "tc_net"
)

target_precompile_headers(net PRIVATE pch.h)
