#include "FpsCounter.h"

FpsCounter::FpsCounter()
{
}

FpsCounter::~FpsCounter()
{
}

void FpsCounter::Initialize()
{
	m_fps = 0;
	m_count = 0;
	m_startTime = timeGetTime();
}

void FpsCounter::Frame()
{
	m_count++;
	if (timeGetTime() >= (m_startTime + 1000))
	{
		m_fps = m_count;
		m_count = 0;

		m_startTime = timeGetTime();
	}
}

int FpsCounter::GetFps()
{
	return m_fps;
}
