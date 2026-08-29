# 默认目标：编译并运行
run: gobang
	./gobang

# 正式编译（不带 -g，适合生产）
gobang: gobang.cc db.hpp logger.hpp onlineManager.hpp room.hpp util.hpp sever.hpp
	g++ $< -o $@ -lmysqlclient -ljsoncpp

# 调试编译（带 -g 符号表）
debug: gobang_debug
gobang_debug: gobang.cc db.hpp logger.hpp onlineManager.hpp room.hpp util.hpp sever.hpp
	g++ -g $< -o $@ -lmysqlclient -ljsoncpp

# 清理
clean:
	rm -f gobang gobang_debug