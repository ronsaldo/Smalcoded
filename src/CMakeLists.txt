set(SmalcodedGameLogic_SOURCES
    Box2.hpp
    ControllerState.hpp
    Float.hpp
    GameInterface.hpp
    GameLogic.cpp
    GameLogic.hpp
    Image.cpp
    Image.hpp
    MemoryZone.hpp
    Rectangle.hpp
    Renderer.cpp
    Renderer.hpp
    Tile.cpp
    Tile.hpp
    Vector2.hpp
)

set(Smalcoded_SOURCES
    Main.cpp
)

if(LIVE_CODING_SUPPORT)
    add_definitions(-DUSE_LIVE_CODING)
    add_library(SmalcodedGameLogic MODULE ${SmalcodedGameLogic_SOURCES})
    target_link_libraries(SmalcodedGameLogic ${Smalcoded_DEP_LIBS})
else()
    set(Smalcoded_SOURCES ${SmalcodedGameLogic_SOURCES} ${Smalcoded_SOURCES} )
endif()

add_executable(Smalcoded ${Smalcoded_SOURCES})
set_target_properties(Smalcoded PROPERTIES LINK_FLAGS "${ASSET_FLAGS}")
target_link_libraries(Smalcoded ${Smalcoded_DEP_LIBS})
