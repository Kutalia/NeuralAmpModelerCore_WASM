file(GLOB_RECURSE NAM_SOURCES ../NAM/*.cpp ../NAM/*.c ../NAM*.h)

set(TOOLS loadwebmodel)

add_custom_target(tools ALL
	DEPENDS ${TOOLS})

include_directories(tools ..)
include_directories(tools ${NAM_DEPS_PATH}/eigen)
include_directories(tools ${NAM_DEPS_PATH}/nlohmann)

add_executable(loadwebmodel loadwebmodel.cpp ${NAM_SOURCES})

add_definitions(-DNAM_SAMPLE_FLOAT)
add_definitions(-DEIGEN_STACK_ALLOCATION_LIMIT=0)

source_group(NAM ${CMAKE_CURRENT_SOURCE_DIR} FILES ${NAM_SOURCES})

target_compile_features(${TOOLS} PUBLIC cxx_std_17)

set_target_properties(${TOOLS}
	PROPERTIES
	CXX_VISIBILITY_PRESET hidden
	INTERPROCEDURAL_OPTIMIZATION TRUE
	PREFIX ""
)

set_target_properties(loadwebmodel PROPERTIES COMPILE_FLAGS "-Os -pthread")
set_target_properties(loadwebmodel PROPERTIES LINK_FLAGS "-Os -sEXPORTED_FUNCTIONS=stringToUTF8,_malloc,_setDsp,_free -sENVIRONMENT=web,worker -sEXPORTED_RUNTIME_METHODS=ccall -sAUDIO_WORKLET -sWASM_WORKERS -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=1024MB -sTOTAL_STACK=512MB -sASYNCIFY -pthread")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_definitions(${TOOLS} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
	target_compile_options(${TOOLS} PRIVATE
		"$<$<CONFIG:DEBUG>:/W4>"
		"$<$<CONFIG:RELEASE>:/O2>"
	)
else()
	target_compile_options(${TOOLS} PRIVATE
		-Wall -Wextra -Wpedantic -Wstrict-aliasing -Wunreachable-code -Weffc++ -Wno-unused-parameter
		"$<$<CONFIG:DEBUG>:-Og;-ggdb;-Werror>"
		"$<$<CONFIG:RELEASE>:-Os>"
	)
endif()