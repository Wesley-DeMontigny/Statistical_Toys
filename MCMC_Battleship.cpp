/*
This was the first MCMC I programmed in C++. It is relatively simple because the likelihood function is binary (consistent/inconsistent).
The way I have set up the proposal makes the hastings ratio 1. As it is currently written, this allows you to make statistically informed 
opinions about the game Battlefield. The posterior probability indicates the probability that you will get a hit if you should at that cell, 
given that you have missed several shots and the location of those shots. It currently does not have hits or sunken ships implemented, 
but this was mostly a quick and fun exercise. It should also be noted that because parameter space is mostly flat, you aren't really
exploring parameter space like you would in a typical MCMC. I have ran this code several times and it appears to give consistent results,
but it is likely that parameter space isn't being fully explored - if we are generous, we are only sampling ~1 million unique board orientations;
I'm sure there are more possibilities than that. I can almost certainly say that the chain is NOT in stationary, but that is sort of just the nature of
the problem here.
*/
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <tuple>
#include <set>
#include <iterator>
#include <iomanip>

//Global Game Vars
const int boardWidth = 10;
const int boardHeight = 10;

//down = 1
//up = 2
//right = 3
//left = 4

struct handle{
    std::tuple<int, int> pos;
    int orientation;
};

class ship{
    public:
        int getShipSize(){return shipSize;}
        handle getHandle(){return myHandle;}
        std::vector<std::tuple<int, int>> getSpots(){return spots;}
        std::vector<std::tuple<int, int>> getSurroundingSpots(){return surroundingSpots;}
        void setActive(bool state){active = state;}
        bool isActive(){return active;}

        void changePosition(handle newHandle){
            std::vector<std::tuple<int, int>> newSpots;
            std::vector<std::tuple<int, int>> newSurroundingSpots;

            for (int k = 0; k < shipSize; k++) {
                int x = std::get<0>(newHandle.pos), y = std::get<1>(newHandle.pos);
                if (newHandle.orientation == 1 && x + k < boardHeight) x = x + k; // Down
                else if (newHandle.orientation == 2 && x - k >= 0) x = x - k; // Up
                else if (newHandle.orientation == 3 && y + k < boardWidth) y = y + k; // Right
                else if (newHandle.orientation == 4 && y - k >= 0) y = y - k; // Left

                newSpots.push_back(std::tuple<int, int>(x, y));

                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < boardHeight && ny >= 0 && ny < boardWidth)
                            newSurroundingSpots.push_back(std::tuple<int, int>(nx, ny));
                    }
                }
            }
            spots = newSpots;
            surroundingSpots = newSurroundingSpots;
            active = true;
            myHandle = newHandle;
        }

        ship(int size) : active(false) {
            shipSize = size;
        }
    private:
        int shipSize;
        handle myHandle;
        bool active;
        std::vector<std::tuple<int, int>> spots;
        std::vector<std::tuple<int, int>> surroundingSpots;
};

std::vector<std::vector<bool>> getBoardMatrix(const std::vector<ship>& ships) {
    std::vector<std::vector<bool>> result(boardHeight, std::vector<bool>(boardWidth, true));

    for (ship s : ships) {
        if(s.isActive()){
            for(std::tuple<int, int> pos : s.getSurroundingSpots()){
                result[std::get<0>(pos)][std::get<1>(pos)] = false;
            }
        }
    }

    return result;
}


std::vector<std::vector<bool>> getPositionMatrix(const std::vector<ship>& ships) {
    std::vector<std::vector<bool>> result(boardHeight, std::vector<bool>(boardWidth, false));

    for (ship s : ships) {
        if(s.isActive()){
            for(std::tuple<int, int> pos : s.getSpots()){
                result[std::get<0>(pos)][std::get<1>(pos)] = true;
            }
        }
    }

    return result;
}

std::vector<handle> getValidHandles(ship myShip, const std::vector<ship>& ships){
    //Get available handle options for a ship, given a board.
    std::vector<std::vector<bool>> availableSpots = getBoardMatrix(ships);
    std::vector<handle> handles;
    int shipSize = myShip.getShipSize();

    // Go through the availability matrix and detect 
    for(int i = 0; i < boardHeight; i++){
        for(int j = 0; j < boardWidth; j++){
            int handleDirectionCounts[4] = {0,0,0,0};
            //Iterate through possible positions
            for(int k = 0; k < shipSize; k++)
            {
                if(i+k < boardHeight && availableSpots[i+k][j]) handleDirectionCounts[0]++;
                if(i-k >= 0 && availableSpots[i-k][j]) handleDirectionCounts[1]++;
                if(j+k < boardWidth && availableSpots[i][j+k]) handleDirectionCounts[2]++;
                if(j-k >= 0 && availableSpots[i][j-k]) handleDirectionCounts[3]++;
            }
            //Add good handles
            for(int x = 0; x < 4; x++){
                if(handleDirectionCounts[x] == shipSize){
                    handles.push_back(handle{{i, j}, x+1});
                }
            }
        }
    }

    return handles;
}

int getProposalIndex(const std::vector<ship>& ships, std::mt19937 rng){
    std::uniform_int_distribution<> distr(0, ships.size()-1);
    return distr(rng);
}

double getBoardLikelihood(const std::vector<ship>& ships, std::vector<std::tuple<int,int>> misses){
    std::vector<std::vector<bool>> shipPoistions = getPositionMatrix(ships);
    for(std::tuple<int,int> miss : misses){
        if(shipPoistions[std::get<0>(miss)][std::get<1>(miss)]) return 0;
    }
    /*
    for(std::tuple<int,int> hit : hits){
        if(!shipPoistions[std::get<0>(hit)][std::get<1>(hit)]) return 0;
    }*/

    return 1;
}

