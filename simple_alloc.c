#include <stdlib.h>

struct TestStructA
{
    char a,b;
    float c;
    int d;
};

struct TestStruct
{
    int x;
    struct TestStructA y;
    double z;
    char c1[3];
    char c2[];
};

int main()
{
	__builtin_cheri_tagged_malloc(4, (struct TestStruct*) 0);
	return 0;
}
