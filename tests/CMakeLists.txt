add_compile_definitions(OMA_SCAN_PROGRAM="${CMAKE_SOURCE_DIR}/bin/omamusic-scan")

function(oma_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE omacore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

oma_test(tst_paths)
oma_test(tst_theme)
oma_test(tst_config)
oma_test(tst_track)
oma_test(tst_rowsmodel)
