#include <stdio.h>

void func(){
    int y =4;
    y =5;
    printf("inside function!\n");
}

int main(){
    int x=1;
    x =2;
    func();
    printf("hi!\n");
    return 0;
}
