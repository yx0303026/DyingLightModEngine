include_guard(GLOBAL)

function(dlmod_set_output_directories target)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_architecture "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(_architecture "x86")
    else()
        message(FATAL_ERROR "Only x86 and x64 Windows targets are supported")
    endif()

    set(_output_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../build/${_architecture}")
    get_filename_component(_output_root "${_output_root}" ABSOLUTE)

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_output_root}"
        LIBRARY_OUTPUT_DIRECTORY "${_output_root}"
        ARCHIVE_OUTPUT_DIRECTORY "${_output_root}"
    )

    foreach(_configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${_configuration} "${_output_root}/${_configuration}"
            LIBRARY_OUTPUT_DIRECTORY_${_configuration} "${_output_root}/${_configuration}"
            ARCHIVE_OUTPUT_DIRECTORY_${_configuration} "${_output_root}/${_configuration}"
        )
    endforeach()
endfunction()
