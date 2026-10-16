qt_add_library(oborcopy_core STATIC
    AppInfo.h AppInfo.cpp
    AppPaths.h AppPaths.cpp
    AppSettings.h AppSettings.cpp
    JobOptions.h JobOptions.cpp
    JobJson.h JobJson.cpp
    JobStore.h JobStore.cpp
    JobValidator.h JobValidator.cpp
    Platform.h Platform.cpp
    FsInfo.h FsInfo.cpp
    FileEntry.h
    PathFilter.h PathFilter.cpp
    FileScanner.h FileScanner.cpp
    FileComparator.h FileComparator.cpp
    Hash.h Hash.cpp
    SyncPlan.h SyncPlan.cpp
    SyncPlanner.h SyncPlanner.cpp
    SyncControl.h SyncControl.cpp
    RetryPolicy.h RetryPolicy.cpp
    Throttle.h Throttle.cpp
    Progress.h Progress.cpp
    RunReport.h RunReport.cpp
    RunLogger.h RunLogger.cpp
    ReportSink.h ReportSink.cpp
    FileCopier.h FileCopier.cpp
    SyncExecutor.h SyncExecutor.cpp
    SyncEngine.h SyncEngine.cpp
    Schedule.h Schedule.cpp
    ServiceFiles.h ServiceFiles.cpp
    ${OBORCOPY_GENERATED_DIR}/buildinfo.cpp
)

add_dependencies(oborcopy_core oborcopy_buildinfo)

target_include_directories(oborcopy_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBORCOPY_GENERATED_DIR})

target_link_libraries(oborcopy_core PUBLIC Qt6::Core)

target_compile_definitions(oborcopy_core PRIVATE
    OBORCOPY_VERSION_STRING="${PROJECT_VERSION}")

if(WIN32)
    target_compile_definitions(oborcopy_core PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
