add_library(Effects STATIC
    AnimationCurve.cpp
    Effect.cpp
    EffectRandom.cpp
    EffectResources.cpp
    Emitter.cpp
    EmitterForms.cpp
    ParticleDrawing.cpp
    VectorMath.cpp
)

target_link_libraries(Effects PUBLIC Formats Graphics SharpArt)
