function(print_var variable)
    message(STATUS "${variable}=${${variable}}")
endfunction()

# There is a long outstanding issue with CMake where CMake does not support
# relative paths in CMAKE_PREFIX_PATH but does not report anything. Many hours
# have been wasted over this behavior. We are checking to save your this to
# save everyone some headache. :)
function(check_prefix_path)
    if(CMAKE_PREFIX_PATH)
        foreach(path ${CMAKE_PREFIX_PATH})
            if(NOT IS_ABSOLUTE ${path})
                message(FATAL_ERROR "CMake does not support relative paths for CMAKE_PREFIX_PATH! [${path}]")
            endif()
        endforeach()
    endif()
endfunction()

# This macro ensures that there is always an explicitly set build type.
# If CMAKE_BUILD_TYPE is NOT set during configure, CMAKE_BUILD_TYPE will
# now default to 'Release'
macro(enforce_build_type_is_set)
    set(default_build_type "Release")
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
        set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
                STRING "Choose the type of build." FORCE)
    endif()
endmacro()

# Adds an option to cmake configure to automatically build with clang
# tidy enabled. NOTE: The clang tidy version is explicitly pinned.
macro(add_clang_tidy_support_option)
    option(WITH_TIDY "Build with clang tidy checks enabled (This will increase your build time)" ON)
    if(WITH_TIDY)
        find_program(
                CLANG_TIDY
                NAMES
                clang-tidy-11
                clang-tidy
                REQUIRED
        )
        if(CLANG_TIDY)
            execute_process(
                    COMMAND ${CLANG_TIDY} --version
                    OUTPUT_VARIABLE clang_tidy_version_output
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            string(REGEX REPLACE
                    ".*LLVM version ([.0-9]+).*"
                    "\\1"
                    clang_tidy_version
                    "${clang_tidy_version_output}"
                    )
            if(clang_tidy_version MATCHES "^11.*")
                message(STATUS "Found clang-tidy ${CLANG_TIDY} [${clang_tidy_version}]")
                set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
                set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY})
            else()
                message(FATAL_ERROR "Found clang-tidy but it reports an incorrect version: ${clang_tidy_version}")
            endif()
        endif()
    endif()
endmacro()

function(get_git_info)
    ################################################################################
    # VCS info
    ################################################################################
    find_package(Git QUIET)
    find_program(GIT_SCM git DOC "Git version control")
    mark_as_advanced(GIT_SCM)
    find_file(GITDIR NAMES .git PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)
    if (GIT_SCM AND GITDIR)
        # the commit's SHA1, and whether the building workspace was dirty or not
        # describe --match=NeVeRmAtCh --always --tags --abbrev=40 --dirty
        execute_process(COMMAND
                "${GIT_EXECUTABLE}" --no-pager describe --tags --always --dirty
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_SHA1
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        # branch
        execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_BRANCH
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        # the date of the commit
        execute_process(COMMAND
                "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_DATE
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

        execute_process(COMMAND
                "${GIT_EXECUTABLE}" describe --tags --abbrev=0
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_TAG
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

        # the subject of the commit
        execute_process(COMMAND
                "${GIT_EXECUTABLE}" log -1 --format=%s
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE GIT_COMMIT_SUBJECT
                ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        # remove # from subject
        string(REGEX REPLACE "[\#\"]+"
                "" GIT_COMMIT_SUBJECT
                ${GIT_COMMIT_SUBJECT})
    else()
        message(STATUS "Not in a git repo")
        set(GIT_SHA1 "UNKNOWN")
        set(GIT_DATE "UNKNOWN")
        set(GIT_COMMIT_SUBJECT "UNKNOWN")
        set(GIT_BRANCH "UNKNOWN")
        set(GIT_TAG "UNKNOWN")
    endif()

    add_library(git-info INTERFACE)
    target_compile_definitions(git-info INTERFACE
            GIT_COMMIT_HASH="${GIT_SHA1}"
            GIT_COMMIT_DATE="${GIT_DATE}"
            GIT_TAG="${GIT_TAG}"
            GIT_COMMIT_SUBJECT="${GIT_COMMIT_SUBJECT}"
            GIT_BRANCH="${GIT_BRANCH}"
            )

endfunction()

#find_package(Git REQUIRED) # no need for this msg. It comes from cmake.findgit()
#
#find_program(GIT_SCM git DOC "Git version control")
#mark_as_advanced(GIT_SCM)
#find_file(GITDIR NAMES .git PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)
#if (GIT_SCM AND GITDIR)
#
## the commit's SHA1, and whether the building workspace was dirty or not
## describe --match=NeVeRmAtCh --always --tags --abbrev=40 --dirty
#execute_process(COMMAND
#  "${GIT_EXECUTABLE}" --no-pager describe --tags --always --dirty
#  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#  OUTPUT_VARIABLE GIT_SHA1
#  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
#
## branch
#execute_process(
#  COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
#  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#  OUTPUT_VARIABLE GIT_BRANCH
#  OUTPUT_STRIP_TRAILING_WHITESPACE
#)
#
## the date of the commit
#execute_process(COMMAND
#  "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
#  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#  OUTPUT_VARIABLE GIT_DATE
#  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
#
## the subject of the commit
#execute_process(COMMAND
#  "${GIT_EXECUTABLE}" log -1 --format=%s
#  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#  OUTPUT_VARIABLE GIT_COMMIT_SUBJECT
#  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
#
#
#
#add_definitions("-DGIT_COMMIT_HASH=\"${GIT_SHA1}\"")
#add_definitions("-DGIT_COMMIT_DATE=\"${GIT_DATE}\"")
#add_definitions("-DGIT_COMMIT_SUBJECT=\"${GIT_COMMIT_SUBJECT}\"")
#add_definitions("-DGIT_BRANCH=\"${GIT_BRANCH}\"")
#add_definitions("-DJPSVIS_VERSION=\"${JPSVIS_VERSION}\"")
#else()
#    message(STATUS "Not in a git repo")
#endif()
