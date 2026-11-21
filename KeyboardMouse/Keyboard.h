#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <windows.h>

class Keyboard {
private:
    int keyPressCount = 0;

public:
    void handleInput(const RAWKEYBOARD& key); //RAWKEYBOARD is a data structure the contains info about a keyboard event
    int getAndResetCount();
};

#endif