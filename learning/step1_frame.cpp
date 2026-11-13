#include <iostream>
#include <string>
#include <vector>

using namespace std;

int main() {
    // cout << "\033[2J\033[5;20H@" << endl;
    const int W = 60, H = 20;

    string line(W, ' ');
    vector<string> grid(H, line); 

    for (int i = 0; i < grid.size(); i++) {
        if (i == 0 || i == grid.size() - 1) {
            grid[i] = string(W, '#');
        } else {
            grid[i][0] = '#';
            grid[i][W - 1] = '#';
        }
    }

    for (const auto& row : grid) {
        cout << row << endl;
    }
   

    
}
