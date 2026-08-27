#include "logger.hpp"
#include "util.hpp"
#include "db.hpp"
#include "onlineManager.hpp"
#include "room.hpp"
#include "session.hpp"

void mysqlTest()
{
    MySQL_Util &SQL = MySQL_Util::getInstance();
    SQL.executeQuery("SELECT * FROM stu");
    MYSQL_RES *result = mysql_store_result(SQL.getConnection());
    int num_rows = mysql_num_rows(result);
    INFO_LOG("Number of rows: %d", num_rows);
    int num_cols = mysql_num_fields(result);
    INFO_LOG("Number of columns: %d\n", num_cols);
    for (int i = 0; i < num_rows; i++)
    {
        MYSQL_ROW row = mysql_fetch_row(result);
        for (int j = 0; j < num_cols; j++)
        {
            printf("%s\t", row[j] ? row[j] : "NULL");
        }
        printf("\n");
    }
}

void jsonTest()
{
    Json::Value root; // 创建一个根节点

    root["name"] = "John Doe";  // 添加字符串类型的数据
    root["age"] = 30;           // 添加整数类型的数据
    root["is_student"] = false; // 添加布尔类型的数据
    root["grades"].append(90);  // 添加数组类型的数据
    root["grades"].append(85);

    std::cout << Json_Util::serializeJson(root);
}

void stringTest()
{
    std::string str = "Hello, World!";
    auto vecStr = string_Util::split(str, "o, W");
    for (const auto &s : vecStr)
    {
        INFO_LOG("Split string: %s", s.c_str());
    }
}

void fileTest()
{
    std::string filePath = "./makefile";
    std::cout << file_Util::readFile(filePath) << std::endl;
}

void loginTest()
{
    user_table user;
    Json::Value userInfo;
    userInfo["username"] = "zhangsan";
    userInfo["password"] = "password123";
    // user.insertUser(userInfo);

    user.select_byUsername("zhangsan", userInfo);

    std::cout << Json_Util::serializeJson(userInfo);
    user.loginUser(userInfo);

    user.win(1);
    // user.lose(1);

    user.select_byUsername("zhangsan", userInfo);
    std::cout << Json_Util::serializeJson(userInfo);
}

void onlineTest()
{
    online_manager om;
    webSocketServer::connection_ptr wsServer;
    om.enterGameRoom(10086, wsServer);
    std::cout << om.isInGameHall(10086) << std::endl;
    std::cout << om.isInGameRoom(10086) << std::endl;

    om.exitGameRoom(10086);
    std::cout << om.isInGameHall(10086) << std::endl;
    std::cout << om.isInGameRoom(10086) << std::endl;
}

void testRoom()
{
    user_table tb;
    online_manager om;
    uint64_t roomID = 10086;
    room r(roomID, &tb, &om);
    uint16_t id = 123;
    r.addBlackUser(id);
    r.addBlackUser(12);
    r.addWhiteUser(uint16_t(656));
}

void testRoomManager()
{
    user_table tb;
    online_manager om;
    roomManager rm(&tb, &om);
    rm.createRoom(11, 15);
}

void testSession()
{
    webSocketServer sever;
    sessionManager sm(&sever);
}

int main()
{
    testSession();
}