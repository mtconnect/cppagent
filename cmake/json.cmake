

set(JSON_BuildTests OFF CACHE INTERNAL "")

function(AddJsonSupport projectTarget)
	target_include_directories(${projectTarget} PRIVATE ${CMAKE_SOURCE_DIR}/json/include)
endfunction()


