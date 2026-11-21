#ifndef MOUSE_H
#define MOUSE_H

#include <windows.h>

class Mouse {
private:
    int clickCount = 0; //counting clicks
    double movementDistance = 0.0; //how far the mouse moves in total (pixels)

public:
    int getAndResetClickCount(); //returns click count and resets it
    double getAndResetMovementDistance(); //returns distance and resets it

    //processes a Windows mouse event
    void handleInput(const RAWMOUSE& mouse);
};

#endif