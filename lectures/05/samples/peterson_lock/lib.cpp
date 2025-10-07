#include "lib.h"

void Increment(PetersonLock& lk, int& counter)
{
	lk.lock(0);
	++counter;
	lk.unlock(0);
}

void Decrement(PetersonLock& lk, int& counter)
{
	lk.lock(1);
	--counter;
	lk.unlock(1);
}