#pragma once
#include "db.hpp"
#include "iostream"
#include "logger.hpp"
#include "matcher.hpp"
#include "onlineManager.hpp"
#include "room.hpp"
#include "session.hpp"
#include "util.hpp"
#include <jsoncpp/json/value.h>

const std::string kWWWRoot = "./wwwroot";
const int kSessionTimeOut = 300000;

class gobangSever
{
public:
  gobangSever()
      : m_UserTable(), m_OnlineManager(), m_cScoketSever(),
        m_SessionManager(&m_cScoketSever),
        m_RoomManager(&m_UserTable, &m_OnlineManager),
        m_Matcher(&m_RoomManager, &m_UserTable, &m_OnlineManager),
        m_strWebRoot(kWWWRoot)
  {
    m_cScoketSever.set_access_channels(websocketpp::log::alevel::none);

    m_cScoketSever.init_asio();

    m_cScoketSever.set_reuse_addr(true);

    m_cScoketSever.set_open_handler(
        std::bind(&gobangSever::opencallback, this, std::placeholders::_1));
    m_cScoketSever.set_close_handler(
        std::bind(&gobangSever::closecallback, this, std::placeholders::_1));
    m_cScoketSever.set_http_handler(
        std::bind(&gobangSever::httpcallback, this, std::placeholders::_1));
    m_cScoketSever.set_message_handler(std::bind(&gobangSever::messagecallback,
                                                 this, std::placeholders::_1,
                                                 std::placeholders::_2));
  }
  ~gobangSever() {}

