set(SdlTemplate_SOURCES
    Main.cpp
)

add_executable(SdlTemplate ${SdlTemplate_SOURCES})
set_target_properties(SdlTemplate PROPERTIES LINK_FLAGS "${ASSET_FLAGS}")
target_link_libraries(SdlTemplate ${SdlTemplate_DEP_LIBS})
