add_library(Layout STATIC
    CurveEvaluation.cpp
    TransformMatrix.cpp
)

target_link_libraries(Layout PUBLIC Formats)
