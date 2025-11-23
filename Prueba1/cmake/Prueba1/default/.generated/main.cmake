# cmake files support debug production
include("${CMAKE_CURRENT_LIST_DIR}/rule.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/file.cmake")

set(Prueba1_default_library_list )

# Handle files with suffix (s|as|asm|AS|ASM|As|aS|Asm), for group default-XC8
if(Prueba1_default_default_XC8_FILE_TYPE_assemble)
add_library(Prueba1_default_default_XC8_assemble OBJECT ${Prueba1_default_default_XC8_FILE_TYPE_assemble})
    Prueba1_default_default_XC8_assemble_rule(Prueba1_default_default_XC8_assemble)
    list(APPEND Prueba1_default_library_list "$<TARGET_OBJECTS:Prueba1_default_default_XC8_assemble>")
endif()

# Handle files with suffix S, for group default-XC8
if(Prueba1_default_default_XC8_FILE_TYPE_assemblePreprocess)
add_library(Prueba1_default_default_XC8_assemblePreprocess OBJECT ${Prueba1_default_default_XC8_FILE_TYPE_assemblePreprocess})
    Prueba1_default_default_XC8_assemblePreprocess_rule(Prueba1_default_default_XC8_assemblePreprocess)
    list(APPEND Prueba1_default_library_list "$<TARGET_OBJECTS:Prueba1_default_default_XC8_assemblePreprocess>")
endif()

# Handle files with suffix [cC], for group default-XC8
if(Prueba1_default_default_XC8_FILE_TYPE_compile)
add_library(Prueba1_default_default_XC8_compile OBJECT ${Prueba1_default_default_XC8_FILE_TYPE_compile})
    Prueba1_default_default_XC8_compile_rule(Prueba1_default_default_XC8_compile)
    list(APPEND Prueba1_default_library_list "$<TARGET_OBJECTS:Prueba1_default_default_XC8_compile>")
endif()

add_executable(Prueba1_default_image_J6cH4u9v ${Prueba1_default_library_list})

set_target_properties(Prueba1_default_image_J6cH4u9v PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${Prueba1_default_output_dir})
set_target_properties(Prueba1_default_image_J6cH4u9v PROPERTIES OUTPUT_NAME "default")
set_target_properties(Prueba1_default_image_J6cH4u9v PROPERTIES SUFFIX ".elf")

target_link_libraries(Prueba1_default_image_J6cH4u9v PRIVATE ${Prueba1_default_default_XC8_FILE_TYPE_link})


# Add the link options from the rule file.
Prueba1_default_link_rule(Prueba1_default_image_J6cH4u9v)



