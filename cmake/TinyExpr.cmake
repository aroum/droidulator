# Prepare a local compatibility patch without changing the upstream submodule.
function(prepare_tinyexpr source_dir output_dir)
    file(MAKE_DIRECTORY "${output_dir}")
    file(READ "${source_dir}/tinyexpr.h" header)
    string(REPLACE "std::ranges::copy(parameters, ret->m_parameters.begin());"
        "std::copy(parameters.begin(), parameters.end(), ret->m_parameters.begin());" patched "${header}")
    if(patched STREQUAL header)
        message(FATAL_ERROR "TinyExpr compatibility patch no longer matches upstream")
    endif()
    file(WRITE "${output_dir}/tinyexpr.h" "${patched}")
    configure_file("${source_dir}/tinyexpr.cpp" "${output_dir}/tinyexpr.cpp" COPYONLY)
endfunction()
