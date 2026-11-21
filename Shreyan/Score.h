#ifndef SCORE_H
#define SCORE_H

#include <string>
#include <deque>
using namespace std;

//for most need to use -1 as placeholder for unassigned rn 
//Emre measures the drop in shoulder-to-nose height from neutral, in pixels.

struct Reading{
    double headDrop = -1, postureConfidence = -1, blinks = -1, eyeConfidence = -1, FractionOfScreen = -1, idleSeconds = -1;
    int screen = -1; //the camera does not measure gaze direction
    double headHeight = -1, loweredSeconds = -1, totalBlinks = -1;
    double leftEAR = -1, rightEAR = -1, openSeconds = -1;

    //from the other one need the keystroke rate, click rate, relative mouse movement, scrolling rate
    double activity[4] = {-1, -1, -1, -1};
};

struct Result{
    double posture = -1, eyes = -1, input = -1, work = -1, score = 0, confidence = 0;
    string quality, state, suggestion;
};

class Score{
    public:
        Result update(const Reading& reading, double seconds);
    
    private:
        double referenceSum[5] = {};
        int referenceCount[5] = {};
        double reference(int channel, double value);

        struct PostureSample{
            double time;
            bool bent;
        };

        deque<PostureSample> postureHistory;

        double time = 0, calibration = 0, focus = 0, away = 0, rest = 0, workTime = 0, inputTime = 0, previousInput = 0, total = 0;
        double previous[4] = {-1, -1, -1, -1};
        bool holdRecovery = false;
};

#endif