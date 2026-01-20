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

# ----------------------------------------------------------------------------
# Compile HLSL to SPIR-V using DXC
# ----------------------------------------------------------------------------
function(compile_hlsl_to_spirv TARGET_NAME HLSL_FILE OUTPUT_DIR)
    if(NOT DXC_EXECUTABLE)
        message(FATAL_ERROR "DXC not found - cannot compile HLSL to SPIR-V")
    endif()

    cmake_path(SET ABS_HLSL "${CMAKE_CURRENT_SOURCE_DIR}/${HLSL_FILE}" NORMALIZE)

    # Get filename without extension (triangle.vert.hlsl → triangle.vert)
    get_filename_component(BASE_NAME_WE "${HLSL_FILE}" NAME_WE)

    # Output: same style as before: <OUTPUT_DIR>/<base>.spv
    cmake_path(SET SPV_OUTPUT "${OUTPUT_DIR}/${BASE_NAME_WE}.spv" NORMALIZE)

    # Determine shader stage/profile from file extension
    #   .vert.hlsl → vs_6_7   (or vs_6_0 / vs_6_8 depending on your needs)
    #   .frag.hlsl → ps_6_7
    #   .comp.hlsl → cs_6_7
    # You can later make this more sophisticated (e.g. read first line pragma)
    set(PROFILE "vs_6_7")  # default to vertex for now
    string(TOLOWER "${HLSL_FILE}" HLSL_LOWER)
    if("${HLSL_LOWER}" MATCHES ".*\\.frag\\.hlsl$")
        set(PROFILE "ps_6_7")
    elseif("${HLSL_LOWER}" MATCHES ".*\\.comp\\.hlsl$")
        set(PROFILE "cs_6_7")
    elseif("${HLSL_LOWER}" MATCHES ".*\\.vert\\.hlsl$")
        set(PROFILE "vs_6_7")
    endif()

    # Recommended minimal flags for clean Vulkan SPIR-V in 2025/2026
    # -spirv             → enable SPIR-V output
    # -T <profile>       → target shader model / stage
    # -E main            → entry point (your shader uses main)
    # -fvk-use-scalar-layout  → use scalar alignment (more natural for Vulkan)
    # Optional debug: -Zi -Od (debug info, no opt) or -O3 for release
    add_custom_command(
            OUTPUT "${SPV_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
            COMMAND ${DXC_EXECUTABLE}
            -spirv
            -T ${PROFILE}
            -E main
            -fvk-use-scalar-layout
            # -Zi -Od               # uncomment for debug builds if you want PDB-like info
            # -O3                   # uncomment for release/optimized
            "${ABS_HLSL}"
            -Fo "${SPV_OUTPUT}"
            DEPENDS "${ABS_HLSL}"
            COMMENT "DXC HLSL → SPIR-V: ${BASE_NAME_WE} (${PROFILE})"
            VERBATIM
    )

    # Same target/dependency pattern as your GLSL function
    string(MAKE_C_IDENTIFIER "${BASE_NAME_WE}" TARGET_ID)
    set(SHADER_TARGET "${TARGET_NAME}_${TARGET_ID}_spv")

    add_custom_target(${SHADER_TARGET} DEPENDS "${SPV_OUTPUT}")
    add_dependencies(${TARGET_NAME} ${SHADER_TARGET})

    set_property(TARGET ${SHADER_TARGET} PROPERTY OUTPUT_FILE "${SPV_OUTPUT}")
endfunction()