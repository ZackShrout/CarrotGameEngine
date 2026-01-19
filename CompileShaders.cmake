# Reusable function to compile GLSL shaders to SPIR-V using glslangValidator
# Keeps the same output directory and naming convention as your old code

function(compile_glsl_to_spv TARGET_NAME GLSL_FILE OUTPUT_DIR)
    # Make absolute path from source root
    cmake_path(SET ABS_GLSL "${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_FILE}" NORMALIZE)

    # Get base name without extension (triangle.vert → triangle.vert)
    get_filename_component(BASE_NAME "${GLSL_FILE}" NAME)

    # Output path: same as before: <OUTPUT_DIR>/<shader_name>.spv
    cmake_path(SET SPV_OUTPUT "${OUTPUT_DIR}/${BASE_NAME}.spv" NORMALIZE)

    add_custom_command(
            OUTPUT "${SPV_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
            COMMAND Vulkan::glslangValidator -V "${ABS_GLSL}" -o "${SPV_OUTPUT}"
            DEPENDS "${ABS_GLSL}"
            COMMENT "glslangValidator → SPIR-V: ${BASE_NAME}"
            VERBATIM
    )

    # Create a unique target per shader so we can depend on it cleanly
    string(MAKE_C_IDENTIFIER "${BASE_NAME}" TARGET_ID)
    set(SHADER_TARGET "${TARGET_NAME}_${TARGET_ID}_spv")

    add_custom_target(${SHADER_TARGET} DEPENDS "${SPV_OUTPUT}")
    add_dependencies(${TARGET_NAME} ${SHADER_TARGET})

    # Optional: make the output file available as a property if needed later
    set_property(TARGET ${SHADER_TARGET} PROPERTY OUTPUT_FILE "${SPV_OUTPUT}")
endfunction()