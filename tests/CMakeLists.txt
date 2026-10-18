function(oborcopy_add_test name)
    qt_add_executable(${name} ${name}.cpp TestUtil.h)
    target_link_libraries(${name} PRIVATE oborcopy_core Qt6::Test)
    if(WIN32)
        target_compile_definitions(${name} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

oborcopy_add_test(tst_comparator)
oborcopy_add_test(tst_retrypolicy)
oborcopy_add_test(tst_pathfilter)
oborcopy_add_test(tst_jobstore)
oborcopy_add_test(tst_planner)
oborcopy_add_test(tst_syncengine)
oborcopy_add_test(tst_schedule)

# End-to-end test of the Linux scheduler service against a real systemd user
# session (scripts/test-linux-service.ps1). Not registered with ctest.
if(UNIX AND NOT APPLE)
    qt_add_executable(oborcopy-servicectl-test
        servicectl_harness.cpp
        ${CMAKE_SOURCE_DIR}/src/gui/ServiceController.h
        ${CMAKE_SOURCE_DIR}/src/gui/ServiceController.cpp)
    target_include_directories(oborcopy-servicectl-test PRIVATE ${CMAKE_SOURCE_DIR}/src/gui)
    target_link_libraries(oborcopy-servicectl-test PRIVATE oborcopy_core)
endif()
