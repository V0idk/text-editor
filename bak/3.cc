#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <limits>
#include <list>
#include <vector>
using namespace std;

vector<string> lines;

int main(){
    std::istringstream input("1\n"
                             "some non-numeric input\n\n\n\n"
                             "2\n");
    string line;
    while (getline(input, line,'\n') )
    {
        cout << line << endl;
        cout << line.length() << endl;
        lines.push_back(line);
    }
    cout << lines.size();
    return 0;
}


