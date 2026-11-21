#ifndef ALERTS_H
#define ALERTS_H

//we want to set 4 levels of the alert state that get progressively higher
//so in terms of state-intensity: 0-no alert, 1-gentle, 2-stronger, 3-highest

class Alerts{
    public: 
        int level() const;
        int update(double now, double score, bool focusMode, bool ready);

    private:
        int current = 0;
        bool previousFocus = false;
        double aboveSince[3] = {-1, -1, -1};
        double lastWarning[3] = {-1000, -1000, -1000};
        double belowSince = -1;
};

#endif