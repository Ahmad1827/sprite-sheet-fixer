file(GLOB_RECURSE UI_HEADERS "Include/*.h")
file(GLOB_RECURSE UI_SOURCES "Source/*.cpp")

add_executable(SpriteSheetStudioApp ${UI_HEADERS} ${UI_SOURCES})
target_include_directories(SpriteSheetStudioApp PRIVATE Include)
target_link_libraries(SpriteSheetStudioApp PRIVATE StudioCore)
