set(SRC
	main.cpp
    pch.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_executable(server ${SRC})

if(MSVC)
    target_compile_options(server PRIVATE
		/W4 /WX
        /permissive-
        /Zc:__cplusplus
    )
endif()

set_target_properties(server
    PROPERTIES
	OUTPUT_NAME "terminal_chat_server"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_precompile_headers(server PRIVATE pch.h)
