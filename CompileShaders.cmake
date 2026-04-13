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

if(APPLE)
    find_program(METAL_SHADERCONVERTER_EXECUTABLE
            NAMES metal-shaderconverter
            PATHS "/usr/bin" "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin"
    )

    if(METAL_SHADERCONVERTER_EXECUTABLE)
        message(STATUS "Found Metal Shader Converter: ${METAL_SHADERCONVERTER_EXECUTABLE}")
    else()
        message(FATAL_ERROR "metal-shaderconverter is required on macOS but was not found. "
                "Install Xcode command line tools or check your PATH.")
    endif()
endif()

# Generate config header
configure_file(
        ${CMAKE_SOURCE_DIR}/src/Engine/HotReload/Config/ShaderToolsConfig.h.in
        ${CMAKE_BINARY_DIR}/src/Engine/HotReload/Config/ShaderToolsConfig.h
        @ONLY
)

# ----------------------------------------------------------------------------
# Compile HLSL to SPIR-V using DXC
# ----------------------------------------------------------------------------
function(compile_hlsl_to_spirv TARGET_NAME HLSL_FILE OUTPUT_DIR)
    if(NOT DXC_EXECUTABLE)
        message(FATAL_ERROR "DXC not found")
    endif()

    cmake_path(SET ABS_HLSL "${CMAKE_CURRENT_SOURCE_DIR}/${HLSL_FILE}" NORMALIZE)

    # Full base for target uniqueness
    get_filename_component(FULL_BASE "${HLSL_FILE}" NAME)
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
            -spirv -T ${PROFILE} -E main -fvk-use-scalar-layout
            -DCARROT_CLIP_SPACE_Y_SIGN=-1.0
            "${ABS_HLSL}" -Fo "${SPV_OUTPUT}" -DVULKAN
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

# ----------------------------------------------------------------------------
# Compile HLSL to .metallib using DXC -> DXIL -> metal-shaderconverter
# ----------------------------------------------------------------------------
function(compile_hlsl_to_metallib TARGET_NAME HLSL_FILE OUTPUT_DIR)
    if(NOT APPLE)
        return()  # Should never be called on non-Apple, but guard anyway
    endif()

    if(NOT DXC_EXECUTABLE OR NOT METAL_SHADERCONVERTER_EXECUTABLE)
        message(FATAL_ERROR "DXC or metal-shaderconverter not found for Metal pipeline")
    endif()

    cmake_path(SET ABS_HLSL "${CMAKE_CURRENT_SOURCE_DIR}/${HLSL_FILE}" NORMALIZE)

    # Naming / sanitization (same as the SPIR-V version)
    get_filename_component(FULL_BASE "${HLSL_FILE}" NAME)
    string(REPLACE "." "_" SANITIZED "${FULL_BASE}")
    string(MAKE_C_IDENTIFIER "${SANITIZED}" TARGET_ID)

    string(REPLACE ".hlsl" "" OUTPUT_BASE "${FULL_BASE}")
    cmake_path(SET DXIL_OUTPUT "${OUTPUT_DIR}/${OUTPUT_BASE}.dxil" NORMALIZE)
    cmake_path(SET METALLIB_OUTPUT "${OUTPUT_DIR}/${OUTPUT_BASE}.metallib" NORMALIZE)

    cmake_path(SET JSON_OUTPUT "${OUTPUT_DIR}/${OUTPUT_BASE}.json" NORMALIZE)

    # Determine shader profile (same logic as SPIR-V version)
    set(PROFILE "vs_6_7")
    string(TOLOWER "${HLSL_FILE}" LOWER)
    if(LOWER MATCHES ".*\\.frag\\.hlsl$")
        set(PROFILE "ps_6_7")
    elseif(LOWER MATCHES ".*\\.vert\\.hlsl$")
        set(PROFILE "vs_6_7")
    elseif(LOWER MATCHES ".*\\.comp\\.hlsl$")
        set(PROFILE "cs_6_7")
    endif()

    # Step 1: HLSL -> DXIL
    add_custom_command(
            OUTPUT "${DXIL_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
            COMMAND ${DXC_EXECUTABLE}
            -T ${PROFILE} -E main
            -DMETAL
            -DCARROT_USE_ROOT_SIGNATURES
            -Zi -Qembed_debug
            "${ABS_HLSL}" -Fo "${DXIL_OUTPUT}"
            DEPENDS "${ABS_HLSL}"
            COMMENT "DXC → DXIL: ${OUTPUT_BASE} (${PROFILE})"
            VERBATIM
    )

    # Step 2: DXIL -> .metallib
    add_custom_command(
            OUTPUT "${METALLIB_OUTPUT}"
            COMMAND ${METAL_SHADERCONVERTER_EXECUTABLE}
            "${DXIL_OUTPUT}" -o "${METALLIB_OUTPUT}"
            --output-reflection-file=${JSON_OUTPUT}
            --minimum-os-build-version 14.0
            # --minimum-os-build-version 13.0     # e.g. target macOS 13+
            # --output-reflection-file "${METALLIB_OUTPUT}.json"   # if you want reflection data
            # --entry-point-name main             # usually not needed if entry is 'main'
            DEPENDS "${DXIL_OUTPUT}"
            COMMENT "metal-shaderconverter → metallib: ${OUTPUT_BASE}"
            VERBATIM
    )

    set(SHADER_TARGET "${TARGET_NAME}_${TARGET_ID}_metal")

    add_custom_target(${SHADER_TARGET}
            DEPENDS "${METALLIB_OUTPUT}"
    )

    add_dependencies(${TARGET_NAME} ${SHADER_TARGET})

    # Optional: store the final .metallib path on the target for later use if needed
    set_property(TARGET ${SHADER_TARGET} PROPERTY OUTPUT_FILE "${METALLIB_OUTPUT}")
