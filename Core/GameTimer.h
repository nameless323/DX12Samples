#pragma once

class GameTimer
{
public:
    GameTimer();

    /// <summary> Total time in seconds </summary>
    float TotalTime() const;
    /// <summary> Delta time in seconds </summary>
    float DeltaTime() const;

    /// <summary> Call before message loop. </summary>
    void Reset();
    /// <summary> Call when unpaused. </summary>
    void Start();
    /// <summary> Call when paused. </summary>
    void Stop();
    /// <summary> Call every frame to update timer. </summary>
    void Tick();
private:
    double _secondsPerCount;
    double _dt;

    __int64 _baseTime;
    __int64 _pausedTime;
    __int64 _stopTime;
    __int64 _prevTime;
    __int64 _currTime;

    bool _stopped;
};