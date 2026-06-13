set(external_folder "ThirdParty")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if (NOT TARGET spdlog)
    option(SPDLOG_BUILD_EXAMPLE "" OFF)
    option(SPDLOG_INSTALL "" OFF)
    add_subdirectory(spdlog)
    set_target_properties(spdlog PROPERTIES FOLDER ${external_folder}/spdlog)
endif ()

if (NOT TARGET glad)
    include(glad.cmake)
    set_target_properties(glad PROPERTIES FOLDER ${external_folder}/glad)
endif ()

if (NOT TARGET glfw)
    option(GLFW_BUILD_EXAMPLES "" OFF)
    option(GLFW_BUILD_TESTS "" OFF)
    option(GLFW_BUILD_DOCS "" OFF)
    option(GLFW_INSTALL "" OFF)
    add_subdirectory(glfw)
    set_target_properties(glfw PROPERTIES FOLDER ${external_folder}/glfw)
endif ()

if (NOT TARGET imgui)
    include(imgui.cmake)
    set_target_properties(imgui PROPERTIES FOLDER ${external_folder}/imgui)
endif ()

if (NOT TARGET EnTT::EnTT)
    option(ENTT_BUILD_TEST "" OFF)
    option(ENTT_BUILD_DOCS "" OFF)
    option(ENTT_INSTALL "" OFF)
    add_subdirectory(entt)

    get_target_property(ENTT_ACTUAL_TARGET EnTT::EnTT ALIASED_TARGET)
    if (ENTT_ACTUAL_TARGET)
        set_target_properties(${ENTT_ACTUAL_TARGET} PROPERTIES FOLDER ${external_folder}/entt)
    endif ()
endif ()

if (NOT TARGET stb_image)
    include(stb_image.cmake)
    set_target_properties(stb_image PROPERTIES FOLDER ${external_folder}/stb_image)
endif ()

if (NOT TARGET glm)
    option(GLM_BUILD_TEST "" OFF)
    option(GLM_BUILD_DOC "" OFF)
    option(GLM_BUILD_INSTALL "" OFF)
    add_subdirectory(glm)
    set_target_properties(glm PROPERTIES FOLDER ${external_folder}/glm)
endif ()

if (NOT TARGET box2d)
    option(BOX2D_BUILD_TEST "" OFF)
    option(BOX2D_BUILD_DOC "" OFF)
    option(BOX2D_BUILD_INSTALL "" OFF)
    add_subdirectory(box2d)
    set_target_properties(box2d PROPERTIES FOLDER ${external_folder}/box2d)
endif()

if (NOT TARGET json)
    option(JSON_BUILD_TEST "" OFF)
    option(JSON_BUILD_DOC "" OFF)
    option(JSON_BUILD_INSTALL "" OFF)
    add_subdirectory(json)

    if (TARGET nlohmann_json::nlohmann_json)
        get_target_property(JSON_ACTUAL_TARGET nlohmann_json::nlohmann_json ALIASED_TARGET)
        if (JSON_ACTUAL_TARGET)
            set_target_properties(${JSON_ACTUAL_TARGET} PROPERTIES FOLDER ${external_folder}/json)
        endif ()
    elseif (TARGET nlohmann_json)
        set_target_properties(nlohmann_json PROPERTIES FOLDER ${external_folder}/json)
    endif ()
endif()

# Mono's coreclr.dll conflicts with .NET 10 runtime.
if(NOT TARGET mono::mono)
    include(mono.cmake)
endif()

if(NOT TARGET assimp)
    include(assimp.cmake)
    set_target_properties(assimp PROPERTIES FOLDER ${external_folder}/assimp)
endif()

if(NOT TARGET ads::qtadvanceddocking-qt6)
    include(qt-ads.cmake)
endif()

if(NOT TARGET nvrhi)
    include(nvrhi.cmake)
    set_target_properties(nvrhi PROPERTIES FLODER ${external_folder}/nvrhi)
endif()

if(NOT TARGET pybind11::pybind11)
    set(Python_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/python")
    if(NOT Python_FOUND AND NOT Python3_FOUND)
        find_package(Python 3.8 REQUIRED COMPONENTS Interpreter Development)
    endif()
    add_subdirectory(pybind11)
    set_target_properties(pybind11_headers PROPERTIES FOLDER ${external_folder}/pybind11)

    # Copy python3*.dll to output directory for runtime
    file(GLOB PYTHON_DLL "${Python_ROOT_DIR}/python3*.dll")
    add_custom_target(DodoePythonRuntimeFiles ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PYTHON_DLL}
            "${BINARY_ROOT_DIR}/"
        COMMENT "Copying python3*.dll to output directory"
    )
endif()

