/*
This was the first MCMC I programmed in C++. It is relatively simple because the likelihood function is binary (consistent/inconsistent).
The way I have set up the proposal makes the hastings ratio 1. As it is currently written, this allows you to make statistically informed 
opinions about the game Battlefield. The posterior probability indicates the probability that you will get a hit if you should at that cell, 
given that you have missed several shots and the location of those shots. It currently does not have hits or sunken ships implemented, 
but this was mostly a quick and fun exercise. It should also be noted that because parameter space is mostly flat, you aren't really
exploring parameter space like you would in a typical MCMC. I have ran this code several times and it appears to give consistent results,
but it is likely that parameter space isn't being fully explored - if we are generous, we are only sampling ~1 million unique board orientations;
I'm sure there are more possibilities than that. Also, this could definitely be faster if I implemented board and ship classes rather than
swapping between handle and ship positions.
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

std::vector<std::vector<bool>> getShipPositions(const std::vector<std::vector<std::tuple<int, unsigned char>>>& board) {
    int rows = board.size();
    int cols = board[0].size();

    // Initialize availableSpots
    std::vector<std::vector<bool>> shipSpots(rows, std::vector<bool>(cols, false));

    // Mark ship positions
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int ship = std::get<0>(board[i][j]);
            if (ship > 0) {
                // Direction of the ship
                unsigned char direction = std::get<1>(board[i][j]);
                for (int k = 0; k < ship; k++) {
                    int x = i, y = j;
                    if (direction == 1 && i + k < rows) x = i + k; // Down
                    else if (direction == 2 && i - k >= 0) x = i - k; // Up
                    else if (direction == 3 && j + k < cols) y = j + k; // Right
                    else if (direction == 4 && j - k >= 0) y = j - k; // Left

                    shipSpots[x][y] = true;
                }
            }
        }
    }

    return shipSpots;
}

std::vector<std::vector<bool>> getOpenPositions(const std::vector<std::vector<std::tuple<int, unsigned char>>>& board) {
    int rows = board.size();
    int cols = board[0].size();

    // Initialize availableSpots
    std::vector<std::vector<bool>> availableSpots(rows, std::vector<bool>(cols, true));

    // Mark ship positions
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int ship = std::get<0>(board[i][j]);
            if (ship > 0) {
                // Direction of the ship
                unsigned char direction = std::get<1>(board[i][j]);
                for (int k = 0; k < ship; k++) {
                    int x = i, y = j;
                    if (direction == 1 && i + k < rows) x = i + k; // Down
                    else if (direction == 2 && i - k >= 0) x = i - k; // Up
                    else if (direction == 3 && j + k < cols) y = j + k; // Right
                    else if (direction == 4 && j - k >= 0) y = j - k; // Left

                    //Set surrounding values to also be false
                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dy = -1; dy <= 1; dy++) {
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < rows && ny >= 0 && ny < cols)
                                availableSpots[nx][ny] = false;
                        }
                    }
                }
            }
        }
    }

    return availableSpots;
}

std::set<std::tuple<int, int, unsigned char>> getValidHandles(int ship, const std::vector<std::vector<std::tuple<int, unsigned char>>>& board){
    //Get available handle options for a ship, given a board.
    std::vector<std::vector<bool>> availableSpots = getOpenPositions(board);
    int rows = availableSpots.size();
    int cols = availableSpots[0].size();
    std::set<std::tuple<int, int, unsigned char>> handles;

    // Go through the availability matrix and detect 
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < cols; j++){
            int handleDirectionCounts[4] = {0,0,0,0};
            //Iterate through possible positions
            for(int k = 0; k < ship; k++)
            {
                if(i+k < rows && availableSpots[i+k][j]) handleDirectionCounts[0]++;
                if(i-k >= 0 && availableSpots[i-k][j]) handleDirectionCounts[1]++;
                if(j+k < cols && availableSpots[i][j+k]) handleDirectionCounts[2]++;
                if(j-k >= 0 && availableSpots[i][j-k]) handleDirectionCounts[3]++;
            }
            //Add good handles
            for(int x = 0; x < 4; x++){
                if(handleDirectionCounts[x] == ship)
                    handles.insert(std::tuple<int, int, unsigned char>(i, j, x+1));
            }
        }
    }

    return handles;
}

void initBoard(const std::vector<int> ships, const int width, const int height, std::mt19937 rng, std::vector<std::vector<std::tuple<int, unsigned char>>>& board){
    //Generate a starting board
    for(int i = 0; i < height; i++){
        std::vector<std::tuple<int, unsigned char>> row;
        for(int j = 0; j < width; j++){
            row.push_back(std::tuple<int, unsigned char>(0,0));
        }
        board.push_back(row);
    }

    std::vector<int> myShips(ships);
    std::shuffle(myShips.begin(), myShips.end(), rng);

    for(int i = 0; i < myShips.size(); i++){
        std::set<std::tuple<int, int, unsigned char>> handles = getValidHandles(myShips[i], board);

        std::uniform_int_distribution<> distr(0, handles.size() - 1);
        auto it = handles.begin();
        std::advance(it, distr(rng));

        board[std::get<0>(*it)][std::get<1>(*it)] = std::tuple<int, unsigned char>(myShips[i], std::get<2>(*it));
    }

}

std::tuple<int, int> getProposalLoc(const std::vector<std::vector<std::tuple<int, unsigned char>>>& board, std::mt19937 rng){
    std::vector<std::tuple<int, int>> shipIndices;
        for (int i = 0; i < board.size(); i++) {
            for (int j = 0; j < board[0].size(); j++) {
                if (std::get<0>(board[i][j]) > 0) {
                    shipIndices.push_back(std::tuple<int, int>(i,j));
                }
        }
    }

    std::shuffle(shipIndices.begin(), shipIndices.end(), rng);
    return shipIndices[0];
}

double getBoardLikelihood(std::vector<std::vector<std::tuple<int, unsigned char>>>& board, std::vector<std::tuple<int,int>> misses){
    std::vector<std::vector<bool>> shipPoistions = getShipPositions(board);
    for(std::tuple<int,int> miss : misses){
        if(shipPoistions[std::get<0>(miss)][std::get<1>(miss)]) return 0;
    }
    /*
    for(std::tuple<int,int> hit : hits){
        if(!shipPoistions[std::get<0>(hit)][std::get<1>(hit)]) return 0;
    }*/

    return 1;
}

