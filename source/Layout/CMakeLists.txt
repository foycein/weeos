add_library(Layout STATIC
    AnimatedValues.cpp
    AnimationPlayer.cpp
    CurveEvaluation.cpp
    DefaultTevStages.cpp
    HitTest.cpp
    LayoutArchive.cpp
    LayoutDrawer.cpp
    LiveLayout.cpp
    LiveMaterial.cpp
    MaterialBuilder.cpp
    PanePlacement.cpp
    PaneRectangle.cpp
    RectangleDrawing.cpp
    TevStageConversion.cpp
    TextBoxDrawing.cpp
    TextFonts.cpp
    TransformMatrix.cpp
    WindowDrawing.cpp
)

target_link_libraries(Layout PUBLIC Formats Text)
