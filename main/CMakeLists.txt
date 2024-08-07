set(requires "")
idf_build_get_property(target IDF_TARGET)

file(GLOB_RECURSE SOURCE ./*.c)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs esp-tls esp_http_server protocol_examples_common nvs_flash)
endif()
idf_component_register(
                    SRC_DIRS "." "./ui" "./ui/screens" "./ui/components" "./ui/fonts"
                    INCLUDE_DIRS "." "./ui"
                    REQUIRES ${requires})
