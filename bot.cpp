/*#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image_write.h"
#include "stb_image.h"*/
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <unistd.h>
#include <opencv2/opencv.hpp>
/*
pkg update
pkg upgrade
pkg install opencv
pkg install libavif
*/
//clang++ bot.cpp -o bot `pkg-config --cflags --libs opencv4` -std=c++17 && su -c ./bot
//ROKbot, Xiaomi Redmi Note 13, landscape orientation mode, focaltech touchscreen, root, termux, android, automation, image recognition autoclicker
struct Pixel {
    unsigned char r, g, b, a;
};
std::vector<Pixel> screenBuffer; int screenW = 0, screenH = 0;
const char* IMAGE_BASE = "/storage/emulated/0/ROKbot/";

bool REC_MODE = false;
FILE* recFile = nullptr;

/* focaltech touchscreen Xiaomi Redmi Note 13
~/ROKbot $ su -c getevent -p /dev/input/event5
add device 1: /dev/input/event5
  name:     "fts_ts"
  events:
    KEY (0001): 0011  0012  0018  001f  0026  002c  002e  002f
                0032  0067  0069  006a  006c  0074  008e  008f
                014a  0152  0162
    ABS (0003): 002f  : value 0, min 0, max 9, fuzz 0, flat 0, resolution 0
                0030  : value 0, min 0, max 255, fuzz 0, flat 0, resolution 0
                0032  : value 0, min 0, max 17278, fuzz 0, flat 0, resolution 0
                0033  : value 0, min 0, max 17278, fuzz 0, flat 0, resolution 0
                0035  : value 0, min 0, max 17279, fuzz 0, flat 0, resolution 0
                0036  : value 0, min 0, max 38399, fuzz 0, flat 0, resolution 0
                0039  : value 0, min 0, max 65535, fuzz 0, flat 0, resolution 0
  input props:
    INPUT_PROP_DIRECT
~/ROKbot $
*/
const int MAX_RAW_X = 17279; 
const int MAX_RAW_Y = 38399;




void recThread() {//horizontal mode
   
    //screenW screenH from getscreen()

    FILE* pipe = popen("getevent -lt /dev/input/event5", "r");//touchscreen send orientation-less coordinates(always in portrait mode)
    if (!pipe) {
        std::cerr << "getevent failed\n";
        return;
    }

    

    std::cout << "Listening for touch events...\n";

    char line[512];
    int rawX = -1, rawY = -1;

    while (fgets(line, sizeof(line), pipe)) {
        std::string s(line);

        //search last word(hex)
        std::stringstream ss(s);
        std::string word;
        while (ss >> word) {} 

        if (s.find("ABS_MT_POSITION_X") != std::string::npos) {
            rawX = (int)std::stoul(word, nullptr, 16);
            //std::cout << "[RAW X] " << rawX << "\n";
        } 
        else if (s.find("ABS_MT_POSITION_Y") != std::string::npos) {
            rawY = (int)std::stoul(word, nullptr, 16);
            //std::cout << "[RAW Y] " << rawY << "\n";
        }
else if (s.find("BTN_TOUCH") != std::string::npos && s.find("UP") != std::string::npos) {
    if (recFile && rawX >= 0 && rawY >= 0) {
        if (screenW <= 0 || screenH <= 0) {//prevent divide by zero
        std::cout << "getscreen not working, use alt sizes\n";
            screenW = 2400; screenH = 1080;
        }

        std::cout << "[RAW] " << rawX << "," << rawY << " | [SCREEN] " << screenW << "x" << screenH << "\n";
        int finalX = (int)((double)rawY * screenW / (MAX_RAW_Y + 1));
        int finalY = (int)((double)(MAX_RAW_X - rawX) * screenH / (MAX_RAW_X + 1));

        fprintf(recFile, "%d,%d\n", finalX, finalY);
        fflush(recFile);
        
        std::cout << "=> SAVED: " << finalX << "," << finalY << "\n";
        
        //reset for next click
        rawX = -1; rawY = -1;
    }
}
        
    }
    pclose(pipe);
}




static void clearDebugImages() {
if (!std::filesystem::exists(IMAGE_BASE)) return;
    for (const auto& entry : std::filesystem::directory_iterator(IMAGE_BASE)) {
        if (entry.is_regular_file()) {
            auto path = entry.path().string();
            if (path.find("debug_") != std::string::npos &&
                path.find(".jpg") != std::string::npos) {
                std::filesystem::remove(entry.path());
            }
        }
    }
}


struct TemplateItem {
    std::string name;
    cv::Mat img;

