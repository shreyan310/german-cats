// Windows display for the existing monitor. The monitor owns all scoring and alert decisions.
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include "picojson.h"

using namespace std;
namespace fs = std::filesystem;
namespace {
HANDLE process = nullptr, job = nullptr;
NOTIFYICONDATAW tray{};
bool hasTray = false, running = false;
HWND relaxButton, focusButton;
HFONT font;
fs::path folder, metrics;
string lastStamp, message = "Choose a mode to start.", advice;
string rows[5];
const char* labels[] = {"Posture strain", "Eye strain", "Repetitive activity", "Prolonged work", "Overall score"};
const char* fields[] = {"posture", "eyes", "repetitive_activity", "prolonged_work"};
ULONGLONG lastUpdate;

string bar(const picojson::value& value){
    if(!value.is<double>()) return "[          ]  --";
    double score = value.get<double>();
    if(!isfinite(score) || score < 0 || score > 100) return "[          ]  --";
    int filled = int(lround(score / 10));
    return "[" + string(filled, '#') + string(10-filled, '-') + "]  " + to_string(lround(score));
}

picojson::value snapshot(){
    ifstream file(metrics);
    picojson::value data;
    if(!file || !picojson::parse(data, file).empty() || !data.is<picojson::object>()) return {};
    return data;
}

string textField(const picojson::value& data, const char* name){
    if(!data.is<picojson::object>() || !data.get(name).is<string>()) return "";
    return data.get(name).get<string>();
}

void notify(HWND window, int level){
    if(!hasTray){
        tray.uFlags = NIF_ICON | NIF_TIP;
        hasTray = Shell_NotifyIconW(NIM_ADD, &tray);
    }
    if(!hasTray) return;
    tray.uFlags = NIF_INFO;
    const wchar_t* titles[] = {L"", L"Workstation reminder", L"Workstation warning", L"Workstation: high score"};
    lstrcpynW(tray.szInfoTitle, titles[level], ARRAYSIZE(tray.szInfoTitle));
    MultiByteToWideChar(CP_UTF8, 0, advice.c_str(), -1, tray.szInfo, ARRAYSIZE(tray.szInfo));
    tray.dwInfoFlags = level == 1 ? NIIF_INFO : NIIF_WARNING;
    Shell_NotifyIconW(NIM_MODIFY, &tray);
}

void refresh(HWND window){
    if(!running) return;
    if(WaitForSingleObject(process, 0) != WAIT_TIMEOUT){
        message = "Monitor stopped. Close and reopen to retry.";
        running = false;
        for(auto& row : rows) row = bar({});
    } else {
        auto record = snapshot();
        string stamp = textField(record, "timestamp");
        if(!stamp.empty() && stamp != lastStamp){
            lastStamp = stamp;
            lastUpdate = GetTickCount64();
            auto& parts = record.get("components");
            for(int i=0; i<4; i++) rows[i] = parts.is<picojson::object>() ? bar(parts.get(fields[i])) : bar({});
            string quality = textField(record, "data_quality");
            bool usable = quality == "reliable" || quality == "partially clear";
            rows[4] = usable ? bar(record.get("score")) : bar({});
            advice = textField(record, "recommendation");
            message = textField(record,"mode") + " | " + quality;
            auto& measurements = record.get("measurements");
            if(measurements.is<picojson::object>()){
                if(measurements.get("head_drop_px").is<picojson::null>()) message += " | C: posture";
                if(measurements.get("blink_count").is<picojson::null>()) message += " | B: eyes";
            }
            auto& warning = record.get("new_warning_level");
            if(usable && warning.is<double>()){
                double level = warning.get<double>();
                if(level==1 || level==2 || level==3) notify(window,int(level));
            }
        }
        if(GetTickCount64()-lastUpdate > 15000){
            message = "Waiting for fresh monitor data.";
            for(auto& row : rows) row = bar({});
        }
    }
    InvalidateRect(window,nullptr,FALSE);
}

void start(HWND window, bool focus){
    lastStamp = textField(snapshot(), "timestamp");
    auto exe = folder / L"monitor.exe";
    wstring cmd = L"\"" + exe.wstring() + L"\" " + (focus ? L"focus" : L"relax");
    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,
                       CREATE_NEW_CONSOLE | CREATE_SUSPENDED,nullptr,folder.c_str(),&si,&pi)){
        MessageBoxW(window,L"Cannot launch monitor.exe. Extract the whole folder and check its dependencies.",
                    L"Workstation monitor",MB_OK); return;
    }
    if(!AssignProcessToJobObject(job,pi.hProcess)){
        TerminateProcess(pi.hProcess,1); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        MessageBoxW(window,L"Windows could not start the monitoring session.",L"Workstation monitor",MB_OK); return;
    }
    process = pi.hProcess;
    ResumeThread(pi.hThread); CloseHandle(pi.hThread);
    running = true; lastUpdate = GetTickCount64();
    ShowWindow(relaxButton, SW_HIDE); ShowWindow(focusButton, SW_HIDE);
    SetFocus(window);
    message = "Sit normally: C. Face camera, eyes open: B.";
    InvalidateRect(window,nullptr,FALSE);
}

