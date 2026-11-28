idf_component_register(
    SRCS
        "main.c"
        "sd_card.c"
        "../core/gvt.c"
    INCLUDE_DIRS
        "."
        "../core"
    REQUIRES
        fatfs
        sdmmc
        esp_driver_sdmmc
)

target_link_libraries(${COMPONENT_LIB} PRIVATE m)
