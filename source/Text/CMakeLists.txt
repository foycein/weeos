add_library(Text STATIC
    GlyphLookup.cpp
    GlyphSheetTextures.cpp
    LaidOutText.cpp
    MessageCatalog.cpp
    TextDrawer.cpp
    TextFormat.cpp
    TextLayouter.cpp
    TextStyle.cpp
)
target_link_libraries(Text PUBLIC Formats)
