#pragma once

#include <random>
#include <vector>

namespace Channels
{

// The pieces of a puzzle and the places they lie in. Piece n belongs in place n; the puzzle is solved when every piece is in its place.
class PuzzleBoard
{
public:
    explicit PuzzleBoard(int pieceCount);

    int PieceCount() const;
    int PieceAt(int place) const;

    // Mixes the pieces so that none is in its own place.
    void Mix(std::mt19937& random);

    void Swap(int firstPlace, int secondPlace);
    bool IsSolved() const;

private:
    std::vector<int> pieceInPlace;
};

}
