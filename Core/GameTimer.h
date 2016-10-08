//
// Describes timer used throughout the game.
//

#pragma once

namespace DX12Samples
{
class GameTimer
{
public:
    GameTimer();

    /**
     * \brief Total time in seconds.
     */
    float TotalTime() const;
    /**
     * \brief Delta time in seconds.
     */
    float DeltaTime() const;
    /**
     * \brief Call before message loop.
     */
    void Reset();
    /**
     * \brief Call when unpaused.
     */
    void Start();
    /**
     * \brief Call when paused.
     */
    void Stop();
    /**
     * \brief Call every frame to update timer.
     */
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
}