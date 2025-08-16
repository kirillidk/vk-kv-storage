add_library(${KV_STORAGE_LIB} INTERFACE ${KV_STORAGE_SOURCES})

target_include_directories(${KV_STORAGE_LIB} INTERFACE 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)

target_link_libraries(${KV_STORAGE_LIB} INTERFACE
    Boost::container
)