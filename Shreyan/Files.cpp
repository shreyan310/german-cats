//NOTE: MAJORITY OF THIS SINGULAR FILE IS AI GENERATED TO INCORPORATE READING AND WRITING
//TO A JSON FILE AND TO CONNECT THE TRACKERS

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../KeyboardMouse/Keyboard.h"
#include "../KeyboardMouse/Mouse.h"
#include "Files.h"
#include "picojson.h"
#include <conio.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

// emre's trackers are defined inside his demo.cpp, so give only its entry point
// another name so we can reuse the classes without running its display loop.
#define main emreDemoMain
#include "../UTEK2026/camera_posture_blink_demo.cpp"
#undef main

namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono;

Keyboard keyboard;
Mouse mouse;
int scrolls = 0;
bool inputReady = false;
Clock::time_point inputStart, lastInput;
bool seenInput = false;

//grouping every event by seconds
struct InputInterval{
    long long second;
    double count[4] = {};
};
deque<InputInterval> intervals;

void removeOldInputs(long long second){
    while(!intervals.empty() && second - intervals.front().second >= 60){
        intervals.pop_front();
    }
}

//even though windows gives these on the main thread, we wnat only the totals
LRESULT CALLBACK inputMessage(HWND window, UINT message, WPARAM w, LPARAM l){
    if(message == WM_INPUT){
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        if(GetRawInputData((HRAWINPUT)l, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1){
            bool used = false;
            if(raw.header.dwType == RIM_TYPEKEYBOARD){
                keyboard.handleInput(raw.data.keyboard);
                used = true;
            } else if(raw.header.dwType == RIM_TYPEMOUSE){
                const RAWMOUSE& event = raw.data.mouse;
                mouse.handleInput(event);
                if(event.usButtonFlags & (RI_MOUSE_WHEEL | RI_MOUSE_HWHEEL)) scrolls++;
                used = event.usButtonFlags || event.lLastX || event.lLastY;
            }
            if(used){
                lastInput = Clock::now();
                seenInput = true;
                long long second = duration_cast<seconds>(lastInput - inputStart).count();
                removeOldInputs(second);
                if(intervals.empty() || intervals.back().second != second){
                    intervals.push_back({second});
                }
                double* counts = intervals.back().count;
                counts[0] += keyboard.getAndResetCount();
                counts[1] += mouse.getAndResetClickCount();
                counts[2] += mouse.getAndResetMovementDistance();
                counts[3] += scrolls;
                scrolls = 0;
            }
        }
    }
    return DefWindowProcW(window, message, w, l);
}


mutex cameraLock;
Reading cameraReading;
Clock::time_point lastFrame;

struct EyeFrame{
    Clock::time_point time;
    double observed;
    unsigned int count;
};

void trackCamera(){
    try{
        MPPersonDet person("person_detection_mediapipe_2023mar.onnx");
        MPPose pose("pose_estimation_mediapipe_2023mar.onnx");
        EyeLandmarkDetector eyeDetector;
        if(!eyeDetector.ready()) cerr << "Eye tracker unavailable: check the mesh and cascade files.\n";
        VideoCapture camera(0);
        if(!camera.isOpened()){
            cerr << "Camera unavailable. Keyboard and mouse monitoring continues.\n";
            return;
        }

        PostureTracker posture;
        BlinkCounter blink;
        bool calibratePosture = false, calibrateEyes = false;
        deque<EyeFrame> frames;
        bool previousEyes = false;
        Clock::time_point previous = Clock::now();
        unsigned int previousCount = 0;
        double observed = 0;
        unsigned int minuteBlinks = 0;

        while(true){
            if(_kbhit()){
                int key = _getch();
                if(key == 'c' || key == 'C') calibratePosture = true;
                if(key == 'b' || key == 'B') calibrateEyes = true;
            }
            Mat frame;
            if(!camera.read(frame) || frame.empty()){
                cerr << "Camera stopped delivering frames. Restart to reconnect.\n";
                break;
            }
            Mat people = person.infer(frame);
            optional<float> height;
            Reading current;
            if(people.rows == 1){
                auto points = pose.infer(frame, people.row(0));
                if(!get<0>(points).empty()){
                    height = measureHeadHeight(get<1>(points));
                    if(height) current.postureConfidence = get<5>(points);
                }
            }
            auto postureSample = posture.update(height, calibratePosture);
            if(height) current.headHeight = *height;
            if(calibratePosture && height){
                calibratePosture = false;
                cout << "Posture calibrated.\n";
            }
            auto change = posture.changeFromBaseline(height);
            if(change){
                current.headDrop = max(0.0, -double(*change));
                current.loweredSeconds = postureSample.secondsOutsideBaseline;
            }

            auto eyes = eyeDetector.measure(frame);
            bool setEyes = calibrateEyes && eyes && eyes->first >= 0.10 && eyes->second >= 0.10;
            auto now = Clock::now();
            auto sample = blink.update(eyes, setEyes, now);
            if(setEyes){
                calibrateEyes = false;
                frames.clear();
                observed = 0;
                minuteBlinks = 0;
                previousEyes = false;
                cout << "Eyes calibrated.\n";
            }
            bool valid = sample.trackingAvailable && blink.calibrated();
            if(eyes){
                current.leftEAR = eyes->first;
                current.rightEAR = eyes->second;
            }
            if(valid){
                current.totalBlinks = sample.blinkCount;
                current.openSeconds = sample.secondsEyesOpen;
            }
            double dt = duration<double>(now - previous).count();
            double tracked = valid && previousEyes && dt <= 0.25 ? dt : 0;
            unsigned int added = sample.blinkCount - previousCount;
            frames.push_back({now, tracked, added});
            observed += tracked;
            minuteBlinks += added;
            while(!frames.empty() && now - frames.front().time >= seconds(60)){
                observed -= frames.front().observed;
                minuteBlinks -= frames.front().count;
                frames.pop_front();
            }
            if(valid && observed >= 55){
                current.blinks = minuteBlinks;
                current.eyeConfidence = min(1.0, observed/60);
            }
            previousCount = sample.blinkCount;
            previousEyes = valid;
            previous = now;
            lock_guard<mutex> guard(cameraLock);
            cameraReading = current;
            lastFrame = now;
        }
    } catch(const std::exception& error){
        cerr << "Camera tracker: " << error.what() << '\n';
    }
}
} // end of input helpers

