# Prepare a local compatibility patch without changing the upstream submodule.
function(prepare_tinyexpr source_dir output_dir)
    file(MAKE_DIRECTORY "${output_dir}")
    file(READ "${source_dir}/tinyexpr.h" header)
    string(REPLACE "std::ranges::copy(parameters, ret->m_parameters.begin());"
        "std::copy(parameters.begin(), parameters.end(), ret->m_parameters.begin());" patched "${header}")
    if(patched STREQUAL header)
        message(FATAL_ERROR "TinyExpr compatibility patch no longer matches upstream")
    endif()
    # Expose the immutable builtin registry to the AST adapter without editing
    # the upstream submodule or evaluating generated expression strings.
    set(registry_anchor "    const std::set<te_variable>& get_variables_and_functions() const noexcept")
    string(FIND "${patched}" "${registry_anchor}" registry_offset)
    if(registry_offset EQUAL -1)
        message(FATAL_ERROR "TinyExpr builtin registry adapter no longer matches upstream")
    endif()
    string(REPLACE "${registry_anchor}"
        "    static const std::set<te_variable>& builtin_functions() noexcept { return m_functions; }\n\n${registry_anchor}"
        patched "${patched}")
    file(WRITE "${output_dir}/tinyexpr.h" "${patched}")
    configure_file("${source_dir}/tinyexpr.cpp" "${output_dir}/tinyexpr.cpp" COPYONLY)
endfunction()
