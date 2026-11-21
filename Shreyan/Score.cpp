#include "Score.h"
#include <cmath>
#include <algorithm>
using namespace std;

static double fraction(double x){
    return clamp(x, 0.0, 1.0);
}

double Score::reference(int channel, double value){
    if(value > 0 && referenceCount[channel] < 12){
        referenceSum[channel] = referenceSum[channel] + value;
        referenceCount[channel]++;
    }
    
    if(referenceCount[channel] == 0){
        return 0;
    }

    return referenceSum[channel] / referenceCount[channel];
}

Result Score::update(const Reading& r, double seconds){
    Result out;
    if (seconds>15 || seconds < 0 || !isfinite(seconds)){
        seconds = 0;
        postureHistory.clear();
        away = rest = 0;
    }

    time = time +seconds;
    bool postureOkay = r.postureConfidence >=0.6 && r.headDrop >=0;
    bool eyesOkay = r.eyeConfidence >=0.6;
    bool watching = eyesOkay && r.screen == 1;
    bool reading = eyesOkay && r.screen == -1 && r.FractionOfScreen >=0.8;
    bool lookingAway = eyesOkay && r.screen == 0;
    bool inputOkay = r.idleSeconds >=0;
    bool active = inputOkay && r.idleSeconds <=5;
    double reliability[4] = {};

    //making the readin gsmoother by combining the current posture readings with those taken in the last minute
    while (!postureHistory.empty() && time-postureHistory.front().time >= 60){
        postureHistory.pop_front();
    }

    if(postureOkay){
        postureHistory.push_back({time, r.headDrop >= 15});
        int bent = 0;
        for (const PostureSample& sample : postureHistory){
            if(sample.bent){
                bent++;
            }
        }
        out.posture = 65 * fraction((r.headDrop - 15) / 15) + 35.0*bent/postureHistory.size();
        reliability[0] = r.postureConfidence;
    }

    //eyes here
    if(watching){
        focus = focus +seconds;
    }

    if (lookingAway) {
        away = away + seconds;
    } else {
        away = 0;
    }

    if(away>=20){
        focus = 0;
    }

    if(eyesOkay){
        double sum = 0;
        int parts = 0;
        double baseline = reference(0, r.blinks);
        if(r.blinks >= 0 && baseline > 0){
            sum = sum+ fraction((0.75-r.blinks / baseline) / 0.45);
            parts++;
        }

        if(r.screen>=0){
            sum = sum + fraction((focus/60 - 5) / 15);
            parts++;
        }

        if(parts>0){
            out.eyes = 100 * sum/parts;
            reliability[1] = r.eyeConfidence * parts/2;
        }
    }


    if(inputOkay){
        const double share[4] = {0.5, 0.175, 0.175, 0.15};
        double intensity = 0, coverage = 0;
        if (active) {
            inputTime += seconds;
        } else {
            inputTime = std::max(0.0, inputTime - seconds);
        }

        for(int i=0; i<4; i++){
            double baseline = reference(i+1, active ? r.activity[i] : -1);
            if(r.activity[i]<0 || (r.activity[i]>0 && baseline ==0)){
                continue;
            }
            intensity = intensity + share[i]*fraction(r.activity[i] / (2*max(1.0, baseline)));
            coverage = coverage + share[i];
        }

        if(coverage>0){
            if(active){
                previousInput = 100*intensity/coverage * fraction(inputTime/1200);
            } else{
                previousInput = previousInput * exp(-seconds/120);
            }
            out.input = previousInput;
            reliability[2] = coverage;
        }
    }

    if (lookingAway && r.idleSeconds >= 5) {
        rest = rest + seconds;
    } else {
        rest = 0;
    }

    if(active || watching || reading || (postureOkay && !lookingAway)){
        out.state = "ACTIVE_WORK";
    } else if(rest>=20){
        out.state = "VERIFIED_BREAK";
    } else if(inputOkay || postureOkay || lookingAway){
        out.state = "LOW_ACTIVITY";
    } else{
        out.state = "SENSORS_UNAVAILABLE";
    }

    //Time at the desk still counts while reading without using the keyboard.
    if(out.state == "ACTIVE_WORK"){
        workTime += seconds;
    } else if(out.state == "VERIFIED_BREAK"){
        workTime = rest >= 300 ? 0 : max(0.0, workTime-seconds);
    }
    if(active || watching || reading || postureOkay || (lookingAway && inputOkay)){
        out.work = 100 * fraction((workTime/60 - 10)/50);
        reliability[3] = active ? 1.0 : max(r.postureConfidence, r.eyeConfidence);
        calibration += seconds;
    }

    const double weights[4] = {0.3, 0.25, 0.25, 0.2};
    double values[4] = {out.posture, out.eyes, out.input, out.work};
    double sum = 0, coverage = 0, largest = 0;

    int dominant = -1;
    bool improved = false;

    for(int i= 0; i<4; i++){
        if(previous[i] >=0 && values[i] < 0){
            holdRecovery = true;
        }
        if(previous[i] >=0 && values[i] >=0 && values[i] < previous[i]){
            improved = true;
        }

        previous[i] = values[i];
        if(values[i]<0){
            continue;
        }

        double contribution = weights[i] * values[i];
        sum = sum + contribution;
        coverage = coverage + weights[i];
        out.confidence = out.confidence + weights[i]*reliability[i];

        if(contribution>largest){
            largest = contribution;
            dominant = i;
        }
    }

    if(coverage>0.999 ||improved || out.state == "VERIFIED_BREAK"){
        holdRecovery = false;
    }

    if(coverage > 0 && out.confidence >= 0.2){
        double tau, target = sum/coverage;
        if(holdRecovery && target < total){
            target = total;
        }

        if(target>total){
            tau = 30;
        } else{
            tau = 120;
        }

        total = total + (target-total) * (1-exp(-seconds/tau));
    }

    out.score = total;
    if(out.confidence <0.2){
        out.quality = "unclear/interrupted";
    } else if(calibration < 30){
        out.quality = "calibrating now";
    } else if(coverage < 0.999 || out.confidence < 0.85 || (eyesOkay && r.screen < 0)){
        out.quality = "partially clear";
    } else{
        out.quality = "reliable";
    }

    const char* tips[] = {"Adjust your sitting position/posture", "Look away from your screen for 20 seconds", "Rest your hands for a bit", "You should move around a bit"};
    out.suggestion = "No changes needed";

    if(out.quality == "unclear/interrupted"){
        out.suggestion = "waiting for more reliable readings";
    } else if(out.quality == "calibrating now"){
        out.suggestion = "waiting to collect readings";
    } else if(dominant >=0 && total >=20){
        out.suggestion = tips[dominant];
    }

    return out;
}

