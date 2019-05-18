#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

using namespace std;

int
main()
{
    char c;
    while(1){
        cin >> c;
        cout << int(c) << endl;
    }
    return 0;
}