void printBoard(std::vector<ship> ships){
    std::vector<std::vector<bool>> openPositions = getPositionMatrix(ships);
    for(int i = 0; i < boardWidth+2; i++)
        std::cout << "#";
    std::cout << std::endl;

    for(int i = 0; i < boardHeight; i++){
        std::cout << "#";
        for(int j = 0; j < boardWidth; j++){
            if(openPositions[i][j]){
                std::cout << "X";
            }else{
                std::cout << ".";
            }
        }
        std::cout << "#" << std::endl;
    }

    for(int i = 0; i < boardWidth+2; i++)
        std::cout << "#";
    std::cout << std::endl;
}

void initBoard(std::vector<ship>& ships, std::mt19937 rng){
    //Generate a starting board

    std::shuffle(ships.begin(), ships.end(), rng);
    std::cout << "--Initializing Board--" << std::endl;
    std::cout << "Available Handles:" << std::endl;

    for(ship& s : ships){
        std::vector<handle> handles = getValidHandles(s, ships);
        std::cout << "Ship "<< s.getShipSize() << " - " << handles.size() << std::endl;

        std::uniform_int_distribution<> distr(0, handles.size()-1);
        s.changePosition(handles[distr(rng)]);
        printBoard(ships);
    }

}

int main(int argc, const char* argv[]){
    std::vector<ship> ships;
    std::vector<int> shipSizes = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};
    for(int size : shipSizes){
        ship newShip(size);
        ships.push_back(newShip);
    }

    //Observations!
    std::vector<std::tuple<int,int>> misses = {
        std::tuple<int,int>(5,5), std::tuple<int,int>(2,3), std::tuple<int, int>(8,7), std::tuple<int, int>(3,8),
        std::tuple<int,int>(8,1)
    };
    //std::vector<std::tuple<int,int>> hits = {std::tuple<int,int>(0,0), std::tuple<int,int>(0,1)};

    //Posterior Board Distribution
    std::vector<std::vector<int>> postBoard(boardHeight, std::vector<int>(boardWidth, 0));

    unsigned seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    seed ^= std::random_device{}();
    std::mt19937 rng(seed);

    initBoard(ships, rng);
    std::cout << "\n\n--Initial Board--" << std::endl;
    printBoard(ships);

    std::cout << "\n\n--Starting MCMC--" << std::endl;
    /*
    I'm pretty sure my Hastings ratio is 1. This is because my proposal mechanism is:
    1. Pick a random ship and remove it from the Board
    2. Pick a random valid handle for that ship.
    Between state theta and state theta prime, there is an equal probability of picking a given ship.
    After you have removed it from the board, the board between these two states are equivalent.

    The likelihood function is super easy - if the positioning is consistent with the observations it gets a 1! Otherwise it is inconsistent and gets a 0.
    The accept/reject is really easy too - since the hastings ratio and prior ratio are 1 AND the likelihood ratio will either be 0 or 1, it will accept any
    state that is consistent with the observations.

    NOTE THAT THIS PROPOSAL MECHANISM WILL NOT WORK FOR HITS. Moving a boat from the board that is under a hit will always result in
    it being inconsistent with the observations. To implement hits, we need to sometimes make proposals according to random changes and
    sometimes try to swap boats around. Additionally, it would also be worth 
    */
    int chainLength = 5000000;
    int burnIn = 1000;
    int sampleRate = 1;
    for(int i = 0; i < chainLength; i++){
        std::vector<ship> proposedShipArrangement(ships);

        int proposedShip = getProposalIndex(proposedShipArrangement, rng); //Select a ship
        ship& adjustedShip = proposedShipArrangement.at(proposedShip);

        adjustedShip.setActive(false);//Remove the ship from the board
        std::vector<handle> handles = getValidHandles(adjustedShip, proposedShipArrangement); //Get potential handles for this ship
        std::uniform_int_distribution<> distr(0, handles.size() - 1);
        adjustedShip.changePosition(handles[distr(rng)]);//Set the handle

        double proposalLikelihood = getBoardLikelihood(proposedShipArrangement, misses);

        if(proposalLikelihood == 1.0){
            ships = proposedShipArrangement;
            //std::cout << "Accepted Proposal: " << proposalLikelihood << std::endl;
        }
        /*else{
            std::cout << "Rejected Proposal: " << proposalLikelihood << std::endl;
        }*/
        

        std::cout << "\rFinished " << i+1 << "/" << chainLength << std::flush;

        //Sample past the burn in
        if(i >= burnIn && i % sampleRate == 0){
            std::vector<std::vector<bool>> shipPoistions = getPositionMatrix(ships);
            for(int j = 0; j < shipPoistions.size(); j++){
                for(int k = 0; k < shipPoistions[0].size(); k++){
                    if(shipPoistions[j][k]) postBoard[j][k]++;
                }
            }
        }
    } 

    std::cout << "\n\n\n--Posterior Board--" << std::endl;

    //Print out posterior samples
    for(int i = 0; i < postBoard.size(); i++){
        for(int j = 0; j < postBoard[0].size(); j++){
            std::cout << std::setprecision(3) << (double)postBoard[i][j]/(chainLength-burnIn) << ", ";
        }
        std::cout << std::endl;
    }
}
