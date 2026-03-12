set(SRC
	include/CommonApi.h
	include/Utils.h
	include/Queue.h

	src/Utils.cpp

    pch.h
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

add_library(common SHARED ${SRC})

target_compile_definitions(common PRIVATE TC_COMMON_BUILD_DLL)

target_include_directories(common
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(common PRIVATE
		/W4 /WX
        /permissive-
        /Zc:__cplusplus
    )
endif()

set_target_properties(common
    PROPERTIES
	OUTPUT_NAME "tc_common"
)

target_precompile_headers(common PRIVATE pch.h)
