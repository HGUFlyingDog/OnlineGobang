#pragma once
#include <iostream>
#include <memory>
#include "onlineManager.hpp"

enum sessionStatus
{
    UnLogin, // 1. 没有登录。
    Login    // 2. 已经登录。
};

class session
{
public:
    session(uint64_t SessionID);
    ~session();

    void setUser();
    uint64_t getUser();
    bool isLogin();
    void setTimer(const webSocketServer::timer_ptr & timePtr);
    webSocketServer::timer_ptr getTimer();

private:
    uint64_t m_nSessionID;                 // 这个Session的ID
    uint64_t m_nUserID;                    // 这个 Session 是归属于哪个用户的
    sessionStatus m_eSessionStatus;        // 这个 session 的状态，现在简单初始化为已经登录
    webSocketServer::timer_ptr m_ptrTimer; // session 关联的定时器 通过这个进行定时任务的取消、重置
};