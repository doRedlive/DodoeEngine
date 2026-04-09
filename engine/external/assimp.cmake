set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable Assimp tests" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Disable Assimp tools" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "Disable Assimp samples" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable Assimp install" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "Disable Assimp docs" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "Disable debug postfix" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "Disable Assimp export" FORCE)

add_subdirectory(assimp)
