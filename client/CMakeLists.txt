set(SRC
	main.cpp
    pch.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_executable(client ${SRC})

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
