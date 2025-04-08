idf_component_register(
    SRCS "RC5.c"
    INCLUDE_DIRS "."
    REQUIRES driver
    REQUIRES driver esp_timer
)
