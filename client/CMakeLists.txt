set(SRC
	main.cpp
    pch.h

    src/Application.h
    src/Application.cpp
	src/NetClient.h
	src/NetClient.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_executable(client ${SRC})

target_include_directories(client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(client PRIVATE common net)

if(MSVC)
    target_compile_options(client PRIVATE
		/W4 /WX
        /permissive-
        /Zc:__cplusplus
    )
endif()

set_target_properties(client
    PROPERTIES
	OUTPUT_NAME "tc_client"
)

target_precompile_headers(client PRIVATE pch.h)
