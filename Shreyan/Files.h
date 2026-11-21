#ifndef FILES_H
#define FILES_H

#include "Score.h"
#include <chrono>

void startInputs();
void waitForInputs(std::chrono::steady_clock::time_point next);

//from https://mohammadijoo.github.io/CaaT_Academy/Lectures/Robot_Control_Course/Chapter12/Lesson4.html
//chapter 12 unit 7.2

Reading readInputs();
bool writeResult(const Result& result, const Reading& reading, bool focusMode, int level, int warning);

#endif