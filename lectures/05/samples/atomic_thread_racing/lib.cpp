#include "lib.h"

void Increment(std::atomic<int>& counter)
{
	++counter;
}

void Decrement(std::atomic<int>& counter)
{
	--counter;
}