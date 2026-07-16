find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

execute_process(
    COMMAND ${DOTNET_EXECUTABLE} --list-runtimes
    OUTPUT_VARIABLE DOTNET_RUNTIME_LIST
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(NETHOST_RUNTIME_VERSION "")
string(REGEX MATCHALL "Microsoft\\.NETCore\\.App [0-9]+\\.[0-9]+\\.[0-9]+"
    DOTNET_CORECLR_VERSIONS "${DOTNET_RUNTIME_LIST}")
foreach(_ver_line IN LISTS DOTNET_CORECLR_VERSIONS)
    string(REGEX REPLACE "Microsoft\\.NETCore\\.App " "" _ver "${_ver_line}")
    if(_ver VERSION_GREATER NETHOST_RUNTIME_VERSION)
        set(NETHOST_RUNTIME_VERSION "${_ver}")
    endif()
endforeach()

if(NOT NETHOST_RUNTIME_VERSION)
    message(FATAL_ERROR "nethost.cmake: Could not find Microsoft.NETCore.App runtime. Install .NET 8 SDK.")
endif()

message(STATUS "nethost: detected Microsoft.NETCore.App ${NETHOST_RUNTIME_VERSION}")

if(WIN32)
    set(NETHOST_SEARCH_PATHS
        "$ENV{ProgramFiles}/dotnet/packs/Microsoft.NETCore.App.Host.win-x64"
        "${DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.win-x64"
        "C:/Program Files/dotnet/packs/Microsoft.NETCore.App.Host.win-x64"
    )

    find_path(NETHOST_INCLUDE_DIR nethost.h
        PATHS ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES
            "${NETHOST_RUNTIME_VERSION}/runtimes/win-x64/native"
            "*/runtimes/win-x64/native"
    )

    find_library(NETHOST_LIBRARY nethost
        PATHS ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES
            "${NETHOST_RUNTIME_VERSION}/runtimes/win-x64/native"
            "*/runtimes/win-x64/native"
    )

    find_file(NETHOST_DLL nethost.dll
        PATHS ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES
            "${NETHOST_RUNTIME_VERSION}/runtimes/win-x64/native"
            "*/runtimes/win-x64/native"
    )
elseif(APPLE)
    set(NETHOST_SEARCH_PATHS
        "/usr/local/share/dotnet/packs/Microsoft.NETCore.App.Host.osx-x64"
        "${DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.osx-x64"
    )
    find_path(NETHOST_INCLUDE_DIR nethost.h
        PATHS ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES "*/runtimes/osx-x64/native"
    )
    find_library(NETHOST_LIBRARY nethost
        PATHS ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES "*/runtimes/osx-x64/native"
    )
else()
    set(NETHOST_SEARCH_PATHS
        "/usr/share/dotnet/packs/Microsoft.NETCore.App.Host.linux-x64"
        "${DOTNET_ROOT}/packs/Microsoft.NETCore.App.Host.linux-x64"
    )
    find_path(NETHOST_INCLUDE_DIR nethost.h
        PATHS ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES "*/runtimes/linux-x64/native"
    )
    find_library(NETHOST_LIBRARY nethost
        PATHS ${NETHOST_SEARCH_PATHS}
        PATH_SUFFIXES "*/runtimes/linux-x64/native"
    )
endif()

if(NOT NETHOST_INCLUDE_DIR)
    message(FATAL_ERROR "nethost.cmake: nethost.h not found. Make sure .NET SDK is installed.")
endif()
if(NOT NETHOST_LIBRARY)
    message(FATAL_ERROR "nethost.cmake: nethost.lib not found. Make sure .NET SDK is installed.")
endif()

message(STATUS "nethost include: ${NETHOST_INCLUDE_DIR}")
message(STATUS "nethost library: ${NETHOST_LIBRARY}")
if(NETHOST_DLL)
    message(STATUS "nethost dll:     ${NETHOST_DLL}")
endif()

if(NOT TARGET dodoe_nethost)
    add_library(dodoe_nethost INTERFACE)
    target_include_directories(dodoe_nethost INTERFACE ${NETHOST_INCLUDE_DIR})
    target_link_libraries(dodoe_nethost INTERFACE ${NETHOST_LIBRARY})
endif()
set_target_properties(dodoe_nethost PROPERTIES FOLDER "ThirdParty/nethost")

if(NOT TARGET nethost::nethost)
    add_library(nethost::nethost ALIAS dodoe_nethost)
endif()