void startInputs(){
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = inputMessage;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"MonitorRawInput";
    if(RegisterClassW(&windowClass)){
        HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"", 0, 0, 0, 0, 0,
                                      HWND_MESSAGE, nullptr, windowClass.hInstance, nullptr);
        if(window){
            RAWINPUTDEVICE devices[] = {{1, 6, RIDEV_INPUTSINK, window}, {1, 2, RIDEV_INPUTSINK, window}};
            inputReady = RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));
        }
    }
    if(!inputReady) cerr << "Keyboard/mouse registration failed: " << GetLastError() << '\n';
    inputStart = Clock::now();
    cout << "Sit normally and press C, then face the camera with eyes open and press B in this console.\n";
    thread(trackCamera).detach();
}

void waitForInputs(Clock::time_point next){
    MSG message;
    while(Clock::now() < next){
        if(PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)){
            DispatchMessageW(&message);
        } else{
            auto ms = ceil<milliseconds>(next - Clock::now()).count();
            if(ms > 0) MsgWaitForMultipleObjectsEx(0, nullptr, DWORD(ms), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
    }
}

Reading readInputs(){
    Reading r;
    auto now = Clock::now();
    {
        lock_guard<mutex> guard(cameraLock);
        if(now - lastFrame <= seconds(2)) r = cameraReading;
    }
    if(!inputReady) return r;
    removeOldInputs(duration_cast<seconds>(now - inputStart).count());
    if(seenInput) r.idleSeconds = duration<double>(now - lastInput).count();
    if(now - inputStart >= seconds(60)){
        for(int i=0; i<4; i++){
            r.activity[i] = 0;
            for(const InputInterval& interval : intervals) r.activity[i] += interval.count[i];
        }
    }
    return r;
}

using picojson::value;
using picojson::object;

static value component(double x) {
    if (x < 0) return value(); // JSON null
    return value(x);
}

bool writeResult(const Result& r, const Reading& reading, bool focusMode, int level, int warning) {
    std::time_t seconds = std::time(nullptr);
    std::ostringstream stamp;
    stamp << std::put_time(std::gmtime(&seconds), "%Y-%m-%dT%H:%M:%SZ");
    object parts;
    parts["posture"] = component(r.posture);
    parts["eyes"] = component(r.eyes);
    parts["repetitive_activity"] = component(r.input);
    parts["prolonged_work"] = component(r.work);

    const char* labels[] = {"monitoring", "caution", "elevated", "high"};
    object record;
    record["timestamp"] = value(stamp.str());
    record["score"] = value(r.score);
    record["confidence"] = value(r.confidence);
    record["mode"] = value(focusMode ? "focus" : "relax");
    record["components"] = value(parts);
    object measurements;
    measurements["shoulder_to_nose_px"] = component(reading.headHeight);
    measurements["head_drop_px"] = component(reading.headDrop);
    measurements["seconds_below_baseline"] = component(reading.loweredSeconds);
    measurements["blink_count"] = component(reading.totalBlinks);
    measurements["blinks_60s"] = component(reading.blinks);
    measurements["left_ear"] = component(reading.leftEAR);
    measurements["right_ear"] = component(reading.rightEAR);
    measurements["eyes_open_seconds"] = component(reading.openSeconds);
    const char* names[] = {"keystrokes_60s", "mouse_clicks_60s", "mouse_distance_raw_60s", "scroll_events_60s"};
    for(int i=0; i<4; i++) measurements[names[i]] = component(reading.activity[i]);
    record["measurements"] = value(measurements);
    record["work_state"] = value(r.state);
    record["data_quality"] = value(r.quality);
    record["status"] = value(r.quality == "unclear/interrupted" || r.quality == "calibrating now" ? r.quality : labels[level]);
    record["recommendation"] = value(r.suggestion);
    record["monitoring"] = value(true);
    record["new_warning_level"] = value(double(warning));

    const char* temp = "data/metrics.tmp";
    const char* finalName = "data/metrics.json";
    std::ofstream file(temp);
    file << value(record).serialize(true);
    file.close();
    if (!file) return false;
#ifdef _WIN32
    return MoveFileExA(temp, finalName, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return std::rename(temp, finalName) == 0;
#endif
}