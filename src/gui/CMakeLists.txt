set(OBORCOPY_RC "")
if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/resources/app.rc.in ${OBORCOPY_GENERATED_DIR}/app.rc @ONLY)
    set(OBORCOPY_RC ${OBORCOPY_GENERATED_DIR}/app.rc)
endif()

qt_add_executable(Oborcopy WIN32
    main.cpp
    Theme.h Theme.cpp
    Language.h Language.cpp
    GuiText.h GuiText.cpp
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
    DevelopersDialog.h DevelopersDialog.cpp
    MainWindow.h MainWindow.cpp
    ${OBORCOPY_RC}
)

qt_add_resources(Oborcopy "app_icons"
    PREFIX "/icons"
    BASE ${CMAKE_SOURCE_DIR}/resources
    FILES
        ${CMAKE_SOURCE_DIR}/resources/oborcopy.png
        ${CMAKE_SOURCE_DIR}/resources/Claude_Logo.png
        ${CMAKE_SOURCE_DIR}/resources/CrucifyTheMadness_Logo.png)

# German UI: our translations plus Qt's own (standard buttons, file dialogs), both
# compiled into the executable under :/i18n so nothing extra has to be deployed.
qt_add_translations(Oborcopy
    TS_FILES ${CMAKE_SOURCE_DIR}/translations/oborcopy_de.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_OPTIONS -no-obsolete -locations none)

set(QT_TRANSLATIONS_DIR "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}")
if(EXISTS "${QT_TRANSLATIONS_DIR}/qtbase_de.qm")
    qt_add_resources(Oborcopy "qt_translations"
        PREFIX "/i18n"
        BASE ${QT_TRANSLATIONS_DIR}
        FILES ${QT_TRANSLATIONS_DIR}/qtbase_de.qm)
else()
    message(WARNING "qtbase_de.qm not found in '${QT_TRANSLATIONS_DIR}' - Qt's standard dialogs stay English")
endif()

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
