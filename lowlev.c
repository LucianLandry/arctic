#include "ref.h"

void ncopy(char a[], char b[], int n)
/* unconditionally copies n chars from b to a. */
{	int i;
	for (i = 0; i < n;)
		a[i] = b[i++];
}

