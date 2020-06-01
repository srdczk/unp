#ifndef WEBSERVER_CHANNEL_H
#define WEBSERVER_CHANNEL_H

#include "EventLoop.h"

class Channel {
public:
    typedef std::shared_ptr<Channel> ChannelPtr;
    typedef std::function<void()> CallbackType;
    Channel(EventLoop *loop, int fd);

    // channel -> release
    ~Channel();

    // not copy able
    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;

    // getter and setter for fd and events
    int Fd() { return fd_; }
    int GetEvents() { return events_; }
    int GetREvents() { return rEvents_; }
    int GetLastEvents() { return lastEvents_; }

    void SetEvents(int events) { events_ = events; }
    void SetREvents(int rEvents) { rEvents_ = rEvents; }
    void SetLastEvents(int lastEvents) { lastEvents_ = lastEvents; }

    bool UpdateLastEvents();

    void SetReadCallback(CallbackType readCb) { readCb_ = readCb; }
    void SetWriteCallback(CallbackType writeCb) { writeCb_ = writeCb; }
    void SetUpdateCallback(CallbackType updateCb) { updateCb_ = updateCb; }

    void HandleEvents();

    void TestRead(ChannelPtr channel);

    void TestUpdate(ChannelPtr channel);

    void TestNewEvent(ChannelPtr channel);
private:
    EventLoop *loop_;
    int fd_;
    int events_;
    // return events -> decide return events
    int rEvents_;
    int lastEvents_;
    // some callback
    CallbackType readCb_;
    CallbackType writeCb_;
    CallbackType updateCb_;
    // if is http channel
//    bool isHttp_;
};


#endif //WEBSERVER_CHANNEL_H
