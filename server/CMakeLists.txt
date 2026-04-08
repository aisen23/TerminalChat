set(APP_OUTPUT_NAME terminal_chat_server)

set(SRC
	main.cpp
	pch.h

	src/Application.h
	src/Application.cpp
	src/NetServer.h
	src/NetServer.cpp

	"${APP_OUTPUT_NAME}.LogConfig.txt"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_executable(server ${SRC})

target_include_directories(server
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${NLOHMANN_JSON_DIR}
)

target_link_libraries(server PRIVATE common net)

if(MSVC)
    target_compile_options(server PRIVATE
		/W4 /WX
        /permissive-
        /Zc:__cplusplus
    )
endif()

set_target_properties(server
	PROPERTIES
	OUTPUT_NAME "${APP_OUTPUT_NAME}"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_custom_command(TARGET server POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${CMAKE_CURRENT_SOURCE_DIR}/${APP_OUTPUT_NAME}.LogConfig.txt"
	"$<TARGET_FILE_DIR:server>/${APP_OUTPUT_NAME}.LogConfig.txt"
)

target_precompile_headers(server PRIVATE pch.h)
