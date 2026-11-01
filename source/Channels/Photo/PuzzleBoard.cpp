#include "Channels/Photo/PuzzleBoard.h"

#include <algorithm>
#include <numeric>

namespace Channels
{

PuzzleBoard::PuzzleBoard(int pieceCount)
    : pieceInPlace(pieceCount)
{
    std::iota(pieceInPlace.begin(), pieceInPlace.end(), 0);
}

int PuzzleBoard::PieceCount() const
{
    return static_cast<int>(pieceInPlace.size());
}

int PuzzleBoard::PieceAt(int place) const
{
    return pieceInPlace[place];
}

// A shuffle that leaves a piece in its own place is mixed again; with 6 pieces or more this ends after a few tries.
void PuzzleBoard::Mix(std::mt19937& random)
{
    auto hasPieceInItsPlace = [this] {
        for (int place = 0; place < PieceCount(); ++place)
        {
            if (pieceInPlace[place] == place)
                return true;
        }
        return false;
    };

    do
    {
        std::shuffle(pieceInPlace.begin(), pieceInPlace.end(), random);
    } while (PieceCount() > 1 && hasPieceInItsPlace());
}

void PuzzleBoard::Swap(int firstPlace, int secondPlace)
{
    std::swap(pieceInPlace[firstPlace], pieceInPlace[secondPlace]);
}

bool PuzzleBoard::IsSolved() const
{
    for (int place = 0; place < PieceCount(); ++place)
    {
        if (pieceInPlace[place] != place)
            return false;
    }
    return true;
}

}
