#pragma once
#include <iostream>
#include "util.hpp"
#include "db.hpp"
#include "onlineManager.hpp"
#include <memory>
#include <vector>

enum roomStatus
{
    GameStart = 0, // 游戏正在进行
    GameFinshed,   // 游戏结束
};

const int kBoardRow = 15;
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
          m_vBoard(kBoardRow, std::vector<int>(kBoardCol, 0))
    {
        INFO_LOG("%lu 房间创建成功!", m_nRoomID);
    }

    ~room()
    {
        INFO_LOG("%lu 房间销毁成功!", m_nRoomID);
    }

    /// @brief 处理玩家的退出 如果是在正常游戏中进行退出的，那么判断对方胜利；如果游戏结束后已经退出的话，就只把房间的玩家数量减一
    /// @param uid 退出的玩家的ID
    void handleExit(uint64_t uid)
    {

        if (m_eStatus != roomStatus::GameFinshed)
        {
            Json::Value JResp;
            JResp["opType"] = "putChess";
            JResp["result"] = true;
            JResp["reason"] = "对方掉线";
            JResp["uid"] = uid;
            JResp["row"] = -1;
            JResp["col"] = -1;
            uint64_t nWinnerId = uid == m_nWhiteID ? m_nBlackID : m_nWhiteID;
            JResp["winner"] = nWinnerId;
            broadCast(JResp);
        }

        m_nPlayerCount--;
    }

    void addWhiteUser(uint64_t uid)
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

    void addBlackUser(uint64_t uid)
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
    /// @brief 处理下棋的请求 该方法由handleRequest 进行调用
    /// @param Jreq 由handleRequest发送过来的请求的Json数据
    /// @return 返回解析好的 JSON 数据
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

        if (nChessRow > kBoardRow && nChessCol > kBoardCol)
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

    Json::Value handleChat(Json::Value &Jreq)
    {
        // 检测房间的ID是否一致
        Json::Value JResp = Jreq;
        uint64_t roomID = JResp["room_id"].asUInt64();

        if (roomID != m_nRoomID)
        {
            JResp["opType"] = "chat";
            JResp["result"] = false;
            JResp["reason"] = "房间号不匹配,当前房间号为" + std::to_string(m_nRoomID) + "传入的房间号为" + std::to_string(roomID);
            return JResp;
        }
        // 关键字屏蔽
        std::string strMsg = Jreq["message"].asString();
        int pos = strMsg.find("敏感词");
        if (pos != std::string::npos)
        {
            std::cout << "敏感词屏蔽:" << strMsg << std::endl;
            JResp["result"] = false;
            JResp["reason"] = "消息中包含敏感词";
            return JResp;
        }

        // 广播消息
        JResp["result"] = true;
        return JResp;
    }

    void handleRequest(Json::Value &Jreq)
    {
        Json::Value JResp = Jreq;
        std::cout << Json_Util::serializeJson(Jreq) << std::endl;

        uint64_t roomID = JResp["room_id"].asUInt64();
        if (roomID != m_nRoomID)
        {
            JResp["opType"] = Jreq["opType"];
            JResp["result"] = false;
            JResp["reason"] = "房间号不匹配,当前房间号为" + std::to_string(m_nRoomID) + "传入的房间号为" + std::to_string(roomID);
            return;
        }

        if (Jreq["opType"] == "putChess")
        {
            JResp = handleChess(Jreq);
            m_eStatus = roomStatus::GameFinshed;
        }
        else if (Jreq["opType"] == "chat")
        {
            JResp = handleChat(Jreq);
        }
        else
        {
            std::cout << "不支持的操作类型:" << Jreq["opType"].asString() << std::endl;
        }

        broadCast(JResp);
    }

    void broadCast(Json::Value &Jrsp)
    {
        // 把传入的信息进行序列化
        std::string strJsonValue = Json_Util::serializeJson(Jrsp);

        // 找到房间里面玩家对应的连接
        webSocketServer::connection_ptr ptrWhiteConnection = m_ptrOnlineManager->getConnectionFromRoom(m_nWhiteID);
        webSocketServer::connection_ptr ptrBlackConnection = m_ptrOnlineManager->getConnectionFromRoom(m_nBlackID);

        // 发送
        if (ptrWhiteConnection)
        {
            ptrWhiteConnection->send(strJsonValue);
        }
        if (ptrBlackConnection)
        {
            ptrBlackConnection->send(strJsonValue);
        }
    }

    void setRoomStatus(roomStatus status)
    {
        m_eStatus = status;
    }

    bool checkFiveChess(int nRow, int nCol, int nRowOffset, int nColOffset)
    {
        // AI: 此函数的cout输出为AI自动补全生成

        int nColor = m_vBoard[nRow][nCol];
        std::cout << "nColor:" << nColor << std::endl;
        int nCount = 1;

        int nCurRow = nRow + nRowOffset;
        int nCurCol = nCol + nColOffset;
        while (nCurRow <= kBoardRow && nCurCol <= kBoardCol &&
               m_vBoard[nRow][nCol] == nColor)
        {
            std::cout << "nCurRow:" << nCurRow << " nCurCol:" << nCurCol << std::endl;
            nCount++;
            nCurRow += nRowOffset;
            nCurCol += nColOffset;
        }

        nCurRow = nRow - nRowOffset;
        nCurCol = nCol - nColOffset;
        while (nCurRow <= kBoardRow && nCurCol <= kBoardCol &&
               m_vBoard[nRow][nCol] == nColor)
        {
            std::cout << "nCurRow:" << nCurRow << " nCurCol:" << nCurCol << std::endl;
            nCount++;
            nCurRow -= nRowOffset;
            nCurCol -= nColOffset;
        }

        std::cout << "nCount:" << nCount << std::endl;
        return nCount >= 5 ? true : false;
    }

    bool isWin(int nRow, int nCol)
    {
        if (checkFiveChess(nRow, nCol, 0, 1) || checkFiveChess(nRow, nCol, 1, 0) ||
            checkFiveChess(nRow, nCol, -1, 1) || checkFiveChess(nRow, nCol, 1, -1))
        {
            return true;
        }
        return false;
    }

