include(CMakeParseArguments)

set(DEPENDENCY_CACHE_DIR ${CMAKE_BINARY_DIR}/dependencies CACHE PATH "Cache downloaded deps in this directory")
set(DEPENDENCY_UNTAR_DIR ${CMAKE_BINARY_DIR}/dependencies CACHE PATH "Where to untar deps")

if(APPLE)
  set(ARCHSTR "darwin-x86_64")
elseif(WIN32)
  set(OS "windows-i386")
  if (CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(ARCHSTR "windows-x86_64")
  else()
    set(ARCHSTR "windows-i386")
  endif()
elseif(OPENELEC)
  if (OE_ARCH STREQUAL "x86_64")
    set(ARCHSTR "linux-openelec-x86_64")
  elseif(OE_ARCH STREQUAL "armv7")
    set(ARCHSTR "linux-openelec-armv7")
  endif()
elseif(UNIX)
  set(ARCHSTR ${JELLYFIN_BUILD_TARGET})
endif(APPLE)

option(ENABLE_CODECS "Enable downloading for Codecs on Demand" OFF)
if(ENABLE_CODECS)
  add_definitions(-DHAVE_CODEC_MANIFEST)
  add_definitions(-DEAE_VERSION=133)
endif()

function(get_content_of_url)
  set(ARGS URL CONTENT_VAR FILENAME)
  cmake_parse_arguments(CU "ALWAYS" "${ARGS}" "" ${ARGN})
  if(NOT DEFINED CU_FILENAME)
    get_filename_component(CU_FILENAME ${CU_URL} NAME)
  endif(NOT DEFINED CU_FILENAME)

  if(EXISTS ${DEPENDENCY_CACHE_DIR}/${CU_FILENAME})
    if(CU_ALWAYS)
      file(REMOVE ${DEPENDENCY_CACHE_DIR}/${CU_FILENAME})
    else()
      file(STRINGS ${DEPENDENCY_CACHE_DIR}/${CU_FILENAME} CVAR LIMIT_COUNT 1)
    endif()
  endif()

  if(NOT CVAR)
    message(STATUS "Downloading ${CU_URL} to ${CU_FILENAME}...")

    file(
      DOWNLOAD ${CU_URL} ${DEPENDENCY_CACHE_DIR}/${CU_FILENAME}
      STATUS DL_STATUS
      LOG DL_LOG
    )

    list(GET DL_STATUS 0 SUCCESS)
    if(NOT SUCCESS EQUAL 0)
      list(GET DL_STATUS 1 ERROR_MESSAGE)
      file(REMOVE ${DEPENDENCY_CACHE_DIR}/${CU_FILENAME})
      message(FATAL_ERROR "Failed to download ${CU_URL}: ${ERROR_MESSAGE}\n${DL_LOG}")
    endif(NOT SUCCESS EQUAL 0)

    get_content_of_url(URL ${CU_URL} CONTENT_VAR CVAR FILENAME ${CU_FILENAME})
  endif()
  set(${CU_CONTENT_VAR} ${CVAR} PARENT_SCOPE)
endfunction(get_content_of_url)

function(download_deps DD_NAME)
  set(ARGS DIRECTORY BUILD_NUMBER
                     ARTIFACTNAME
                     VARIANT
                     DEPHASH_VAR
                     ARCHSTR
                     DYLIB_SCRIPT_PATH
                     BASE_URL
                     TOKEN)

  cmake_parse_arguments(DD "NO_HASH_FILE" "${ARGS}" "" ${ARGN})

  if(NOT DEFINED DD_VARIANT)
    set(DD_VARIANT "release")
  endif(NOT DEFINED DD_VARIANT)

  if(NOT DEFINED DD_ARTIFACTNAME)
    set(DD_ARTIFACTNAME ${DD_NAME})
  endif(NOT DEFINED DD_ARTIFACTNAME)

  if(NOT DEFINED DD_BUILD_NUMBER)
    set(DD_BUILD_NUMBER "latest")
  endif(NOT DEFINED DD_BUILD_NUMBER)

  if(NOT DEFINED DD_ARCHSTR)
    set(DD_ARCHSTR ${ARCHSTR})
  endif(NOT DEFINED DD_ARCHSTR)

  if(DD_BUILD_NUMBER STREQUAL latest)
    set(DD_ALWAYS_DOWNLOAD ALWAYS)
  endif()

  if(NOT DEFINED DD_TOKEN)
    set(DD_TOKEN jellyfin-dependencies)
  endif()

  if(NOT DEFINED DD_BASE_URL)
    set(DD_BASE_URL "")
  endif()

  set(DEP_DIR ${DEPENDENCY_UNTAR_DIR}/${DD_ARCHSTR}-${DD_NAME}/${DD_BUILD_NUMBER})

  get_cmake_property(cacheVars CACHE_VARIABLES)
  foreach(var ${cacheVars})
    if(("${${var}}" MATCHES "^${DEPENDENCY_UNTAR_DIR}/${DD_ARCHSTR}-${DD_NAME}/[0-9]+") AND
       (NOT ("${${var}}" MATCHES "^${DEP_DIR}")))
          unset(${var} CACHE)
     endif()
  endforeach()

  if(NOT DD_NO_HASH_FILE)
    set(HASH_FILENAME ${DD_NAME}-${DD_BUILD_NUMBER}-hash.txt)
    get_content_of_url(URL ${DD_BASE_URL}/hash.txt CONTENT_VAR DEP_HASH FILENAME ${HASH_FILENAME} ${DD_ALWAYS_DOWNLOAD})

    if(NOT DEP_HASH)
      message(FATAL_ERROR "Failed to get hash for dependencies. Abort abort abort...")
    endif()

    message(STATUS "Dependency hash is: ${DEP_HASH}")
    if(DD_DEPHASH_VAR)
      set(${DD_DEPHASH_VAR} ${DEP_HASH} PARENT_SCOPE)
    endif()

    set(DEP_DIRNAME "${DD_ARTIFACTNAME}-${DD_ARCHSTR}-${DD_VARIANT}-${DEP_HASH}")
    set(DEP_FILENAME ${DEP_DIRNAME}.tbz2)
    set(${DD_DEP_HASH} ${DEP_HASH} PARENT_SCOPE)
    set(DEP_URL "${DD_BASE_URL}/${DEP_FILENAME}")
    set(CONTENT_HASH_URL ${DEP_URL}.sha.txt)
  else()
    set(DEP_DIRNAME "${DD_ARTIFACTNAME}-${DD_VARIANT}")
    set(DEP_FILENAME "${DEP_DIRNAME}.tar.xz")
    set(DEP_URL "${DD_BASE_URL}/${DEP_FILENAME}")
    set(CONTENT_HASH_URL ${DEP_URL}.sha1)
  endif()

  set(${DD_DIRECTORY} ${DEP_DIR}/${DEP_DIRNAME} PARENT_SCOPE)

  get_content_of_url(URL ${CONTENT_HASH_URL} CONTENT_VAR CONTENT_HASH)

  string(SUBSTRING "${CONTENT_HASH}" 0 40 CONTENT_HASH)

  if(NOT EXISTS ${DEP_DIR}/${DEP_DIRNAME}/_FINISHED)
    message(STATUS "Clearing out old dependencies ...")
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${DEPENDENCY_UNTAR_DIR}/${DD_ARCHSTR}-${DD_NAME})
    file(MAKE_DIRECTORY ${DEP_DIR})

    if(EXISTS ${DEPENDENCY_CACHE_DIR}/${DEP_FILENAME})
      message(STATUS "Checking checksum of file ${DEP_FILENAME}")
      file(SHA1 ${DEPENDENCY_CACHE_DIR}/${DEP_FILENAME} CURRENT_SHA1)
    endif()

    if(NOT CURRENT_SHA1 STREQUAL CONTENT_HASH)
      message(STATUS "Downloading ${DEP_FILENAME}...")
      file(
        DOWNLOAD ${DEP_URL} ${DEPENDENCY_CACHE_DIR}/${DEP_FILENAME}
        SHOW_PROGRESS
        STATUS DEP_STATUS
        LOG DEP_LOG
      )

      list(GET DEP_STATUS 0 DEP_SUCCESS)

      if(NOT DEP_SUCCESS EQUAL 0)
        list(GET DEP_STATUS 1 DEP_ERROR)
        file(REMOVE ${DEPENDENCY_CACHE_DIR}/${DEP_FILENAME})
        message(FATAL_ERROR "Failed to download ${DEP_URL}: ${DEP_ERROR}\n${DEP_LOG}")
      endif()

      message(STATUS "Checking checksum of file ${DEP_FILENAME}")
      file(SHA1 ${DEPENDENCY_CACHE_DIR}/${DEP_FILENAME} CURRENT_SHA1)
      if(NOT CURRENT_SHA1 STREQUAL CONTENT_HASH)
        file(REMOVE ${DEPENDENCY_CACHE_DIR}/${DEP_FILENAME})
        message(FATAL_ERROR "Failed to verify hash of dependencies, expected: ${CONTENT_HASH} actual: ${CURRENT_SHA1}")
      endif()
    endif()

    message(STATUS "Unpacking ${DEP_FILENAME}...")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar xjf ${DEPENDENCY_CACHE_DIR}/${DEP_FILENAME}
      WORKING_DIRECTORY ${DEP_DIR}
      RESULT_VARIABLE UNPACK_RESULT
    )

    if(NOT UNPACK_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to unpack deps..")
    endif(NOT UNPACK_RESULT EQUAL 0)

    if(APPLE AND DD_DYLIB_SCRIPT_PATH)
      message(STATUS "Fixing install library names...${DEP_DIR}/${DEP_DIRNAME}")
      execute_process(
        COMMAND ${DD_DYLIB_SCRIPT_PATH} ${DEP_DIR}/${DEP_DIRNAME}
        WORKING_DIRECTORY ${DEP_DIR}
        RESULT_VARIABLE DYLIB_RESULT
      )
      if(NOT DYLIB_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to run ${DD_DYLIB_SCRIPT_PATH}")
      endif()
      message(STATUS "Done")
    endif()

    if(EXISTS ${DEP_DIR}/${DEP_DIRNAME}/etc)
      message(STATUS "Removing etc in dependency bundle")
      file(REMOVE_RECURSE ${DEP_DIR}/${DEP_DIRNAME}/etc)
    endif(EXISTS ${DEP_DIR}/${DEP_DIRNAME}/etc)

    file(WRITE "${DEP_DIR}/${DEP_DIRNAME}/_FINISHED" "Dummy")
  else(NOT EXISTS ${DEP_DIR}/${DEP_DIRNAME}/_FINISHED)
    message(STATUS "Directory ${DEP_DIR}/${DEP_DIRNAME} already exists, remove it to redownload")
  endif(NOT EXISTS ${DEP_DIR}/${DEP_DIRNAME}/_FINISHED)
endfunction(download_deps DD_NAME)

function(download_dep_file DD_NAME)
  set(ARGS DIRECTORY BUILD_NUMBER ARTIFACTNAME TOKEN FILENAME DST_FILENAME)
  cmake_parse_arguments(DD "" "${ARGS}" "" ${ARGN})

  if(NOT DEFINED DD_ARTIFACTNAME)
    set(DD_ARTIFACTNAME ${DD_NAME})
  endif(NOT DEFINED DD_ARTIFACTNAME)

  if(NOT DEFINED DD_BUILD_NUMBER)
    set(DD_BUILD_NUMBER "latest")
  endif(NOT DEFINED DD_BUILD_NUMBER)

  if(DD_BUILD_NUMBER STREQUAL latest)
    set(DD_ALWAYS_DOWNLOAD ALWAYS)
  endif()

  if(NOT DEFINED DD_TOKEN)
    set(DD_TOKEN jellyfin-dependencies)
  endif()

  set(BASE_URL "")

  set(DEP_URL "${BASE_URL}/${DD_FILENAME}")

  if(NOT EXISTS ${DEPENDENCY_CACHE_DIR}/${DD_FILENAME})
    message(STATUS "Downloading ${DD_FILENAME}...")
    file(
      DOWNLOAD ${DEP_URL} ${DEPENDENCY_CACHE_DIR}/${DD_FILENAME}
      SHOW_PROGRESS
      STATUS DEP_STATUS
      LOG DEP_LOG
    )

    list(GET DEP_STATUS 0 DEP_SUCCESS)

    if(NOT DEP_SUCCESS EQUAL 0)
      list(GET DEP_STATUS 1 DEP_ERROR)
      file(REMOVE ${DEPENDENCY_CACHE_DIR}/${DD_FILENAME})
      message(FATAL_ERROR "Failed to download ${DEP_URL}: ${DEP_ERROR}\n${DEP_LOG}")
    endif()
  endif()

    #file(COPY ${DEPENDENCY_CACHE_DIR}/${FILENAME} ${DST_FILENAME})
endfunction()