// Forward only the user's two calibration commands to the monitor's existing console.
void calibrate(HWND window, char key){
    FreeConsole();
    if(!AttachConsole(GetProcessId(process))){
        message = "Monitor starting. Press the calibration key again.";
        InvalidateRect(window,nullptr,FALSE); return;
    }
    HANDLE input = CreateFileW(L"CONIN$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                               nullptr,OPEN_EXISTING,0,nullptr);
    INPUT_RECORD events[2]{};
    for(int i=0;i<2;i++){
        events[i].EventType = KEY_EVENT;
        auto& k = events[i].Event.KeyEvent;
        k.bKeyDown = (i==0); k.wRepeatCount=1; k.wVirtualKeyCode=key;
        k.wVirtualScanCode = WORD(MapVirtualKeyW(key,MAPVK_VK_TO_VSC));
        k.uChar.UnicodeChar=key;
    }
    DWORD written=0;
    if(input!=INVALID_HANDLE_VALUE){
        WriteConsoleInputW(input,events,2,&written); CloseHandle(input);
    }
    FreeConsole();
    message = written==2 ? "Calibration requested. Waiting for readings." : "Calibration failed. Press the key again.";
    InvalidateRect(window,nullptr,FALSE);
}

LRESULT CALLBACK handle(HWND window,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_COMMAND && !running && !process){
        if(LOWORD(w)==1 || LOWORD(w)==2) start(window,LOWORD(w)==2);
        return 0;
    }
    if(msg==WM_KEYDOWN && running && !(l & (1L<<30)) && (w=='C' || w=='B')){
        calibrate(window,char(w)); return 0;
    }
    if(msg==WM_TIMER){ refresh(window); return 0; }
    if(msg==WM_PAINT){
        PAINTSTRUCT ps; HDC dc=BeginPaint(window,&ps);
        RECT area; GetClientRect(window,&area);
        FillRect(dc,&area,(HBRUSH)GetStockObject(BLACK_BRUSH));
        SelectObject(dc,font); SetTextColor(dc,RGB(255,255,255)); SetBkMode(dc,TRANSPARENT);
        for(int i=0;i<4;i++){
            TextOutA(dc,16,12+26*i,labels[i],int(strlen(labels[i])));
            TextOutA(dc,220,12+26*i,rows[i].c_str(),int(rows[i].size()));
        }
        string overall = string(labels[4])+"  "+rows[4];
        RECT line{0,127,area.right,155}; DrawTextA(dc,overall.c_str(),-1,&line,DT_CENTER|DT_SINGLELINE);
        SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));
        RECT status{8,166,area.right-8,190}; DrawTextA(dc,message.c_str(),-1,&status,DT_CENTER|DT_SINGLELINE);
        EndPaint(window,&ps); return 0;
    }
    if(msg==WM_DESTROY){
        KillTimer(window,1); if(hasTray) Shell_NotifyIconW(NIM_DELETE,&tray);
        if(job) CloseHandle(job);
        if(process) CloseHandle(process);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window,msg,w,l);
}
}

int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int){
    HANDLE single=CreateMutexW(nullptr,FALSE,L"Local\\UTEKWorkstationDisplay");
    if(!single || GetLastError()==ERROR_ALREADY_EXISTS){
        MessageBoxW(nullptr,L"A dashboard is already open.",L"Workstation monitor",MB_OK); return 0;
    }
    wchar_t path[32768]; GetModuleFileNameW(nullptr,path,ARRAYSIZE(path));
    folder=fs::path(path).parent_path(); metrics=folder/L"data"/L"metrics.json";
    job=CreateJobObjectW(nullptr,nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job || !SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) return 1;
    for(auto& row:rows) row=bar({});
    font=CreateFontW(-18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,FIXED_PITCH,L"Consolas");
    WNDCLASSW wc{}; wc.lpfnWndProc=handle; wc.hInstance=instance; wc.lpszClassName=L"WorkstationScores";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW); RegisterClassW(&wc);
    DWORD style=WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    RECT size{0,0,428,224}; AdjustWindowRect(&size,style,FALSE);
    HWND win=CreateWindowW(wc.lpszClassName,L"Workstation monitor",style,CW_USEDEFAULT,CW_USEDEFAULT,
        size.right-size.left,size.bottom-size.top,nullptr,nullptr,instance,nullptr);
    if(!win) return 1;
    relaxButton=CreateWindowW(L"BUTTON",L"Relax",WS_CHILD|WS_VISIBLE,92,190,110,25,win,(HMENU)1,instance,nullptr);
    focusButton=CreateWindowW(L"BUTTON",L"Focus",WS_CHILD|WS_VISIBLE,226,190,110,25,win,(HMENU)2,instance,nullptr);
    tray.cbSize=sizeof(tray); tray.hWnd=win; tray.uID=1; tray.hIcon=LoadIcon(nullptr,IDI_INFORMATION);
    lstrcpyW(tray.szTip,L"Workstation monitor");
    SetTimer(win,1,1000,nullptr); ShowWindow(win,SW_SHOW);
    MSG msg; while(GetMessageW(&msg,nullptr,0,0)>0){ TranslateMessage(&msg); DispatchMessageW(&msg); }
    DeleteObject(font); CloseHandle(single); return 0;
}
