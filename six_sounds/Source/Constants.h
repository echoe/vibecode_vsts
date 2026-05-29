// Inside Source/Constants.h
#pragma once

namespace ProjectConfig
{
    // Operator count ...
    static constexpr int numOperators = 6;
    // Number of additional mod lanes on top of the matrix ...
    constexpr int numModLanes = 4;
    // For organizing the matrix X and Y positions, and spacing.
    static constexpr int matrixYPos = 40;
    static constexpr int matrixXPos = 80;
    static constexpr int matrixSpacing = 90;
}