void printBoard(std::vector<std::vector<std::tuple<int, unsigned char>>>& board){
    std::vector<std::vector<bool>> openPositions = getShipPositions(board);
    for(int i = 0; i < openPositions.size(); i++){
        std::cout << "|";
        for(int j = 0; j < openPositions[0].size(); j++){
            if(openPositions[i][j]){
                std::cout << "X";
            }else{
                std::cout << " ";
            }
        }
        std::cout << "|" << std::endl;
    }
}

int main(int argc, const char* argv[]){
    std::vector<int> ships = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};
    int boardWidth = 10;
    int boardHeight = 10;

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
    /*
    I need my board to easily reflect both the size of the ships, the positions they are taking, and the open positions. 
    I think the best way to do this is to have the main board consist of handle locations and directions (1-4) and the
    open locations be calculated from this matrix.
    This is how the directions of the handles are coded:
        1 - up
        2 - down
        3 - right
        4 - left
    */
    std::vector<std::vector<std::tuple<int, unsigned char>>> board;

    initBoard(ships, boardWidth, boardHeight, rng, board);

    std::cout << "--Initial Board--" << std::endl;
    printBoard(board);

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
    int chainLength = 1000000;
    int burnIn = 1000;
    int sampleRate = 1;
    for(int i = 0; i < chainLength; i++){
        std::vector<std::vector<std::tuple<int, unsigned char>>> proposal(board);

        std::tuple<int, int> proposalLoc = getProposalLoc(proposal, rng); //Select a ship
        int adjustedShip = std::get<0>(proposal[std::get<0>(proposalLoc)][std::get<1>(proposalLoc)]);

        proposal[std::get<0>(proposalLoc)][std::get<1>(proposalLoc)] = std::tuple<int, unsigned char>(0,0);//Remove the ship from the board
        std::set<std::tuple<int, int, unsigned char>> handles = getValidHandles(adjustedShip, proposal); //Get potential handles for this ship
        std::uniform_int_distribution<> distr(0, handles.size() - 1);
        auto it = handles.begin();
        std::advance(it, distr(rng));
        proposal[std::get<0>(*it)][std::get<1>(*it)] = std::tuple<int, unsigned char>(adjustedShip, std::get<2>(*it));//Pick a random handle as the proposal

        double proposalLikelihood = getBoardLikelihood(proposal, misses);

        if(proposalLikelihood == 1.0){
            board = proposal;
            //std::cout << "Accepted Proposal: " << proposalLikelihood << std::endl;
        }
        /*else{
            std::cout << "Rejected Proposal: " << proposalLikelihood << std::endl;
        }*/
        

        std::cout << "\rFinished " << i+1 << "/" << chainLength << std::flush;

        //Sample past the burn in
        if(i >= burnIn && i % sampleRate == 0){
            std::vector<std::vector<bool>> shipPoistions = getShipPositions(board);
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