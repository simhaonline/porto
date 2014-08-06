#include "file.hpp"
#include "log.hpp"

#include <fstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

TFile::TFile(std::string path) : path(path) {
};

string TFile::Path() {
    return path;
}

TFile::EFileType TFile::Type() {
    struct stat st;

    if (lstat(path.c_str(), &st))
        throw "Cannot stat: " + path;

    if (S_ISREG(st.st_mode))
        return Regular;
    else if (S_ISDIR(st.st_mode))
        return Directory;
    else if (S_ISCHR(st.st_mode))
        return Character;
    else if (S_ISBLK(st.st_mode))
        return Block;
    else if (S_ISFIFO(st.st_mode))
        return Fifo;
    else if (S_ISLNK(st.st_mode))
        return Link;
    else if (S_ISSOCK(st.st_mode))
        return Socket;
    else
        return Unknown;
}
    
TError TFile::Remove() {
    int ret = unlink(path.c_str());
    TLogger::LogAction("unlink " + path, ret, errno);

    if (ret && (errno != ENOENT))
        return TError(errno);
    return TError();
}

string TFile::AsString() {
    ifstream in(Path());
    if (!in.is_open())
        throw "Cannot open " + Path();

    string ret;
    in >> ret;

    return ret;
}

int TFile::AsInt() {
    ifstream in(Path());
    if (!in.is_open())
        return 0;

    string s;
    in >> s;

    try {
        return stoi(s);
    } catch (...) {
        return 0;
    }
}

vector<string> TFile::AsLines() {
    ifstream in(path);
    string line;

    if (!in.is_open())
        throw "Cannot open " + path;

    vector<string> ret;
    while (getline(in, line))
        ret.push_back(line);

    return ret;
}

TError TFile::WriteStringNoAppend(string str)
{
    ofstream out(path, ofstream::trunc);
    if (out.is_open()) {
        out << str;
        TLogger::LogAction("write " + path, 0, 0);
        return TError();
    } else {
        TLogger::LogAction("write " + path, -1, errno);
        return TError(errno);
    }
}

TError TFile::AppendString(string str)
{
    ofstream out(path, ofstream::out);
    if (out.is_open()) {
        out << str;
        TLogger::LogAction("append " + path, 0, 0);
        return TError();
    } else {
        TLogger::LogAction("append " + path, -1, errno);
        return TError(errno);
    }
}
