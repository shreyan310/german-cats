#include "Keyboard.h"

//returns the count and resets it for the next time interval
int Keyboard::getAndResetCount() {
    int result = keyPressCount;
    keyPressCount = 0;
    return result;
}

//processes a keyboard event - only counting key down event, not key releases
void Keyboard::handleInput(const RAWKEYBOARD& key) {
    if (!(key.Flags & RI_KEY_BREAK)) {
        keyPressCount++;
    }
}