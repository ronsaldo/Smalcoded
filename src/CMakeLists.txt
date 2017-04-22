set(SmallEcoDestroyedGameLogic_SOURCES
    ControllerState.hpp
    Float.hpp
    GameInterface.hpp
    GameLogic.cpp
    GameLogic.hpp
    MemoryZone.hpp
    Renderer.cpp
    Renderer.hpp
    Vector2.hpp
)

set(SmallEcoDestroyed_SOURCES
    Main.cpp
)

if(LIVE_CODING_SUPPORT)
    add_definitions(-DUSE_LIVE_CODING)
    add_library(SmallEcoDestroyedGameLogic MODULE ${SmallEcoDestroyedGameLogic_SOURCES})
    target_link_libraries(SmallEcoDestroyedGameLogic ${SmallEcoDestroyed_DEP_LIBS})

else()
    set(SmallEcoDestroyed_SOURCES ${SmallEcoDestroyed_SOURCES} ${SmallEcoDestroyedGameLogic_SOURCES})
endif()

add_executable(SmallEcoDestroyed ${SmallEcoDestroyed_SOURCES})
set_target_properties(SmallEcoDestroyed PROPERTIES LINK_FLAGS "${ASSET_FLAGS}")
target_link_libraries(SmallEcoDestroyed ${SmallEcoDestroyed_DEP_LIBS})
