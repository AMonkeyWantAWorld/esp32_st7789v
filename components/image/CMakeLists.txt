idf_component_register(SRCS ${components_srcs}
                    INCLUDE_DIRS ${components_incs}
                    PRIV_REQUIRES ${components_requires})

set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web_image")
if (EXISTS ${WEB_SRC_DIR})
    spiffs_create_partition_image(storage ${WEB_SRC_DIR} FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "${WEB_SRC_DIR} doesn't exit.")
endif()
