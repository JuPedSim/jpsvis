#########################################################
#                    INSTALL                            #
#########################################################
function (cpack_write_deb_config)
    message(STATUS "Package generation - LINUX")
#    list(APPEND CPACK_GENERATOR "DEB")
#    set (CPACK_GENERATOR  ${CPACK_GENERATOR} PARENT_SCOPE)
#    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON PARENT_SCOPE)
#    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-dev (>=1.65), zlib1g-dev" PARENT_SCOPE)
#    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://jupedsim.org" PARENT_SCOPE)
#    set(CPACK_DEBIAN_PACKAGE_SUGGESTS, "jpsvis, jpseditor" PARENT_SCOPE)
#    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional" PARENT_SCOPE)
#    set(CPACK_DEBIAN_PACKAGE_SECTION "science" PARENT_SCOPE)
#    set(CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR} PARENT_SCOPE)
#    set( CPACK_DEBIAN_PACKAGE_MAINTAINER "David Haensel" PARENT_SCOPE)
#    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "JuPedSim: framework for simulation and analysis of pedestrian dynamics" PARENT_SCOPE)
endfunction()

function (cpack_write_osx_config)
    message(STATUS "Package generation - MacOS")
    list(APPEND CPACK_GENERATOR "DragNDrop")
    set (CPACK_GENERATOR  ${CPACK_GENERATOR} PARENT_SCOPE)
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/jpscore/forms/background.png" PARENT_SCOPE)
    set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK ON  PARENT_SCOPE)
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/jpscore/forms/JPScore.icns" PARENT_SCOPE)
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}" PARENT_SCOPE)
    set(CPACK_SYSTEM_NAME "OSX" PARENT_SCOPE)
    set(CPACK_GENERATOR "DragNDrop")

    SET(MACOSX_BUNDLE_ICON_FILE jpsvis.icns)
    SET(MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2015-2021 Forschungszentrum Juelich. All rights reserved.")
    SET(MACOSX_BUNDLE_INFO_STRING "Visualisation module for JuPedSim.")
    SET(MACOSX_BUNDLE_BUNDLE_NAME "JPSvis")
    SET(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    SET(MACOSX_BUNDLE_LONG_VERSION_STRING "version ${PROJECT_VERSION}")
    SET(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

    set_target_properties(jpsvis PROPERTIES
            MACOSX_BUNDLE_BUNDLE_VERSION       "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            )

    set_target_properties(jpsvis PROPERTIES
            INSTALL_RPATH @executable_path/../Frameworks
            )
    get_target_property(mocExe Qt5::moc IMPORTED_LOCATION)
    get_filename_component(qtBinDir "${mocExe}" DIRECTORY)

    find_program(DEPLOYQT_EXECUTABLE macdeployqt PATHS "${qtBinDir}" NO_DEFAULT_PATH)

    configure_file(${CMAKE_SOURCE_DIR}/cmake_modules/deployapp.cmake.in deployapp.cmake @ONLY)
    install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/deployapp.cmake)

    #   TODO copy 3rd party licences to bundle
    #    install(DIRECTORY "${Qt5Core_DIR}/../../../License*" DESTINATION "Licenses/Qt_Licenses")
    #    install(DIRECTORY "${OpenCV_DIR}/etc/licenses" DESTINATION "Licenses/OpenCV_Licenses")
    #    install(FILES "${OpenCV_DIR}/LICENSE/" DESTINATION "Licenses/OpenCV_Licenses")

    # install Qwt and OpenCV
    install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"\$<TARGET_FILE:jpsvis>\"  \"\" \"\")
    ")

endfunction()

function (cpack_write_windows_config)
    message(STATUS "Package generation - Windows")
    set(CPACK_GENERATOR "NSIS" CACHE STRING "Generator used by CPack")

    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/forms/jpsvis.ico")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_DISPLAY_NAME "jpsVis")
    set(CPACK_NSIS_PACKAGE_NAME "jpsVis")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "${CMAKE_SOURCE_DIR}/forms/jpsvis.ico")
    set(CPACK_NSIS_HELP_LINK "http://jupedsim.org")
    set(CPACK_NSIS_URL_INFO_ABOUT ${CPACK_NSIS_HELP_LINK})
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    set (WINDEPLOYQT_APP \"${Qt5Core_DIR}/../../../bin/windeployqt\")
    install(CODE "
        message(\"\${CMAKE_INSTALL_PREFIX}/jpsvis.exe\")
        execute_process(COMMAND ${WINDEPLOYQT_APP} \"\${CMAKE_INSTALL_PREFIX}/bin/jpsvis.exe\")
    ")

    # NOTE: Paths might be platform specific
    install(DIRECTORY "${Qt5Core_DIR}/../../../../../Licenses" DESTINATION "Licenses/Qt_Licenses")

    install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/jpsvis${CMAKE_EXECUTABLE_SUFFIX}\"  \"\" \"\")
    ")

    # some configs for installer
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/jpscore/forms/JPScore.ico" PARENT_SCOPE)
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/jpscore/forms/JPScore.ico" PARENT_SCOPE)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON PARENT_SCOPE)
    set(CPACK_NSIS_MODIFY_PATH ON PARENT_SCOPE)
    set(CPACK_NSIS_HELP_LINK "http://www.jupedsim.org/jupedsim_install_on_windows.html" PARENT_SCOPE)
    set(CPACK_NSIS_URL_INFO_ABOUT "http://www.jupedsim.org/" PARENT_SCOPE)
    set(CPACK_NSIS_DISPLAY_NAME ${CMAKE_PROJECT_NAME} PARENT_SCOPE)
    # ----------------------------
endfunction()

function (cpack_write_config)
    message(STATUS "Cpack write configs")
#    set(CPACK_COMPONENTS_ALL applications PARENT_SCOPE)
#    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE" PARENT_SCOPE)
#    set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md" PARENT_SCOPE)
#    set(CPACK_COMPONENTS_ALL applications jpscore_samples jpsreport_samples PARENT_SCOPE)
#    set(CPACK_COMPONENT_CTDATA_GROUP "Sample files" PARENT_SCOPE)
#    set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "jpscore and jpsreport" PARENT_SCOPE)
#    set(CPACK_COMPONENT_GROUP_DATA_DESCRIPTION "Sample files" PARENT_SCOPE)
#    set(CPACK_COMPONENT_DATA_FILES_DESCRIPTION "Sample files to get started" PARENT_SCOPE)
#    set(CPACK_PACKAGE_DESCRIPTION "jpscore and jpsreport" PARENT_SCOPE)
#    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "JuPedSim: framework for simulation and analysis of pedestrian dynamics" PARENT_SCOPE)
#    set(CPACK_PACKAGE_VENDOR "Forschungszentrum Juelich GmbH" PARENT_SCOPE)
#    set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME} PARENT_SCOPE)
#    set(CPACK_PACKAGE_CONTACT "m.chraibi@fz-juelich.de" PARENT_SCOPE)
#    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION} PARENT_SCOPE)
#    set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${PROJECT_VERSION}" PARENT_SCOPE)
#    set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "jpsvis-installer-${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VENDOR "Forschungszentrum Juelich GmbH")
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERISON_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERISON_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERISON_PATCH})
    SET(CPACK_PACKAGE_DESCRIPTION "Visualisation module for JuPedSim")
    SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Visualisation module for  JuPedSim, a framework for simulation and analysis of pedestrian dynamics")
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
    set(CPACK_PACKAGE_HOMEPAGE_URL "http://jupedsim.org")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
    set(CPACK_PACKAGE_EXECUTABLES "jpsvis;JPSVis")
    set(CPACK_MONOLITHIC_INSTALL TRUE)
    set(CPACK_CREATE_DESKTOP_LINKS jpsVis)
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    SET(CPACK_PACKAGE_CONTACT "m.chraibi@fz-juelich.de")

    include(InstallRequiredSystemLibraries)
    include(GNUInstallDirs)

    install(TARGETS jpsvis
            BUNDLE DESTINATION .
            )

    print_var(CPACK_SOURCE_PACKAGE_FILE_NAME)

    include(CPack)
endfunction()
