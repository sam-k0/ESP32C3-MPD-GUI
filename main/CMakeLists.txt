idf_component_register(
    SRC_DIRS 
        "."
        "./layer_manager"
    INCLUDE_DIRS 
        "."
        "./layer_manager"
    )


target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-cast-function-type)