  void start(int port) // 启动服务器
  {
    // 监听端口
    m_cScoketSever.listen(port);

    // 开始获取新链接
    m_cScoketSever.start_accept();

    // 启动服务器
    m_cScoketSever.run(); // 内部是一个无限循环，阻塞在这里，直到服务器关闭
  }

private:
  session_ptr getSessionByCookie(webSocketServer::connection_ptr ptrConnection)
  {
    std::string strCookie = ptrConnection->get_request_header("Cookie");
    if (strCookie.empty())
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "请求没有Cookie,请重新登陆");
      return nullptr;
    }
    // 然后在Session里面找到用户相关的信息 如果找不到对应的Session的话
    // 说明用户的登录已经过期了 需要重新登录
    std::string strSSID = getValueFromCookie(strCookie, "SSID");

    if (strSSID.empty())
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "Cookie没有SSID,请重新登陆");
      return nullptr;
    }

    return m_SessionManager.getSession(std::stoul(strSSID));
  }
  // 处理静态的资源请求
  void staticFileHandler(webSocketServer::connection_ptr ptrConnection)
  {
    // 获取到用户的请求资源路径
    // 如果用户请求到的是一个空的目录 或者其他无效的资源文件的话
    // 那么就返回login.html

    auto req = ptrConnection->get_request();
    std::string strUri = req.get_uri();

    std::string strFileName = m_strWebRoot + strUri;
    // INFO_LOG("staticFileHandler, file name: %s", strFileName.c_str());
    if (strFileName.back() == '/') // 请求的是一个目录
    {
      strFileName += "login.html";
    }

    std::string strBody = file_Util::readFile(strFileName);
    // INFO_LOG("staticFileHandler read file size: %ld", strBody.size());

    if (strBody.size() == 0)
    {
      // ERR_LOG("staticFileHandler read file failed, file name: %s",
      // strFileName.c_str());
      ptrConnection->set_status(websocketpp::http::status_code::not_found);
    }
    else
    {
      ptrConnection->set_body(strBody);
      ptrConnection->set_status(websocketpp::http::status_code::ok);
      ptrConnection->append_header("Content-Length",
                                   std::to_string(strBody.size()));
      // 根据文件扩展名设置正确的 Content-Type
      std::string strContentType = "text/html";
      std::string strExt = strFileName.substr(strFileName.find_last_of('.') + 1);
      if (strExt == "css")
        strContentType = "text/css";
      else if (strExt == "js")
        strContentType = "application/javascript";
      else if (strExt == "jpeg" || strExt == "jpg")
        strContentType = "image/jpeg";
      else if (strExt == "png")
        strContentType = "image/png";
      else if (strExt == "gif")
        strContentType = "image/gif";
      else if (strExt == "ico")
        strContentType = "image/x-icon";
      else if (strExt == "json")
        strContentType = "application/json";
      ptrConnection->append_header("Content-Type", strContentType);
    }
  }

  void handleHttpResp(webSocketServer::connection_ptr ptrConnection,
                      bool result, websocketpp::http::status_code::value code,
                      const std::string &strReason)
  {
    Json::Value JResp;
    JResp["result"] = result;
    JResp["reason"] = strReason;
    std::string body = Json_Util::serializeJson(JResp);
    ptrConnection->set_body(body);
    ptrConnection->set_status(code);
    ptrConnection->append_header("Content-Length", std::to_string(body.size()));
    ptrConnection->append_header("Content-Type", "application/json");
  }

  // 处理用户注册功能
  void registerUser(webSocketServer::connection_ptr ptrConnection)
  {
    auto strBody = ptrConnection->get_request_body();
    Json::Value JRet = Json_Util::deserializeJson(strBody);

    if (JRet.empty())
    {
      ptrConnection->set_status(websocketpp::http::status_code::bad_request);
    }
    if (JRet["username"].isNull() || JRet["password"].isNull())
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "用户名或密码不能为空");
      return;
    }

    if (!m_UserTable.insertUser(JRet))
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "用户名被占用");
      return;
    }

    handleHttpResp(ptrConnection, true, websocketpp::http::status_code::ok,
                   "注册用户成功");
    return;
  }

  // 处理用户登录功能 使用Set-Cookie 把 SessionID 通过 cookie 返回。
  void loginUser(webSocketServer::connection_ptr ptrConnection)
  {
    auto strBody = ptrConnection->get_request_body();
    Json::Value JRet = Json_Util::deserializeJson(strBody);

    if (JRet.empty())
    {
      ptrConnection->set_status(websocketpp::http::status_code::bad_request);
      return;
    }
    if (JRet["username"].isNull() || JRet["password"].isNull())
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "用户名或密码不能为空");
      return;
    }

    if (!m_UserTable.loginUser(JRet))
    {
      DBG_LOG("用户名或密码错误");
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "用户名或密码错误");
      return;
    }

    // 验证成功 创建Session
    session_ptr ptrSession = m_SessionManager.createSession();
    if (!ptrSession)
    {
      DBG_LOG("Session创建失败");
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::internal_server_error,
                     "Session创建失败");
    }
    ptrSession->setUser(JRet["id"].asUInt64());

    std::string strSession =
        "SSID=" + std::to_string(ptrSession->getSessionID());
    ptrConnection->append_header("Set-Cookie", strSession);

    handleHttpResp(ptrConnection, true, websocketpp::http::status_code::ok,
                   "Session创建成功");
  }

  std::string getValueFromCookie(const std::string &strCookie,
                                 const std::string &strKey)
  {
    // Cookie的组织使用 "; " 一个分号加上一个空格进行组织的
    std::vector<std::string> JRet = string_Util::split(strCookie, "; ");

    for (auto str : JRet)
    {
      std::vector<std::string> strSubString = string_Util::split(str, "=");
      if (strSubString.size() != 2) // 不是Key Value 形式 跳过
      {
        continue;
      }
      if (strSubString[0] == strKey)
      {
        return strSubString[1];
      }
    }
    return std::string();
  }

  // 获取用户信息 在进入游戏大厅加载用户信息时调用
  void getUserInfo(webSocketServer::connection_ptr ptrConnection)
  {
    session_ptr ptrSession = getSessionByCookie(ptrConnection);

    if (ptrSession.get() == nullptr)
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "登陆过期, 请重新登陆");
      return;
    }

    // 从数据库找到用户的信息
    uint64_t uid = ptrSession->getUser();

    Json::Value JUserINfo;
    if (!m_UserTable.selectById(uid, JUserINfo))
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "找不到用户信息, 请重新登陆");
    }

    std::string strJson = Json_Util::serializeJson(JUserINfo);

    ptrConnection->set_body(strJson);
    ptrConnection->set_status(websocketpp::http::status_code::ok);
    ptrConnection->append_header("Content-Type", "application/json");

    m_SessionManager.setSessionExpireTime(ptrSession->getSessionID(),
                                          kSessionTimeOut);
  }

  void openGameHall(webSocketServer::connection_ptr ptrConnection)
  {
    // 游戏大厅长连接建立成功

    // 登录验证 如果登录失败的话 重新登陆
    Json::Value JErrResp;
    std::string strCookie = ptrConnection->get_request_header("Cookie");
    if (strCookie.empty())
    {
      JErrResp["optype"] = "hall_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "请求没有Cookie,请重新登陆";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));
      return;
    }
    std::string strSSID = getValueFromCookie(strCookie, "SSID");

    if (strSSID.empty())
    {
      JErrResp["optype"] = "hall_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "Cookie没有SSID,请重新登陆";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));
      return;
    }

    session_ptr ptrSession = m_SessionManager.getSession(std::stoul(strSSID));

    if (ptrSession.get() == nullptr)
    {
      JErrResp["optype"] = "hall_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "登陆过期, 请重新登陆";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));
      return;
    }

    uint64_t uid = ptrSession->getUser();
    // 判断是不是重复登录 查看是不是两个设备进行登录
    if (m_OnlineManager.isInGameHall(uid) ||
        m_OnlineManager.isInGameRoom(uid))
    {
      JErrResp["optype"] = "hall_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "重复登录, 请重新登陆";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));
    }

    // 把当前客户端和连接加入到对应的大厅
    m_OnlineManager.enterGameHall(uid, ptrConnection);

    // 给客户端响应游戏大厅建立成功"hall_ready"
    Json::Value JResp;
    JResp["optype"] = "hall_ready";
    JResp["result"] = true;
    JResp["reason"] = "登陆成功";
    ptrConnection->send(Json_Util::serializeJson(JResp));

    // 设置Session设置为永久存在
    m_SessionManager.setSessionExpireTime(ptrSession->getSessionID(), -1);
  }

  void openGameRoom(webSocketServer::connection_ptr ptrConnection)
  {
    // 从游戏大厅退出，进入到游戏房间 此时游戏大厅的长连接会被关闭，需要手动移除删除的Session的逻辑
    session_ptr ptrSession = getSessionByCookie(ptrConnection);

    if (ptrSession.get() == nullptr)
    {
      return;
    }

    // 获取Session 检查是不是在其他的房间或者在大厅里面
    uint64_t uid = ptrSession->getUser();
    Json::Value JErrResp;
    if (m_OnlineManager.isInGameHall(uid) || m_OnlineManager.isInGameRoom(uid))
    {
      JErrResp["optype"] = "room_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "用户重复登录";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));

      return;
    }
    // 判断是不是已经为用户创建了房间 这个房间是在用户匹配完成之后生成的
    room_Ptr ptrRoom = m_RoomManager.getRoomByUserID(uid);
    if (ptrRoom.get() == nullptr)
    {
      JErrResp["optype"] = "room_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "没有找到玩家的房间信息";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));
      return;
    }

    // 把用户设置到在线用户管理的游戏房间中

    m_OnlineManager.enterGameRoom(uid, ptrConnection);

    // 重置 session
    m_SessionManager.setSessionExpireTime(ptrSession->getSessionID(), -1);

    Json::Value JSuccessResp;
    JSuccessResp["optype"] = "room_ready";
    JSuccessResp["result"] = true;
    JSuccessResp["reason"] = "房间准备完毕";
    JSuccessResp["room_id"] = ptrRoom->getRoomID();
    JSuccessResp["uid"] = ptrSession->getUser();
    JSuccessResp["white_id"] = ptrRoom->getWhiteID();
    JSuccessResp["black_id"] = ptrRoom->getBlackID();

    ptrConnection->send(Json_Util::serializeJson(JSuccessResp));
    return;
  }

  void opencallback(websocketpp::connection_hdl hdl) // 建立WebSocket长连接的回调函数
  {
    // 我们可以根据URi 来判断这个是游戏大厅的长连接请求还是游戏房间的长连接请求
    auto prtConnection = m_cScoketSever.get_con_from_hdl(hdl);
    auto request = prtConnection->get_request();
    std::string strUri = request.get_uri();
    INFO_LOG("opencallback, uri: %s", strUri.c_str());

    if (strUri == "/hall") // 游戏大厅的长连接
    {
      openGameHall(prtConnection);
    }
    else if (strUri == "/room") // 游戏房间的长连接
    {
      openGameRoom(prtConnection);
    }
  }

  // 游戏大厅长连接断开
  void closeGameHall(webSocketServer::connection_ptr ptrConnection)
  {
    session_ptr ptrSession = getSessionByCookie(ptrConnection);

    if (ptrSession.get() == nullptr)
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "登陆过期, 请重新登陆");
    }

    // 从数据库找到用户的信息
    uint64_t uid = ptrSession->getUser();

    m_OnlineManager.exitGameHall(uid);

    // session 恢复生命周期的管理

    m_SessionManager.setSessionExpireTime(ptrSession->getSessionID(), kSessionTimeOut);
  }

  void closeGameRoom(webSocketServer::connection_ptr ptrConnection)
  {

    auto ptrSession = getSessionByCookie(ptrConnection);
    if (!ptrSession)
    {
      return;
    }
    // 从用户管理器中删除玩家的信息
    m_OnlineManager.exitGameRoom(ptrSession->getUser());
    // 更新 session 的过期时间
    m_SessionManager.setSessionExpireTime(ptrSession->getSessionID(), kSessionTimeOut);
    // 从游戏房间管理器中删除用户信息 当所有玩家都退出后，房间会被销毁
    m_RoomManager.removeRoomUser(ptrSession->getUser());
  }

  void closecallback(websocketpp::connection_hdl hdl) // webSocket连接断开的处理
  {
    // 区分是游戏游戏房间断开还是游戏大厅断开
    auto prtConnection = m_cScoketSever.get_con_from_hdl(hdl);
    auto request = prtConnection->get_request();

    std::string strUri = request.get_uri();
    INFO_LOG("连接断开, uri: %s", strUri.c_str());

    if (strUri == "/hall") // 游戏大厅的长连接
    {
      closeGameHall(prtConnection);
    }
    else if (strUri == "/room") // 游戏房间的长连接
    {
      closeGameRoom(prtConnection);
    }
  }

  void messageGameHall(webSocketServer::connection_ptr ptrConnection, webSocketServer::message_ptr msg)
  {
    Json::Value JErrResp;
    std::string strCookie = ptrConnection->get_request_header("Cookie");
    if (strCookie.empty())
    {
      JErrResp["optype"] = "hall_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "请求没有Cookie,请重新登陆";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));
      return;
    }
    std::string strSSID = getValueFromCookie(strCookie, "SSID");

    if (strSSID.empty())
    {
      JErrResp["optype"] = "hall_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "Cookie没有SSID,请重新登陆";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));
      return;
    }

    session_ptr ptrSession = m_SessionManager.getSession(std::stoul(strSSID));
    if (!ptrSession)
    {
      JErrResp["optype"] = "hall_ready";
      JErrResp["result"] = false;
      JErrResp["reason"] = "Session失效请重新登录";
      ptrConnection->send(Json_Util::serializeJson(JErrResp));
      return;
    }

    std::string strBody = msg->get_payload();

    Json::Value JBody = Json_Util::deserializeJson(strBody);
    // 获取请求
    if (JBody.empty())
    {
      ERR_LOG("messageGameHall, json parse failed, body: %s", strBody.c_str());
      JBody["result"] = false;
      JBody["reason"] = "请求信息解析失败";
      ptrConnection->send(Json_Util::serializeJson(JBody));
      return;
    }

    // 完成请求的处理 对战匹配 停止对战匹配

    if (JBody["optype"].isNull())
    {
    }

    uint64_t uid = ptrSession->getUser();
    if (JBody["optype"].asString() == "match_start")
    {
      // 开始匹配
      m_Matcher.addUser(uid);
      JBody["result"] = true;
      ptrConnection->send(Json_Util::serializeJson(JBody));
    }
    else if (JBody["optype"].asString() == "match_stop")
    {
      // 停止匹配
      m_Matcher.removeUser(uid);
      JBody["result"] = true;
      ptrConnection->send(Json_Util::serializeJson(JBody));
    }
    else
    {
      m_Matcher.removeUser(uid);
      JBody["optype"] = "unknown";
      JBody["result"] = false;
      ptrConnection->send(Json_Util::serializeJson(JBody));
    }
  }

  void messageGameRoom(webSocketServer::connection_ptr ptrConnection, webSocketServer::message_ptr msg)
  {
    auto ptrSession = getSessionByCookie(ptrConnection);
    if (!ptrSession)
    {
      return;
    }
    auto uid = ptrSession->getUser();
    room_Ptr ptrRoom = m_RoomManager.getRoomByUserID(uid);

    if (!ptrRoom)
    {
      return;
    }

    auto JReqBody = Json_Util::deserializeJson(msg->get_payload());

    ptrRoom->handleRequest(JReqBody);
  }
  //
  void messagecallback(websocketpp::connection_hdl hdl,
                       webSocketServer::message_ptr msg)
  {
    auto prtConnection = m_cScoketSever.get_con_from_hdl(hdl);
    auto request = prtConnection->get_request();

    std::string strUri = request.get_uri();
    INFO_LOG("长连接建立, uri: %s", strUri.c_str());

    if (strUri == "/hall") // 游戏大厅的长连接
    {
      messageGameHall(prtConnection, msg);
    }
    else if (strUri == "/room") // 游戏房间的长连接
    {
      messageGameRoom(prtConnection, msg);
    }
  }

  void httpcallback(websocketpp::connection_hdl hdl)
  {
    auto ptrConnetion = m_cScoketSever.get_con_from_hdl(hdl);
    auto req = ptrConnetion->get_request();
    auto strUri = req.get_uri();
    auto strMethod = req.get_method();

    // INFO_LOG("httpcallback, method: %s, uri: %s", strMethod.c_str(),
    //          strUri.c_str());

    if (strMethod == "POST" && strUri == "/reg")
    {
      return registerUser(ptrConnetion);
    }
    else if (strMethod == "POST" && strUri == "/login")
    {
      return loginUser(ptrConnetion);
    }
    else if (strMethod == "GET" && strUri == "/info")
    {
      return getUserInfo(ptrConnetion);
    }
    else
    {
      return staticFileHandler(ptrConnetion);
    }
  }

private:
  user_table m_UserTable;
  online_manager m_OnlineManager;
  webSocketServer m_cScoketSever;
  sessionManager m_SessionManager;
  roomManager m_RoomManager;
  matcher m_Matcher;

  std::string m_strWebRoot; // 静态资源根目录 需要指定一个路径
};
