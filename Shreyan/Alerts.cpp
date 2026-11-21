#include "Alerts.h"

int Alerts::level() const{
    return current;
}

//now is the time elapsed in seconds. using that, want to reurtn a nonzero level only when
//the user needs a new warning

//theres also gonna be a "relaxed" and a "focused" mode. essentially we want the thresholds
//of the warnings to be higher in focused mode than in relaxed mode

int Alerts::update(double now, double score, bool focusMode, bool ready){
    const int relax[3] = {50, 70, 90};
    const int focus[3] = {65, 80, 90};
    
    const int* threshold;
    if (focusMode){
        threshold = focus;
    } else{
        threshold = relax;
    }

    if (!ready || focusMode != previousFocus){
        current = 0;
        belowSince = -1;
        for (int i=0; i < 3; i++){
            aboveSince[i] = -1;
        }
    }

    previousFocus = focusMode;
    if (!ready){
        return 0;
    }

    for (int i=0; i < 3; i++){
        if (score < threshold[i]){
            aboveSince[i] = -1;
        } else if (aboveSince[i] < 0){
            aboveSince[i] = now;
        }
    }

    if (current > 0 && score <= threshold[current-1] - 10){
        if (belowSince < 0){
            belowSince = now;
        }
        if (now - belowSince >= 30){
            while (current > 0 && score <= threshold[current-1] - 10){
                current--;
            }
            belowSince = -1;
        }
    } else{
        belowSince = -1;
    }

    for (int i = 2; i >= current; i--){
        if (aboveSince[i] >= 0 && now - aboveSince[i] >= 15){
            current = i+1;
            belowSince = -1;
            if (now - lastWarning[i] < 300){
                return 0;
            }
            lastWarning[i] = now;
            return current;
        }
    }

    return 0;
}