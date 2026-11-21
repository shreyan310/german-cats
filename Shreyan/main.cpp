#include "Alerts.h"
#include "Files.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

//NOTE: A LOT OF MY REFERENCE FOR THIS FILE CAME FROM
//https://mohammadijoo.github.io/CaaT_Academy/Lectures/Robot_Control_Course/Chapter12/Lesson4.html
//IN SECTION 7.2: C++: Periodic Real-Time Thread with Jitter Monitoring
//FRMO THE CAAT ACADEMY CHAPTER 12 ROBOT CONTROL COURSE

//AND ALSO https://learn.microsoft.com/en-us/cpp/cpp/main-function-command-line-args?view=msvc-170
//TO LEARN ABOUT THE ARGC AND THE ARGV PARAMETERS

//AND ALSO https://en.cppreference.com/cpp/filesystem/create_directory
//TO LEARN ABOUT THE filesystem stuff to create the data folder and check for errors

using namespace std;
using namespace std::chrono;

void updateMonitor(Score& calculator, Alerts& alerts, bool focusMode,
                   double elapsed, double totalTime) {
    Reading reading = readInputs();
    Result result = calculator.update(reading, elapsed);

    bool ready = result.quality == "reliable" || result.quality == "partially clear";
    if (elapsed > 15 || result.confidence < 0.60) {
        ready = false;
    }

    int warning = alerts.update(totalTime, result.score, focusMode, ready);
    if (!writeResult(result, reading, focusMode, alerts.level(), warning)) {
        cerr << "Could not write to the json, will try again\n";
    }
    if (warning > 0) {
        cout << result.suggestion << endl;
    }
}

int main(int argc, char* argv[]) {
    string mode = "relax";
    if (argc == 2) mode = argv[1];
    if (argc > 2 || (mode != "relax" && mode != "focus")) {
        cerr << "Usage: monitor [relax|focus]\n";
        return 1;
    }

    error_code error;
    filesystem::create_directories("data", error);
    if (error) {
        cerr << "cannot make the data folder\n";
        return 1;
    }

    startInputs();
    Score calculator;
    Alerts alerts;
    auto previous = steady_clock::now();
    auto next = previous;
    double totalTime = 0;

    while (true) {
        auto now = steady_clock::now();
        double elapsed = duration<double>(now - previous).count();
        previous = now;
        totalTime += elapsed;

        updateMonitor(calculator, alerts, mode == "focus", elapsed, totalTime);

        next += seconds(5);
        if (next <= steady_clock::now()) {
            next = steady_clock::now() + seconds(5);
        }
        waitForInputs(next); //process mouse and keyboard messages until the next score
    }
}