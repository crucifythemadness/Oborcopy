set(OBORCOPY_RC "")
if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/resources/app.rc.in ${OBORCOPY_GENERATED_DIR}/app.rc @ONLY)
    set(OBORCOPY_RC ${OBORCOPY_GENERATED_DIR}/app.rc)
endif()

qt_add_executable(Oborcopy WIN32
    main.cpp
    Theme.h Theme.cpp
    IconFactory.h IconFactory.cpp
    GuiUtil.h GuiUtil.cpp
    CollapsibleSection.h CollapsibleSection.cpp
    ModeCard.h ModeCard.cpp
    PathPicker.h PathPicker.cpp
    JobListWidget.h JobListWidget.cpp
    BasicOptionsPage.h BasicOptionsPage.cpp
    AdvancedOptionsPage.h AdvancedOptionsPage.cpp
    ProgressPanel.h ProgressPanel.cpp
    SyncController.h SyncController.cpp
    ResultTreeView.h ResultTreeView.cpp
    PreviewDialog.h PreviewDialog.cpp
    SummaryDialog.h SummaryDialog.cpp
    HistoryDialog.h HistoryDialog.cpp
    ServiceController.h ServiceController.cpp
    ServiceStatusPill.h ServiceStatusPill.cpp
    ServiceDialog.h ServiceDialog.cpp
    ScheduleEditorDialog.h ScheduleEditorDialog.cpp
    AboutDialog.h AboutDialog.cpp
    MainWindow.h MainWindow.cpp
    ${OBORCOPY_RC}
)

qt_add_resources(Oborcopy "app_icons"
    PREFIX "/icons"
    BASE ${CMAKE_SOURCE_DIR}/resources
    FILES ${CMAKE_SOURCE_DIR}/resources/oborcopy.png)

target_link_libraries(Oborcopy PRIVATE oborcopy_core Qt6::Widgets)

if(WIN32)
    # Service control manager, elevation (ShellExecuteEx) and GetUserNameEx.
    target_link_libraries(Oborcopy PRIVATE advapi32 shell32 secur32)
    target_compile_definitions(Oborcopy PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(Oborcopy PROPERTIES OUTPUT_NAME oborcopy)
endif()

install(TARGETS Oborcopy RUNTIME DESTINATION bin)
