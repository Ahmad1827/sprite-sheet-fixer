file(GLOB_RECURSE UI_HEADERS "Include/*.h")
file(GLOB_RECURSE UI_SOURCES "Source/*.cpp")

add_executable(SpriteSheetStudioApp ${UI_HEADERS} ${UI_SOURCES})

target_include_directories(SpriteSheetStudioApp PRIVATE Include)

# Explicitly link StudioCore and all SFML components
target_link_libraries(SpriteSheetStudioApp PRIVATE 
    StudioCore 
    sfml-graphics 
    sfml-window 
    sfml-system
)