endfunction()

# ----------------------------------------------------------------------------
# Compile HLSL to DXIL using DXC
# ----------------------------------------------------------------------------
function(compile_hlsl_to_dxil TARGET_NAME HLSL_FILE OUTPUT_DIR)
    if(NOT DXC_EXECUTABLE)
        message(FATAL_ERROR "DXC not found")
    endif()

    cmake_path(SET ABS_HLSL "${CMAKE_CURRENT_SOURCE_DIR}/${HLSL_FILE}" NORMALIZE)

    # Naming / sanitization (same as the SPIR-V version)
    get_filename_component(FULL_BASE "${HLSL_FILE}" NAME)
    string(REPLACE "." "_" SANITIZED "${FULL_BASE}")
    string(MAKE_C_IDENTIFIER "${SANITIZED}" TARGET_ID)

    string(REPLACE ".hlsl" "" OUTPUT_BASE "${FULL_BASE}")
    cmake_path(SET DXIL_OUTPUT "${OUTPUT_DIR}/${OUTPUT_BASE}.dxil" NORMALIZE)

    # Determine shader profile (same logic as SPIR-V version)
    set(PROFILE "vs_6_7")
    string(TOLOWER "${HLSL_FILE}" LOWER)
    if(LOWER MATCHES ".*\\.frag\\.hlsl$")
        set(PROFILE "ps_6_7")
    elseif(LOWER MATCHES ".*\\.vert\\.hlsl$")
        set(PROFILE "vs_6_7")
    elseif(LOWER MATCHES ".*\\.comp\\.hlsl$")
        set(PROFILE "cs_6_7")
    endif()

    # Step 1: HLSL -> DXIL
    add_custom_command(
            OUTPUT "${DXIL_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
            COMMAND ${DXC_EXECUTABLE}
            -T ${PROFILE} -E main
            -DDX12
            -DCARROT_USE_ROOT_SIGNATURES
            # Add any other flags you need, e.g. -Zi for debug, -Od, etc.
            # -HV 2021   # HLSL version if needed
            "${ABS_HLSL}" -Fo "${DXIL_OUTPUT}"
            DEPENDS "${ABS_HLSL}"
            COMMENT "DXC → DXIL: ${OUTPUT_BASE} (${PROFILE})"
            VERBATIM
    )

    set(SHADER_TARGET "${TARGET_NAME}_${TARGET_ID}_dxil")

    add_custom_target(${SHADER_TARGET}
            DEPENDS "${DXIL_OUTPUT}"
    )

    add_dependencies(${TARGET_NAME} ${SHADER_TARGET})

    set_property(TARGET ${SHADER_TARGET} PROPERTY OUTPUT_FILE "${DXIL_OUTPUT}")
endfunction()
