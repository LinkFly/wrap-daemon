#ifndef REDIRECTOUTPUT_H
#define REDIRECTOUTPUT_H

#include <sstream>
//#include <cstdio>
#include <string>
#include <iostream>

#include <syslog.h>

using std::string;

class LogStreamShared : public std::stringbuf {
protected:
    void write_log(const string& msg) {
        syslog(9, msg.c_str());
    }
    int sync_base(const char* prefix)
    {
        auto s = str();
        if (s.size() > 0 && s != "\n") {
            std::string msg(prefix);
            //syslog(9, prefix);
            //syslog(9, s.c_str());
            msg += s;
            write_log(msg);
            str("");
            return std::stringbuf::sync();
        } else if (s == "\n") {
            str("");
        }
    }
};
class LogStreamCout : public LogStreamShared
{
public:
    static void bind() {
        static LogStreamCout logStream;
        std::cout.rdbuf(&logStream);
    }
    int sync()
    {
        return sync_base("info: ");
    }

};

class LogStreamCerr : public LogStreamShared
{
public:
    static void bind() {
        static LogStreamCerr logStream;
        std::cerr.rdbuf(&logStream);
    }
protected:
    int sync()
    {
        return sync_base("error: ");
    }
};

#endif // REDIRECTOUTPUT_H
