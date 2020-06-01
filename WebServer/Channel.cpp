#include "Channel.h"
#include "NetHelper.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timeb.h>

//EventLoop *loop_;
//int fd_;
//int events_;
//// return events -> decide return events
//int rEvents_;
//int lastEvents_;
//// some callback
//CallbackType readCb_;
//CallbackType writeCb_;
//CallbackType updateCb_;

const int Channel::kBufSize = 1024;

Channel::Channel(EventLoop *loop, int fd):
loop_(loop),
fd_(fd),
events_(0),
rEvents_(0),
lastEvents_(0) {
}

Channel::~Channel() {
    close(fd_);
}

bool Channel::UpdateLastEvents() {
    auto res = (lastEvents_ == events_);
    lastEvents_ = events_;
    return !res;
}

void Channel::HandleEvents() {
    // handle events
    if ((rEvents_ & EPOLLIN) && readCb_) {
        readCb_();
    }
    if ((rEvents_ & EPOLLOUT) && writeCb_) {
        writeCb_();
    }
    if (updateCb_)
        updateCb_();
}

void Channel::TestNewEvent(ChannelPtr channel) {
    channel->SetReadCallback(std::bind(&Channel::TestRead, this, channel));
    channel->SetUpdateCallback(std::bind(&Channel::TestUpdate, this, channel));
    channel->SetEvents(EPOLLIN | EPOLLET);
    loop_->AddToPoller(channel);
}

void Channel::TestRead(ChannelPtr channel) {
    int len;
    char buff[kBufSize];
    len = NetHelper::GetLine(fd_, buff, kBufSize);
    if (!len) {
        // TODO: Add Log
        std::cout << "Net First GetLine Failed";
        return;
    }
    std::string firstLine(buff);
    std::cout << "FirstLine:" << firstLine << "\n";
    while (len) {
        len = NetHelper::GetLine(fd_, buff, kBufSize);
    }
    // parse request
//
//    struct timeb tp;
//    ftime(&tp);
//    time_t now = tp.time;
//    tm time;
//    localtime_r(&now, &time);
//    char timeString[64];
//    memset(timeString, '\0', sizeof(now));
//    snprintf(timeString, sizeof(timeString), "%04d-%02d-%02d %02d:%02d:%02d:%03d", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec, tp.millitm);
//    firstLine = "<h1>" + firstLine;
//    firstLine += "</h1>\r\n<hr>\r\n";
//    std::string nowString(timeString);
//    firstLine += nowString;
//    std::string header;
//    header += "HTTP/1.1 200 OK\r\n";
////    header += "Content-Type: image/png\r\n";
////    header += "Content-Length: " + std::to_string(sizeof(favicon)) + "\r\n";
//    header += "Content-Type: text/html\r\n";
//    header += "Content-Length: " + std::to_string(firstLine.length()) + "\r\n";
//    header += "Server: CZK's Web Server\r\n";
//
//    header += "\r\n";
//    header += firstLine;
//    NetHelper::WriteN(fd_, header);
    if (!strncasecmp("get", firstLine.data(), 3)) {
        // Do Request -> DisConnect
        NetHelper::HttpRequest(firstLine.data(), fd_);
        // request Do -> Close() -> channel -> (Delete)
        // http don't keep alive
//        disconnect(cfd, epfd);
    }
}

void Channel::TestUpdate(ChannelPtr channel) {
    loop_->RemovePoller(channel);
//    loop_->UpdatePoller(channel);
}


