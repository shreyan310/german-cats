#include "Mouse.h"
#include <cmath>
using namespace std;

int Mouse::getAndResetClickCount() {
    int result = clickCount;
    clickCount = 0;
    return result;
}

double Mouse::getAndResetMovementDistance() {
    double result = movementDistance;
    movementDistance = 0.0;
    return result;
}

void Mouse::handleInput(const RAWMOUSE& mouse) {
    //count button presses
    USHORT clickFlags =
        RI_MOUSE_LEFT_BUTTON_DOWN |
        RI_MOUSE_RIGHT_BUTTON_DOWN |
        RI_MOUSE_MIDDLE_BUTTON_DOWN |
        RI_MOUSE_BUTTON_4_DOWN |
        RI_MOUSE_BUTTON_5_DOWN;
    
    if (mouse.usButtonFlags & clickFlags) {
        clickCount++;
    }

    //calculate mouse movement
    if (!(mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
        int dx = mouse.lLastX;
        int dy = mouse.lLastY;
        
        movementDistance += sqrt(dx*dx + dy*dy);
    }
}