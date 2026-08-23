#pragma once
#include <iostream>
#include "util.hpp"
#include "db.hpp"
#include "onlineManager.hpp"
#include <vector>

enum roomStatus
{
    GameStart = 0, // 游戏正在进行
    GameFinshed,   // 游戏结束
};

const int kBoardRaw = 15;
const int kBoardCol = 15;

const int kWhiteChess = 1;
const int kBlackChess = 2;

class room
{

public:
    room(uint16_t nRoomID, user_table *ptrUserTable, online_manager *ptrOnlineManager)
        : m_nWhiteID(0), m_nBlackID(0),
          m_nRoomID(nRoomID), m_eStatus(roomStatus::GameStart), m_nPlayerCount(0),
          m_ptrUserTable(ptrUserTable),
          m_ptrOnlineManager(ptrOnlineManager),
          m_vBoard(kBoardRaw, std::vector<int>(kBoardCol, 0))
    {
        INFO_LOG("%lu 房间创建成功!", m_nRoomID);
    }

    ~room()
    {
        INFO_LOG("%lu 房间销毁成功!", m_nRoomID);
    }

    Json::Value handleChess(Json::Value &Jreq)
    {
        Json::Value resp;
        // 1. 判断房间号的信息是不是这个房间号
        uint64_t roomID = resp["room_id"].asUInt64();

        if (roomID != m_nRoomID)
        {
            resp["opType"] = "putChess";
            resp["result"] = false;
            resp["reason"] = "房间号不匹配,当前房间号为" + std::to_string(m_nRoomID) + "传入的房间号为" + std::to_string(roomID);
            return resp;
        }
        resp["room_id"] = roomID;
        
        // 2. 判断是否有玩家掉线 ,需要下棋位置的回显 所以需要传回来下棋的位置
        int nChessRow = Jreq["row"].asInt64();
        int nChessCol = Jreq["col"].asInt64();
        if (!m_ptrOnlineManager->isInGameRoom(m_nWhiteID))
        {
            resp["opType"] = "putChess";
            resp["result"] = true;
            resp["reason"] = "白方掉线,黑方获胜!!!";
            resp["row"] = nChessRow;
            resp["col"] = nChessCol;
            resp["winner"] = m_nBlackID;
            return resp;
        }
        else if (!m_ptrOnlineManager->isInGameRoom(m_nWhiteID))
        {
            resp["opType"] = "putChess";
            resp["result"] = true;
            resp["reason"] = "黑掉线,白方获胜!!!";
            resp["row"] = nChessRow;
            resp["col"] = nChessCol;
            resp["winner"] = m_nWhiteID;
            return resp;
        }
        // 3. 判断下棋的位置是否合法

        if (nChessRow > kBoardRaw && nChessCol > kBoardCol)
        {
            resp["opType"] = "putChess";
            resp["result"] = false;
            resp["reason"] = "下棋的位置不合法,棋盘的范围是0~14";
            return resp;
        }

        if (m_vBoard[nChessRow][nChessCol] != 0)
        {
            resp["opType"] = "putChess";
            resp["result"] = false;
            resp["reason"] = "此位置已经下过棋 颜色为" + (m_vBoard[nChessRow][nChessCol] == kWhiteChess) ? "白色" : "黑色";
            return resp;
        }

        // 4. 下棋之后是否有获胜的玩家

        uint64_t uid = Jreq["uid"].asInt64();
        m_vBoard[nChessRow][nChessCol] = uid == m_nWhiteID ? kWhiteChess : kBlackChess;

        if (isWin(nChessRow, nChessCol))
        {

            std::string strWinner;
            uint64_t nWinnerId;
            uint64_t nLoserId;
            if (uid == m_nWhiteID)
            {
                strWinner = "白色";
                nWinnerId = m_nWhiteID;
                nLoserId = m_nBlackID;
            }
            else
            {
                strWinner = "黑色";
                nWinnerId = m_nBlackID;
                nLoserId = m_nWhiteID;
            }
            // 更新数据库的信息
            m_ptrUserTable->win(nWinnerId);
            m_ptrUserTable->lose(nLoserId);

            // 返回客户端
            resp["opType"] = "putChess";
            resp["result"] = true;
            resp["reason"] = strWinner + "获胜";
            resp["row"] = nChessRow;
            resp["col"] = nChessCol;
            resp["winner"] = uid;
            return resp;
        }
        else
        {
            resp["opType"] = "putChess";
            resp["result"] = true;
            resp["row"] = nChessRow;
            resp["col"] = nChessCol;
            resp["winner"] = 0;
            return resp;
        }
    }

    Json::Value handleChat(Json::Value &Jreq);
    Json::Value handleExit(uint64_t uid);
    Json::Value handleRequest(Json::Value &Jreq);

    void broadCast(Json::Value &Jrsp);

    void setRoomStatus(roomStatus status)
    {
        m_eStatus = status;
    }

    void addWhiteUser(uint16_t uid)
    {
        if (m_nWhiteID == 0)
        {
            m_nWhiteID = uid;
            ++m_nPlayerCount;
            INFO_LOG("成功创建ID为%lu的白玩家", m_nBlackID);
        }
        else
        {
            ERR_LOG("白棋玩家已经创建ID:%lu , ID为%lu的玩家创建失败", m_nWhiteID, uid);
        }
    }

    void addBlackUser(uint16_t uid)
    {
        if (m_nBlackID == 0)
        {
            m_nBlackID = uid;
            ++m_nPlayerCount;
            INFO_LOG("成功创建ID为%lu的黑棋玩家", m_nBlackID);
        }
        else
        {
            ERR_LOG("黑棋玩家已经创建ID:%lu , ID为%lu的玩家创建失败", m_nBlackID, uid);
        }
    }

    // AI:get函数的生成使用了AI自动补全
    uint16_t getRoomID() const
    {
        return m_nRoomID;
    }

    uint16_t getWhiteID() const
    {
        return m_nWhiteID;
    }

    uint16_t getBlackID() const
    {
        return m_nBlackID;
    }

    size_t getPlayerCount() const
    {
        return m_nPlayerCount;
    }

    roomStatus getRoomStatus() const
    {
        return m_eStatus;
    }

    std::vector<std::vector<int>> getBoard() const
    {
        return m_vBoard;
    }

private:
    bool isWin(int nRow, int nCol)
    {
    }

private:
    uint16_t m_nRoomID; // 房间的ID

    uint16_t m_nWhiteID; // 白色棋子玩家的ID
    uint16_t m_nBlackID; // 褐色棋子玩家的ID

    size_t m_nPlayerCount;

    roomStatus m_eStatus; // 游戏的状态

    user_table *m_ptrUserTable;         // 用户管理模块
    online_manager *m_ptrOnlineManager; // 用户的在线管理模块

    std::vector<std::vector<int>> m_vBoard; // 棋盘的数据
};