private:
    uint64_t m_nRoomID; // 房间的ID

    uint64_t m_nWhiteID; // 白色棋子玩家的ID
    uint64_t m_nBlackID; // 褐色棋子玩家的ID

    size_t m_nPlayerCount;

    roomStatus m_eStatus; // 游戏的状态

    user_table *m_ptrUserTable;         // 用户管理模块
    online_manager *m_ptrOnlineManager; // 用户的在线管理模块

    std::vector<std::vector<int>> m_vBoard; // 棋盘的数据
};

using room_Ptr = std::shared_ptr<room>;

class roomManager
{

public:
    roomManager(user_table *userTable, online_manager *onlineManager)
        : m_ptrUserTable(userTable), m_ptrOnlineManager(onlineManager),
          m_nNextValue(0)
    {
    }

    /// @brief 为在大厅匹配成功的用户创建一个房间
    /// @return
    room_Ptr createRoom(uint64_t nID1, uint64_t nID2)
    {
        // 两个用户都在才创建房间
        if (!(m_ptrOnlineManager->isInGameHall(nID1) && m_ptrOnlineManager->isInGameHall(nID2)))
        {
            INFO_LOG("有用户不在游戏大厅,创建房间失败");
            return nullptr;
        }

        std::unique_lock<std::mutex> lock(m_mMutex);
        uint64_t nRoomID = m_nNextValue++;
        room_Ptr ptrRoom = std::make_shared<room>(nRoomID, m_ptrUserTable, m_ptrOnlineManager);
        ptrRoom->addWhiteUser(nID1);
        ptrRoom->addBlackUser(nID2);

        m_mpRoomMap.insert({nRoomID, ptrRoom});
        m_mpUserMap.insert({nID1, nRoomID});
        m_mpUserMap.insert({nID2, nRoomID});

        return ptrRoom;
    }

    room_Ptr getRoomByRoomID(uint64_t nRoomId)
    {
        std::unique_lock<std::mutex> lock(m_mMutex);
        auto it = m_mpRoomMap.find(nRoomId);
        if (it == m_mpRoomMap.end())
        {
            return nullptr;
        }
        return it->second;
    }

    room_Ptr getRoomByUserID(uint64_t nUserId)
    {
        std::lock_guard<std::mutex> lock(m_mMutex);
        auto it = m_mpUserMap.find(nUserId);
        if (it == m_mpUserMap.end())
        {
            return nullptr;
        }

        // Tip:unique_lock是不可重入锁，所以不能再次直接调用上面的函数
        auto itR = m_mpRoomMap.find(it->second);
        if (itR == m_mpRoomMap.end())
        {
            return nullptr;
        }
        return itR->second;
    }

    /// @brief 删除房间中的指定用户 通常实在用户掉线的时候进行调用 如果用户的数量为0 的话 销毁房间
    /// @param nUserID
    void removeRoomUser(uint64_t nUserID)
    {
        room_Ptr ptrRoom = getRoomByUserID(nUserID); // 这里已经进行加锁
        if(ptrRoom.get() == nullptr)
        {
            return;
        }
        // 处理玩家的退出功能
        ptrRoom->handleExit(nUserID);

        if(ptrRoom->getPlayerCount() ==  0 ) //玩家都退出了就销毁房间
        {
            removeRoom(ptrRoom->getRoomID());
        }
    }

private:
    // 这个函数由其他函数进行调用 在调用前需要先进行锁定
    void removeRoom(uint64_t nRoomID)
    {
        // 房间信息在 unordered_map 里面使用 shared_ptr 进行管理
        // 所以在没有其他房间占用这个 shared_ptr 的情况下，这个房间会被自动析构，所以我们不需要进行释放
        auto it = m_mpRoomMap.find(nRoomID);
        if (it == m_mpRoomMap.end())
        {
            return;
        }

        // 先找到房间里面的用户 然后从房间里面删除这里面的用户
        m_mpUserMap.erase(it->second->getBlackID());
        m_mpUserMap.erase(it->second->getWhiteID());
        m_mpRoomMap.erase(nRoomID);
    }

private:
    user_table *m_ptrUserTable;
    online_manager *m_ptrOnlineManager;

    uint64_t m_nNextValue; // 为了保护计数器需要上锁
    std::mutex m_mMutex;

    std::unordered_map<uint64_t, room_Ptr> m_mpRoomMap;
    std::unordered_map<uint64_t, uint64_t> m_mpUserMap;
};