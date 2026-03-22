set(SRC
	include/Net/Message.h
	include/Net/Connection.h

	src/net.cpp

    pch.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_library(net SHARED ${SRC})

target_include_directories(net
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${BOOST_DIR}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

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
