idf_component_register(
   SRC_DIRS               "." 
                          "Arduino_libs"
                          "display"
                          "bsp"
                          

   INCLUDE_DIRS           "." 
                          "Arduino_libs/include"
                          "board"
                          "display"
                          "bsp"
)

target_compile_options(${COMPONENT_TARGET} PUBLIC
    -DBOARD_HAS_PSRAM
)
