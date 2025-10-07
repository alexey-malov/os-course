#pragma once
#include <atomic>

class PetersonLock
{
public:
	PetersonLock() = default;

	PetersonLock(const PetersonLock&) = delete;
	PetersonLock& operator=(const PetersonLock&) = delete;

	void lock(int threadId)
	{
		int otherThreadId = 1 - threadId;
		m_wantsToEnter[threadId] = true;
		m_turn = threadId;
		while (m_wantsToEnter[otherThreadId] && m_turn == threadId)
			; // spin
	}

	void unlock(int threadId)
	{
		m_wantsToEnter[threadId] = false;
	}

private:
	volatile bool m_wantsToEnter[2]{ false, false };
	volatile int m_turn{ 0 };
};

void Increment(PetersonLock& lk, int& counter);
void Decrement(PetersonLock& lk, int& counter);
