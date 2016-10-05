#include <windows.h>

#include "GameTimer.h"

GameTimer::GameTimer() : _secondsPerCount(0.0), _dt(-1.0f), _baseTime(0), _pausedTime(0), _stopTime(0), _prevTime(0), _currTime(0), _stopped(false)
{
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    _secondsPerCount = 1.0 / (double)countsPerSec;
}

float GameTimer::TotalTime() const
{
    if (_stopped)
        return (float)(((_stopTime - _pausedTime) - _baseTime)*_secondsPerCount);
    return (float)((_currTime - _pausedTime) - _baseTime) * _secondsPerCount;
}

float GameTimer::DeltaTime() const
{
    return (float)_dt;
}

void GameTimer::Reset()
{
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

    _baseTime = currTime;
    _prevTime = currTime;
    _stopTime = 0;
    _stopped = false;
}

void GameTimer::Start()
{
    __int64 startTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

    if (_stopped)
    {
        _pausedTime += (startTime - _stopTime);
        _prevTime = startTime;
        _stopTime = 0;
        _stopped = false;
    }
}

void GameTimer::Stop()
{
    if (!_stopped)
    {
        __int64 currTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
        _stopTime = currTime;
        _stopped = true;
    }
}

void GameTimer::Tick()
{
    if (_stopped)
    {
        _dt = 0.0;
        return;
    }
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    _currTime = currTime;
    _dt = (currTime - _prevTime) * _secondsPerCount;

    _prevTime = _currTime;

    if (_dt < 0.0)
        _dt = 0.0;
}
