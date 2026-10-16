# The scheduler service: Qt Core only, so it stays small.
qt_add_executable(oborcopy-service
    main.cpp
    Scheduler.h Scheduler.cpp
    ServiceLog.h ServiceLog.cpp
)

target_link_libraries(oborcopy-service PRIVATE oborcopy_core)

if(WIN32)
    target_sources(oborcopy-service PRIVATE WinService.h WinService.cpp WinServiceAdmin.cpp)
    target_link_libraries(oborcopy-service PRIVATE advapi32 credui ole32)
    target_compile_definitions(oborcopy-service PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_sources(oborcopy-service PRIVATE PosixDaemon.h PosixDaemon.cpp)
endif()

install(TARGETS oborcopy-service RUNTIME DESTINATION bin)