    bool hasROI = false;
    cv::Rect roi;//region of interest
};

TemplateItem openTemplate(const char* filename)
{
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s", IMAGE_BASE, filename);

    TemplateItem t;
    t.name = filename;
    t.img = cv::imread(fullpath, cv::IMREAD_COLOR);

    if (t.img.empty())
        std::cerr << "Failed load: " << fullpath << "\n";

    return t;
}

void wait(int sec){
std::this_thread::sleep_for(std::chrono::seconds(sec));
}

void tap(int x, int y, bool randomize=true){
    if(randomize){
    x += (rand() % 11) - 5; // -5 ... +5
    y += (rand() % 11) - 5;
    }
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "input tap %d %d", x, y);
    system(cmd);
    wait(1);//wait game animations
}

/*Pixel getpixel(int x, int y){
    return screenBuffer[y * screenW + x];
}*/
bool getscreen(bool onlysize=false) {
    FILE* pipe = popen("screencap", "r");
    if (!pipe) {
        std::cerr << "Failed to run screencap\n";
        return false;
    }

    int format = 0;

    
if (fread(&screenW, 4, 1, pipe) != 1) { pclose(pipe); return false; }
if (fread(&screenH, 4, 1, pipe) != 1) { pclose(pipe); return false; }
if(onlysize)return true;
if (fread(&format, 4, 1, pipe) != 1) { pclose(pipe); return false; }

    screenBuffer.resize(screenW * screenH);

    fread(screenBuffer.data(), sizeof(Pixel), screenW * screenH, pipe);

    pclose(pipe);
    return true;
}
/*inline bool match(const Pixel& a, const Pixel& b){
    int t = 25;//alt confidence
    return abs(a.r - b.r) < t &&
           abs(a.g - b.g) < t &&
           abs(a.b - b.b) < t;
}*/
bool FindImageCV(TemplateItem& tpl,
                 int& outX, int& outY,
                 double& confidence,
                 bool cachedMode=false) // cachedMode with errors now :(
{
if (tpl.img.empty())
    {
    std::cout << "error templace not loaded\n";
    return false;}
    
    

    cv::Mat screenMat(screenH, screenW, CV_8UC4, screenBuffer.data());

cv::Mat screenBGR;
cv::cvtColor(screenMat, screenBGR, cv::COLOR_RGBA2BGR);

    cv::Rect searchArea;

    if (cachedMode && tpl.hasROI) {
        searchArea = tpl.roi & cv::Rect(0,0,screenW,screenH);
    } else {
        searchArea = cv::Rect(0,0,screenW,screenH);
    }


    cv::Mat screenROI = screenBGR(searchArea);



if (tpl.img.cols > screenROI.cols ||
    tpl.img.rows > screenROI.rows)
{
std::cout << "error template bigger than screen\n";
    return false;
}
    cv::Mat result;
    cv::matchTemplate(screenROI, tpl.img, result, cv::TM_CCOEFF_NORMED);

    cv::Point maxLoc;
    double maxVal;

    cv::minMaxLoc(result, 0, &maxVal, 0, &maxLoc);

    confidence = maxVal;



if (maxVal > 0.80) {

    outX = maxLoc.x + tpl.img.cols / 2 + searchArea.x;
    outY = maxLoc.y + tpl.img.rows / 2 + searchArea.y;

    cv::Rect foundRect(
        maxLoc.x + searchArea.x,
        maxLoc.y + searchArea.y,
        tpl.img.cols,
        tpl.img.rows
    );

    cv::rectangle(
        screenBGR,
        foundRect,
        cv::Scalar(0,0,255),
        3
    );

    cv::drawMarker(
        screenBGR,
        cv::Point(outX, outY),
        cv::Scalar(0,0,255),
        cv::MARKER_CROSS,
        40,
        3
    );
    
    //template name on debug image
    cv::putText(
    screenBGR,
    tpl.name,
    cv::Point(foundRect.x, foundRect.y - 35),
    cv::FONT_HERSHEY_SIMPLEX,
    0.7,
    cv::Scalar(255, 0, 0),
    2
);
    // ===== CONFIDENCE TEXT =====
    char confText[64];
    snprintf(confText, sizeof(confText), "conf: %.2f", maxVal);

    cv::putText(
        screenBGR,
        confText,
        cv::Point(foundRect.x, foundRect.y - 10),
        cv::FONT_HERSHEY_SIMPLEX,
        0.7,
        cv::Scalar(0, 255, 0),
        2
    );
    // ============================
    
static int debugIndex = 0;
    char debugPath[512];
    snprintf(
        debugPath,
        sizeof(debugPath),
        "%sdebug_%d.jpg",
        IMAGE_BASE,
        debugIndex
    );
    debugIndex++;

    cv::imwrite(debugPath, screenBGR);

    if (!tpl.hasROI) {
        tpl.roi = cv::Rect(outX - 200, outY - 200, 400, 400)
                 & cv::Rect(0,0,screenW,screenH);

        tpl.hasROI = true;
    }

    return true;
}


    return false;
}

