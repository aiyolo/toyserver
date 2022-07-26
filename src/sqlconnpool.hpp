#include <mutex>
#include <thread>
#include <queue>
#include <mysql/mysql.h>
#include <assert.h>
#include <iostream>

class SqlConnPool{
public:
	static SqlConnPool* getInstance();
	void init(const char* host, const char* user, const char* passwd, const char* db, int port, int maxConn);
	MYSQL* getConn();
	void releaseConn(MYSQL* conn);
	void destroy();
    int getFreeConnCount();

private:
	SqlConnPool();
	~SqlConnPool();
	int userCount;
    int maxConn;
	std::mutex mtx;
	std::queue<MYSQL*> connQueue;

};

inline SqlConnPool::SqlConnPool():userCount(0){}

inline SqlConnPool* SqlConnPool::getInstance(){
    static SqlConnPool instance;
    return &instance;
}
inline void SqlConnPool::init(const char* host, const char* user, const char* passwd, const char* db, int port, int maxConn){
	assert(maxConn>0);
	for(int i=0; i<maxConn; i++){
		MYSQL* conn = mysql_init(NULL);
		if(!conn){
			std::cout << "mysql init error!\n";
			assert(conn);
		}
		conn = mysql_real_connect(conn, host, user, passwd, db, port, NULL, 0);
		if(!conn){
			std::cout << "mysql connect error!\n";
		}
		connQueue.push(conn);
	}
}

inline MYSQL* SqlConnPool::getConn(){
	std::lock_guard<std::mutex> locker(mtx);
	if(connQueue.empty()){
		std::cout << "no more conn!\n";
		return NULL;
	}
	MYSQL* conn = connQueue.front();
	connQueue.pop();
	return conn;
}

inline void SqlConnPool::releaseConn(MYSQL* conn){
	std::lock_guard<std::mutex> locker(mtx);
	connQueue.push(conn);
}

inline int SqlConnPool::getFreeConnCount(){
    std::lock_guard<std::mutex> locker(mtx);
    return connQueue.size();
}

inline void SqlConnPool::destroy(){
	std::lock_guard<std::mutex> locker(mtx);
	while(!connQueue.empty()){
		MYSQL* conn = connQueue.front();
		connQueue.pop();
		mysql_close(conn);
	}
}

inline SqlConnPool::~SqlConnPool(){
	destroy();
}