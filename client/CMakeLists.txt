set(APP_OUTPUT_NAME terminal_chat)

set(SRC
	main.cpp
	pch.h

	src/Application.h
	src/Application.cpp
	src/NetClient.h
	src/NetClient.cpp
	src/Utils.h
	src/Utils.cpp

	"${APP_OUTPUT_NAME}.LogConfig.txt"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_executable(client ${SRC})

target_include_directories(client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${NLOHMANN_JSON_DIR}
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
	OUTPUT_NAME "${APP_OUTPUT_NAME}"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET client POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${CMAKE_CURRENT_SOURCE_DIR}/${APP_OUTPUT_NAME}.LogConfig.txt"
	"$<TARGET_FILE_DIR:client>/${APP_OUTPUT_NAME}.LogConfig.txt"
)

target_precompile_headers(client PRIVATE pch.h)
