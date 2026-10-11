qt_add_executable(oborcopy-cli
    main.cpp
    CliRunner.h CliRunner.cpp
)

target_link_libraries(oborcopy-cli PRIVATE oborcopy_core)

if(WIN32)
    target_compile_definitions(oborcopy-cli PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

install(TARGETS oborcopy-cli RUNTIME DESTINATION bin)
