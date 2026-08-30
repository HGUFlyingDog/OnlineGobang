#pragma once
#include "db.hpp"
#include "iostream"
#include "matcher.hpp"
#include "onlineManager.hpp"
#include "room.hpp"
#include "session.hpp"
#include <jsoncpp/json/value.h>

const std::string kWWWRoot = "./wwwroot";
const int kSessionTimeOut = 30000;

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
      ptrConnection->append_header("Content-Type", "text/html");
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

  // 获取用户信息
  void getUserInfo(webSocketServer::connection_ptr ptrConnection)
  {

    // 获取用户信息前需要先校验用户的状态
    // 从用户的Cookie获取相关的Session 如果没有说明用户没有登录 让客户端重新登陆
    std::string strCookie = ptrConnection->get_request_header("Cookie");
    if (strCookie.empty())
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "请求没有Cookie,请重新登陆");
      return;
    }
    // 然后在Session里面找到用户相关的信息 如果找不到对应的Session的话
    // 说明用户的登录已经过期了 需要重新登录
    std::string strSSID = getValueFromCookie(strCookie, "SSID");

    if (strSSID.empty())
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "Cookie没有SSID,请重新登陆");
      return;
    }
    // 找到 用户信息之后 序列化返回给客户端 刷新用户的Session的过期时间

    session_ptr ptrSession = m_SessionManager.getSession(std::stoul(strSSID));

    if (ptrSession.get() == nullptr)
    {
      handleHttpResp(ptrConnection, false,
                     websocketpp::http::status_code::bad_request,
                     "登陆过期, 请重新登陆");
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

  void
  opencallback(websocketpp::connection_hdl hdl) // 建立WebSocket长连接的回调函数
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
    }
  }

  void closecallback(websocketpp::connection_hdl hdl)
  {
    std::cout << "Connection closed" << std::endl;
  }

  void messagecallback(websocketpp::connection_hdl hdl,
                       webSocketServer::message_ptr msg)
  {
    webSocketServer::connection_ptr ptrConnet =
        m_cScoketSever.get_con_from_hdl(hdl);
    std::cout << "Message received: " << msg->get_payload() << std::endl;
    std::string response = "Server received: " + msg->get_payload();
    ptrConnet->send(response);
  }

  void httpcallback(websocketpp::connection_hdl hdl)
  {
    auto ptrConnetion = m_cScoketSever.get_con_from_hdl(hdl);
    auto req = ptrConnetion->get_request();
    auto strUri = req.get_uri();
    auto strMethod = req.get_method();

    INFO_LOG("httpcallback, method: %s, uri: %s", strMethod.c_str(),
             strUri.c_str());

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
