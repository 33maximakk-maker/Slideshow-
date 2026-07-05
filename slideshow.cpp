// slideshow.cpp
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>

using namespace std;
using namespace cv;
namespace fs = std::filesystem;

string RESET = "\033[0m";
string BOLD = "\033[1m";
string GREEN = "\033[92m";
string YELLOW = "\033[93m";
string BLUE = "\033[94m";

string rgbToAnsi(int r, int g, int b) {
    return "\033[38;2;" + to_string(r) + ";" + to_string(g) + ";" + to_string(b) + "m";
}

vector<string> getImageFiles(const vector<string>& paths, bool recursive) {
    vector<string> files;
    vector<string> exts = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".webp"};
    for (const string& path : paths) {
        if (fs::is_regular_file(path)) {
            files.push_back(path);
        } else if (fs::is_directory(path)) {
            if (recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        string ext = entry.path().extension().string();
                        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (find(exts.begin(), exts.end(), ext) != exts.end()) {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            } else {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        string ext = entry.path().extension().string();
                        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (find(exts.begin(), exts.end(), ext) != exts.end()) {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            }
        }
    }
    return files;
}

char getch() {
    struct termios oldt, newt;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int getTerminalWidth() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

int getTerminalHeight() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
}

Mat loadImage(const string& path) {
    return imread(path, IMREAD_COLOR);
}

void displayImage(const Mat& img, const string& name, int index, int total, int delay, bool paused, bool shuffle, bool asciiMode, int termW, int termH) {
    cout << "\033[H\033[2J";
    int w = img.cols, h = img.rows;
    int maxW = termW - 2;
    int maxH = termH - 8;
    double scaleX = (double)maxW / w;
    double scaleY = (double)maxH / h;
    double scale = min(scaleX, scaleY);
    if (scale > 1.0) scale = 1.0;
    int newW = max(1, (int)(w * scale));
    int newH = max(1, (int)(h * scale));
    Mat resized;
    resize(img, resized, Size(newW, newH), 0, 0, INTER_LANCZOS4);

    string info = BOLD + "📸 " + name + "  |  " + to_string(index+1) + "/" + to_string(total) + "  |  Задержка: " + to_string(delay) + "с" + RESET;
    if (paused) info += " " + YELLOW + "[ПАУЗА]" + RESET;
    if (shuffle) info += " " + BLUE + "[СЛУЧ]" + RESET;
    cout << info << endl;
    cout << "Управление: ← → (нав)  Space (пауза)  +/- (скорость)  s (случ)  f (режим)  r (рестарт)  q (выход)" << endl;

    if (asciiMode) {
        Mat gray;
        cvtColor(resized, gray, COLOR_BGR2GRAY);
        string chars = " .:-=+*#%@";
        for (int r = 0; r < gray.rows; ++r) {
            for (int c = 0; c < gray.cols; ++c) {
                int val = gray.at<uchar>(r, c);
                int idx = val * (chars.size()-1) / 255;
                cout << chars[idx];
            }
            cout << endl;
        }
    } else {
        for (int r = 0; r < resized.rows; ++r) {
            for (int c = 0; c < resized.cols; ++c) {
                Vec3b pixel = resized.at<Vec3b>(r, c);
                cout << rgbToAnsi(pixel[2], pixel[1], pixel[0]) << "██";
            }
            cout << RESET << endl;
        }
    }
}

int main(int argc, char* argv[]) {
    vector<string> paths;
    int delay = 3;
    bool shuffle = false, asciiMode = false, recursive = false;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-d" && i+1 < argc) delay = stoi(argv[++i]);
        else if (arg == "-s") shuffle = true;
        else if (arg == "-a") asciiMode = true;
        else if (arg == "-r") recursive = true;
        else if (arg == "-h" || arg == "--help") {
            cout << "Usage: slideshow <path> [options]\n"
                 << "  -d <sec>   Delay between slides (default 3)\n"
                 << "  -s         Shuffle order\n"
                 << "  -a         ASCII mode\n"
                 << "  -r         Recursive\n";
            return 0;
        } else paths.push_back(arg);
    }

    if (paths.empty()) {
        cerr << "Укажите папку или файлы." << endl;
        return 1;
    }

    vector<string> files = getImageFiles(paths, recursive);
    if (files.empty()) {
        cerr << "Нет изображений." << endl;
        return 1;
    }

    if (shuffle) {
        random_device rd;
        mt19937 g(rd());
        shuffle(files.begin(), files.end(), g);
    }

    int total = files.size();
    int idx = 0;
    bool paused = false;
    int termW = getTerminalWidth();
    int termH = getTerminalHeight();

    while (true) {
        Mat img = loadImage(files[idx]);
        if (img.empty()) {
            cerr << "Ошибка загрузки " << files[idx] << endl;
            idx = (idx + 1) % total;
            continue;
        }
        string name = fs::path(files[idx]).filename().string();
        displayImage(img, name, idx, total, delay, paused, shuffle, asciiMode, termW, termH);

        auto start = chrono::steady_clock::now();
        char key = 0;
        while (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - start).count() < delay && !paused) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            struct timeval tv = {0, 100000};
            if (select(STDIN_FILENO+1, &read_fds, NULL, NULL, &tv) > 0) {
                key = getch();
                break;
            }
        }

        if (key == 'q') break;
        else if (key == ' ') { paused = !paused; }
        else if (key == '+') delay = min(10, delay + 1);
        else if (key == '-') delay = max(1, delay - 1);
        else if (key == 's') {
            shuffle = !shuffle;
            if (shuffle) {
                random_device rd;
                mt19937 g(rd());
                shuffle(files.begin(), files.end(), g);
                idx = 0;
            }
        } else if (key == 'f') asciiMode = !asciiMode;
        else if (key == 'r') idx = 0;
        else if (key == 27) { // ESC
            char seq[2];
            if (getchar() == '[') {
                char c = getchar();
                if (c == 'D') idx = (idx - 1 + total) % total;
                else if (c == 'C') idx = (idx + 1) % total;
            }
        } else if (key == 0 && !paused) {
            idx = (idx + 1) % total;
        }
    }
    cout << GREEN << "\n✅ Слайд-шоу завершено." << RESET << endl;
    return 0;
}