/*
bool FindImage(const char* filename,
               int& outX, int& outY)
{
char fullpath[512];
snprintf(fullpath, sizeof(fullpath), "%s%s", IMAGE_BASE, filename);

int templateW, templateH, templateC;
unsigned char* data = stbi_load(fullpath, &templateW, &templateH, &templateC, 4);
if (!data) {
    std::cerr << "Failed load template: " << fullpath << "\n";
    return false;
}


    
    //convert template to Pixel
    std::vector<Pixel> templ(templateW * templateH);
    memcpy(templ.data(), data, templateW * templateH * 4);
    stbi_image_free(data);

    for (int y = 0; y <= screenH - templateH; y++) {
        for (int x = 0; x <= screenW - templateW; x++) {

            bool ok = true;

            for (int ty = 0; ty < templateH && ok; ty++) {
                for (int tx = 0; tx < templateW; tx++) {

                    const Pixel& p1 = screenBuffer[(y + ty) * screenW + (x + tx)];
                    const Pixel& p2 = templ[ty * templateW + tx];

                    if (!match(p1, p2)) {
                        ok = false;
                        break;
                    }
                }
            }

            if (ok) {
                outX = x + templateW / 2;
                outY = y + templateH / 2;
                return true;
            }
        }
    }

    return false;
}*/
int main(int argc, char** argv) {
for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "rec") {
        REC_MODE = true;
    }
}
srand(time(nullptr));
clearDebugImages();
if (REC_MODE) {
    recFile = fopen((std::string(IMAGE_BASE) + "pos.txt").c_str(), "w");
    if (!recFile) {
        std::cerr << "failed to open pos.txt\n";
        return 1;
    }

    std::cout << "REC MODE ON\n";

if(!getscreen(true)) return 1;//get screenW screenH
    recThread();
    return 0;
}

    while(1){
    static int scr=1;
    std::cout << "GETSCREEN "<<scr<<"\n"; scr++;
    if(!getscreen()) return 1;
    
/*
char outpath[512];
snprintf(outpath, sizeof(outpath), "%sscreen.png", IMAGE_BASE);

stbi_write_png(//not necessary
    outpath,
    screenW,
    screenH,
    4,
    screenBuffer.data(),
    screenW * sizeof(Pixel)
);*/


int x, y;
double conf;

static TemplateItem hu = openTemplate("help ui.jpg");
if (FindImageCV(hu, x, y, conf)) {
tap(x, y);
static TemplateItem gh = openTemplate("goto home.jpg");
if (FindImageCV(gh, x, y, conf)) tap(x, y);
static TemplateItem hh = openTemplate("help house.jpg");
if (FindImageCV(hh, x, y, conf)) {tap(x, y);  
    /*Pixel p = getpixel(x, y);
    std::cout << "Pixel: "
              << (int)p.r << " "
              << (int)p.g << " "
              << (int)p.b << "\n";*/
    }
static TemplateItem hr = openTemplate("help request.jpg");
if (FindImageCV(hr, x, y, conf)) tap(x, y);
}//endof if help ui


static TemplateItem gm = openTemplate("goto map.jpg");
if (FindImageCV(gm, x, y, conf)) tap(x, y);

static TemplateItem sf = openTemplate("search barb fort.jpg");
static TemplateItem bf = openTemplate("barb fort.jpg");
static TemplateItem cr = openTemplate("collect red.jpg");
static TemplateItem sb = openTemplate("search btn.jpg");
bool iscr=FindImageCV(cr, x, y, conf);
if (iscr) {tap(x, y);continue;}
bool isbf=FindImageCV(bf, x, y, conf);
if (!isbf&&/*!iscr&&*/FindImageCV(sf, x, y, conf)) {tap(x, y); continue;}
if (isbf&&FindImageCV(sb, x, y, conf)) {
    tap(x, y);continue;}
static TemplateItem cb = openTemplate("collect blue.jpg");
if (FindImageCV(cb, x, y, conf)) {tap(x, y);continue;}





    //wait(1);
    }

    return 0;
}
