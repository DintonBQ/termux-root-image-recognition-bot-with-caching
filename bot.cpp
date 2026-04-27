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
#include <opencv2/opencv.hpp>
/*
pkg update
pkg upgrade
pkg install opencv
pkg install libavif
*/
//clang++ bot.cpp -o bot `pkg-config --cflags --libs opencv4` -std=c++17 && su -c ./bot
struct Pixel {
    unsigned char r, g, b, a;
};
std::vector<Pixel> screenBuffer; int screenW = 0, screenH = 0;
const char* IMAGE_BASE = "/storage/emulated/0/ROKbot/";





struct TemplateItem {
    std::string name;
    cv::Mat img;

    bool hasROI = false;
    cv::Rect roi;//region of interest
};

std::vector<TemplateItem> cacheList;

TemplateItem& cache(const char* filename)
{
    for (auto& t : cacheList)
        if (t.name == filename)
            return t;

    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s", IMAGE_BASE, filename);

    TemplateItem t;
    t.name = filename;
    t.img = cv::imread(fullpath, cv::IMREAD_GRAYSCALE);

    if (t.img.empty())
        std::cerr << "Failed load: " << fullpath << "\n";

    cacheList.push_back(std::move(t));

    return cacheList.back();
}


void tap(int x, int y){
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "input tap %d %d", x, y);
    system(cmd);
}
void wait(int sec){
std::this_thread::sleep_for(std::chrono::seconds(sec));
}
/*Pixel getpixel(int x, int y){
    return screenBuffer[y * screenW + x];
}*/
bool getscreen() {
    FILE* pipe = popen("screencap", "r");
    if (!pipe) {
        std::cerr << "Failed to run screencap\n";
        return false;
    }

    int format = 0;

    fread(&screenW, 4, 1, pipe);
    fread(&screenH, 4, 1, pipe);
    fread(&format, 4, 1, pipe);

    screenBuffer.resize(screenW * screenH);

    fread(screenBuffer.data(), sizeof(Pixel), screenW * screenH, pipe);

    pclose(pipe);
    return true;
}
/*inline bool match(const Pixel& a, const Pixel& b){
    int t = 25; // підбирай 10-40
    return abs(a.r - b.r) < t &&
           abs(a.g - b.g) < t &&
           abs(a.b - b.b) < t;
}*/
bool FindImageCV(TemplateItem& tpl,
                 int& outX, int& outY,
                 double& confidence,
                 bool cachedMode=true)
{
    static cv::Mat screenGray;

    if (screenGray.empty() ||
        screenGray.cols != screenW ||
        screenGray.rows != screenH)
    {
        screenGray.create(screenH, screenW, CV_8UC1);
    }

    for (int i = 0; i < screenW * screenH; i++) {
        screenGray.data[i] = screenBuffer[i].r;
    }

    cv::Rect searchArea;

    if (cachedMode && tpl.hasROI) {
        searchArea = tpl.roi & cv::Rect(0,0,screenW,screenH);
    } else {
        searchArea = cv::Rect(0,0,screenW,screenH);
    }

    cv::Mat screenROI = screenGray(searchArea);

    cv::Mat result;
    cv::matchTemplate(screenROI, tpl.img, result, cv::TM_CCOEFF_NORMED);

    cv::Point maxLoc;
    double maxVal;

    cv::minMaxLoc(result, 0, &maxVal, 0, &maxLoc);

    confidence = maxVal;

    if (maxVal > 0.80) {
        outX = maxLoc.x + tpl.img.cols / 2 + searchArea.x;
        outY = maxLoc.y + tpl.img.rows / 2 + searchArea.y;

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


    
    // конвертимо template в Pixel
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
int main() {
    while(1){
    if(!getscreen()) return 1;
    
    
char outpath[512];
snprintf(outpath, sizeof(outpath), "%sscreen.png", IMAGE_BASE);

/*stbi_write_png(//не обов'язково
    outpath,
    screenW,
    screenH,
    4,
    screenBuffer.data(),
    screenW * sizeof(Pixel)
);*/


int x, y;
double conf;

static TemplateItem& clan = cache("button.jpg");
if (FindImageCV(clan, x, y, conf)) {
    std::cout << "FOUND\n";
    std::cout << "conf=" << conf
              << " x=" << x
              << " y=" << y << "\n";

    /*Pixel p = getpixel(x, y);
    std::cout << "Pixel: "
              << (int)p.r << " "
              << (int)p.g << " "
              << (int)p.b << "\n";*/

    tap(x, y);
}
else {
    std::cout << "NOT FOUND\n";
}

    //wait(1);
    }

    return 0;
}