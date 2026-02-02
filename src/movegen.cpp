#include "movegen.hpp"

// Pawn
std::vector<int> MoveGen::genPawn(Board & board, int location, bool isBlack)
{
    std::vector<int> generated; 
	
    uint64_t allOccupied = board.occupancy[dualOccupancy];
    int rankFrom = location / 8;
    int fileFrom = location % 8;
    
    if(!isBlack)
    {

        bool isFirst = (location > 7 && location < 16); 

		int oneStep = location + 8;

        if (oneStep >= 0 && oneStep <= 63 && !(allOccupied & (1ull << oneStep)))
        {
			generated.push_back(oneStep);

            if (isFirst)
            {
                int twoStep = location + 16;
                if (twoStep >= 0 && twoStep <= 63 && !(allOccupied & (1ull << twoStep)))
                {
                    generated.push_back(twoStep);
                }
            }
        }

		// Capture in white turn
        static const int pawnCapture[2] = { -1,1 };
        for (int i = 0; i < 2; i++)
        {
            int rankDest = rankFrom + 1;
            int fileDest = fileFrom + pawnCapture[i];
            int dest = rankDest * 8 + fileDest;

            if (board.occupancy[blackOccupancy] & (1ull << dest))
            {
				generated.push_back(dest);
            }
        }
		return generated;
    }
    else
    {
        bool isFirst = (location > 47 && location < 56);
		int oneStep = location - 8;
        if (oneStep >= 0 && oneStep <= 63 && !(allOccupied & (1ull << oneStep)))
        {
            generated.push_back(oneStep);

            if (isFirst)
            {
                int twoStep = location - 16;
                if (twoStep >= 0 && twoStep <= 63 && !(allOccupied & (1ull << twoStep)))
                {
                    generated.push_back(twoStep);
                }
            }
        }
        // Capture in black turn
        static const int pawnCapture[2] = { -1,1 };
        for (int i = 0; i < 2; i++)
        {
            int rankDest = rankFrom - 1;
            int fileDest = fileFrom + pawnCapture[i];
            int dest = rankDest * 8 + fileDest;

            if (board.occupancy[whiteOccupancy] & (1ull << dest))
            {
                generated.push_back(dest);
            }
        }
		return generated;
    }
  }
