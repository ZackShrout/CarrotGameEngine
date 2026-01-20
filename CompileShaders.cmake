# Reusable function to compile GLSL shaders to SPIR-V using glslangValidator
# Keeps the same output directory and naming convention as your old code
find_program(DXC_EXECUTABLE
        NAMES dxc dxcompiler
)



if(DXC_EXECUTABLE)
    message(STATUS "Found DirectX Shader Compiler: ${DXC_EXECUTABLE}")
else()
    message(FATAL_ERROR "dxc is required but was not found.")
endif()

# Generate config header
configure_file(
        ${CMAKE_SOURCE_DIR}/src/Engine/HotReload/Config/ShaderToolsConfig.h.in
        ${CMAKE_BINARY_DIR}/src/Engine/HotReload/Config/ShaderToolsConfig.h
        @ONLY
)

#function(compile_glsl_to_spv TARGET_NAME GLSL_FILE OUTPUT_DIR)
#    # Make absolute path from source root
#    cmake_path(SET ABS_GLSL "${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_FILE}" NORMALIZE)
#
#    # Get base name without extension (triangle.vert → triangle.vert)
#    get_filename_component(BASE_NAME "${GLSL_FILE}" NAME)
#
#    # Output path: same as before: <OUTPUT_DIR>/<shader_name>.spv
#    cmake_path(SET SPV_OUTPUT "${OUTPUT_DIR}/${BASE_NAME}.spv" NORMALIZE)
#
#    add_custom_command(
#            OUTPUT "${SPV_OUTPUT}"
#            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
#            COMMAND Vulkan::glslangValidator -V "${ABS_GLSL}" -o "${SPV_OUTPUT}"
#            DEPENDS "${ABS_GLSL}"
#            COMMENT "glslangValidator → SPIR-V: ${BASE_NAME}"
#            VERBATIM
#    )
#
#    # Create a unique target per shader so we can depend on it cleanly
#    string(MAKE_C_IDENTIFIER "${BASE_NAME}" TARGET_ID)
#    set(SHADER_TARGET "${TARGET_NAME}_${TARGET_ID}_spv")
#
#    add_custom_target(${SHADER_TARGET} DEPENDS "${SPV_OUTPUT}")
#    add_dependencies(${TARGET_NAME} ${SHADER_TARGET})
#
#    # Optional: make the output file available as a property if needed later
#    set_property(TARGET ${SHADER_TARGET} PROPERTY OUTPUT_FILE "${SPV_OUTPUT}")
#endfunction()

# ----------------------------------------------------------------------------
# Compile HLSL to SPIR-V using DXC
# ----------------------------------------------------------------------------
function(compile_hlsl_to_spirv TARGET_NAME HLSL_FILE OUTPUT_DIR)
    if(NOT DXC_EXECUTABLE)
        message(FATAL_ERROR "DXC not found")
    endif()

    cmake_path(SET ABS_HLSL "${CMAKE_CURRENT_SOURCE_DIR}/${HLSL_FILE}" NORMALIZE)

    # Full base for target uniqueness
    get_filename_component(FULL_BASE "${HLSL_FILE}" NAME)  # triangle.vert.hlsl
    string(REPLACE "." "_" SANITIZED "${FULL_BASE}")
    string(MAKE_C_IDENTIFIER "${SANITIZED}" TARGET_ID)

    # Output base: strip .hlsl, keep stage if present
    string(REPLACE ".hlsl" "" OUTPUT_BASE "${FULL_BASE}")
    cmake_path(SET SPV_OUTPUT "${OUTPUT_DIR}/${OUTPUT_BASE}.spv" NORMALIZE)

    # Profile detection
    set(PROFILE "vs_6_7")
    string(TOLOWER "${HLSL_FILE}" LOWER)
    if(LOWER MATCHES ".*\\.frag\\.hlsl$")
        set(PROFILE "ps_6_7")
    elseif(LOWER MATCHES ".*\\.vert\\.hlsl$")
        set(PROFILE "vs_6_7")
    elseif(LOWER MATCHES ".*\\.comp\\.hlsl$")
        set(PROFILE "cs_6_7")
    endif()

    add_custom_command(
            OUTPUT "${SPV_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
            COMMAND ${DXC_EXECUTABLE}
            -spirv -T ${PROFILE} -E main -fvk-use-scalar-layout -v
            "${ABS_HLSL}" -Fo "${SPV_OUTPUT}"
            DEPENDS "${ABS_HLSL}"
            COMMENT "DXC → SPIR-V: ${OUTPUT_BASE} (${PROFILE})"
            VERBATIM
    )

    set(SHADER_TARGET "${TARGET_NAME}_${TARGET_ID}")

    add_custom_target(${SHADER_TARGET}
            DEPENDS "${SPV_OUTPUT}"
    )

    add_dependencies(${TARGET_NAME} ${SHADER_TARGET})

    set_property(TARGET ${SHADER_TARGET} PROPERTY OUTPUT_FILE "${SPV_OUTPUT}")
endfunction()