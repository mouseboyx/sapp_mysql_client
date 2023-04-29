// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

//
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <string>
#include <future>
#include <iconv.h>
#include <stdexcept>
using namespace std;
//mutex is global to the dll
std::mutex g_driverMutex;
// Function to get the MySQL Connector/C++ connection instance
sql::Connection* getConInstance(string server_s, string username_s, string password_s) {
	std::lock_guard<std::mutex> lock(g_driverMutex); // Acquire the mutex
	sql::ConnectOptionsMap connection_properties;
	connection_properties["hostName"] = server_s;
	connection_properties["userName"] = username_s;
	connection_properties["password"] = password_s;
	connection_properties["CLIENT_MULTI_STATEMENTS"] = true;
	sql::Driver* driver = get_driver_instance(); // Call get_driver_instance()
	sql::Connection* con = driver->connect(connection_properties);
	return con; // Return the connection instance
}

std::mutex iconv_mutex;
//iconv function to convert WINDOWS-1252 to utf8, halo's extended characters that can be used in names and chat messages beyond byte 127 are WINDOWS-1252 (or similar 0-255 encoding)
std::string convertToUTF8(const std::string& input) {
	std::lock_guard<std::mutex> lock(iconv_mutex);
	iconv_t cd = iconv_open("UTF-8", "WINDOWS-1252");
	if (cd == (iconv_t)-1) {
		throw std::runtime_error("iconv_open failed");
	}

	const char* inbuf = input.c_str(); // cast to const char*
	size_t inbytesleft = input.length();

	size_t outbytesleft = inbytesleft * 4;
	std::string output(outbytesleft, '\0');
	char* outbuf = &output[0];

	if (iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1) {
		throw std::runtime_error("iconv failed");
	}

	output.resize(output.length() - outbytesleft);

	iconv_close(cd);

	return output;
}

std::string convertToWindows1252(const std::string& input) {
	std::lock_guard<std::mutex> lock(iconv_mutex);
	iconv_t cd = iconv_open("WINDOWS-1252", "UTF-8");
	if (cd == (iconv_t)-1) {
		throw std::runtime_error("iconv_open failed");
	}

	const char* inbuf = input.c_str();
	size_t inbytesleft = input.length();

	size_t outbytesleft = inbytesleft * 4;
	std::string output(outbytesleft, '\0');
	char* outbuf = &output[0];

	if (iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1) {
		throw std::runtime_error("iconv failed");
	}

	output.resize(output.length() - outbytesleft);

	iconv_close(cd);

	return output;
}

class MysqlResultAsync {

public:
	bool async_complete = false;
	string server_s;
	string username_s;
	string password_s;
	string database_s;
	string query_s;
	bool convert_result_encoding_b;
	char*** return_value;

	MysqlResultAsync(const std::string& server, const std::string& username, const std::string& password, const std::string& database, const std::string& query, bool convert_result_encoding) {  //class constructor


		server_s = server;
		username_s = username;
		password_s = password;
		database_s = database;
		query_s = query;
		convert_result_encoding_b = convert_result_encoding;
	}
	~MysqlResultAsync() {  //class deconstructor
		if (return_value != nullptr) {
			//delete any new variables created by the class here
			// Step 1: Delete each char** and its associated char* arrays
			for (int i = 0; return_value[i] != nullptr; i++) {
				for (int j = 0; return_value[i][j] != nullptr; j++) {
					delete[] return_value[i][j]; // Delete each char* array
				}
				delete[] return_value[i]; // Delete each char** array
			}

			// Step 2: Delete the char*** itself
			delete[] return_value;
			//delete return_value;
		}
	}


	std::future<void> future_result = std::async([this]() {
		do_async_work();
		});

	void do_async_work() {

		try {


			//sql::Driver* driver = getDriverInstance();
			sql::Connection* con = getConInstance(server_s, username_s, password_s);
			con->setSchema(database_s);
			sql::Statement* stmt;
			sql::ResultSet* res;
			stmt = con->createStatement();
			res = stmt->executeQuery(query_s);
			sql::ResultSetMetaData* rsmd = res->getMetaData();

			int num_rows = res->rowsCount();
			if (num_rows == 0) {
				return_value = nullptr;
				delete res;
				delete stmt;
				delete con;
				async_complete = true;
				return;

			}
			int num_cols = rsmd->getColumnCount();

			char*** data = new char** [num_rows + 1];
			int i = 0;
			int jj = 0;
			while (res->next()) {
				data[i] = new char* [num_cols + 1];
				jj = 0;
				for (int j = 1; j <= num_cols; j++) {
					//sql::SQLString columnType = res->getMetaData()->getColumnTypeName(j);


					//
					string single_result = res->getString(j);
					string col_val = single_result;
					if (convert_result_encoding_b == true) {
						col_val = convertToWindows1252(single_result);
					}
					//

					//cout << col_val.length() << endl;
					int num_chars = col_val.length();
					data[i][jj] = new char[num_chars + 1];
					//const char* cstr = col_val.c_str();
					//strcpy_s(data[i][jj], num_chars, cstr);
					for (int k = 0; k < num_chars; k++) {
						data[i][jj][k] = col_val[k];
					}
					data[i][jj][num_chars] = '\0';
					//cout << col_val << endl;
					jj++;
				}
				data[i][num_cols] = nullptr;
				i++;
			}
			data[num_rows] = nullptr;



			//std::cout << "Data has been populated in the DLL." << std::endl;
			delete res;
			delete stmt;
			delete con;
			return_value = data;

		}
		catch (sql::SQLException e) {
			cout << "error" << e.what() << endl;
			return_value = nullptr;
		}
		//std::this_thread::sleep_for(std::chrono::seconds(2));
		//cout << "async_complete=true" << endl;
		async_complete = true;
	}
	bool get_async_status() {
		return async_complete;
	}

	char*** join_async() {
		future_result.get();
		return return_value;
	}
};

class MysqlQueryAsync {

public:
	bool async_complete = false;
	string server_s;
	string username_s;
	string password_s;
	string database_s;
	string query_s;
	MysqlQueryAsync(const std::string& server, const std::string& username, const std::string& password, const std::string& database, const std::string& query) {  //class constructor


		server_s = server;
		username_s = username;
		password_s = password;
		database_s = database;
		query_s = query;
	}

	~MysqlQueryAsync() {  //class deconstructor
		//delete any new variables created by the class here
	}


	std::future<void> future_result = std::async([this]() {
		do_async_work();
		});

	void do_async_work() {
		try {
			//sql::Driver* driver = get_driver_instance();
			//sql::Driver* driver = getDriverInstance();
			sql::Connection* con = getConInstance(server_s, username_s, password_s);
			con->setSchema(database_s);
			sql::Statement* stmt;
			stmt = con->createStatement();
			stmt->execute(query_s);
			delete stmt;
			delete con;
		}
		catch (sql::SQLException e) {
			cout << "error" << e.what() << endl;
		}
		async_complete = true;
	}
	bool get_async_status() {
		return async_complete;
	}

	void join_async() {
		future_result.get();

	}
};

extern "C" {
	//mysqlresultasync RESULTS
	__declspec(dllexport) void* mysql_query_results_async_ptr(const char* server, const char* username, const char* password, const char* database, const char* query, bool convert_query_encoding, bool convert_result_encoding)
	{
		string query_s = query;
		if (convert_query_encoding == true) {
			//string query_before_encode = query;
			query_s = convertToUTF8(query);
		}
		MysqlResultAsync* mysqlresultasync = new MysqlResultAsync(server, username, password, database, query_s, convert_result_encoding);
		return reinterpret_cast<void*>(mysqlresultasync);


	}

	__declspec(dllexport) bool get_mysql_query_results_async_status(MysqlResultAsync* mysqlresultasync)
	{
		return mysqlresultasync->get_async_status();

	}

	__declspec(dllexport) char*** join_mysql_query_results_async(MysqlResultAsync* mysqlresultasync)
	{
		char*** return_value = mysqlresultasync->join_async();

		return return_value;
	}

	__declspec(dllexport) void delete_mysql_query_results_async_ptr(MysqlResultAsync* mysqlresultasync)
	{
		delete mysqlresultasync;
	}

	//mysqlqueryasync QUERY
	__declspec(dllexport) void* mysql_query_async_ptr(const char* server, const char* username, const char* password, const char* database, const char* query, bool convert_query_encoding)
	{
		string query_s = query;
		if (convert_query_encoding == true) {
			//string query_before_encode = query;
			query_s = convertToUTF8(query);
		}
		MysqlQueryAsync* mysqlqueryasync = new MysqlQueryAsync(server, username, password, database, query_s);
		return reinterpret_cast<void*>(mysqlqueryasync);


	}

	__declspec(dllexport) bool get_mysql_query_async_status(MysqlQueryAsync* mysqlqueryasync)
	{
		return mysqlqueryasync->get_async_status();

	}

	__declspec(dllexport) void join_mysql_query_async(MysqlQueryAsync* mysqlqueryasync)
	{
		mysqlqueryasync->join_async();

		//return return_value;
	}

	__declspec(dllexport) void delete_mysql_query_async_ptr(MysqlQueryAsync* mysqlqueryasync)
	{
		delete mysqlqueryasync;
	}

	// Non async functions
	__declspec(dllexport) void* get_mysql_con_wrapper(const char* server, const char* username, const char* password, const char* database)
	{
		std::string server_s = server;
		std::string username_s = username;
		std::string password_s = password;
		std::string database_s = database;
		try
		{
			//sql::Driver* driver = get_driver_instance();
			//sql::Driver* driver = getDriverInstance();
			sql::Connection* con = getConInstance(server_s, username_s, password_s);
			con->setSchema(database_s);
			return reinterpret_cast<void*>(con);
		}
		catch (sql::SQLException e)
		{
			cout << "Could not connect to mysql server. Error message: " << e.what() << endl;
			//exit(1);
			return reinterpret_cast<void*>(NULL);
		}

	}
	__declspec(dllexport) void delete_mysql_con(sql::Connection* con)
	{
		delete con;
	}

	__declspec(dllexport) void mysql_query(sql::Connection* con, const char* query, bool convert_query_encoding)
	{
		string query_s = query;
		if (convert_query_encoding == true) {
			//string query_before_encode = query;
			query_s = convertToUTF8(query);
		}
		try
		{

			sql::Statement* stmt;

			stmt = con->createStatement();
			stmt->execute(query_s);
			delete stmt;
		}
		catch (sql::SQLException e)
		{
			cout << e.what() << endl;


		}

	}


	__declspec(dllexport) char*** mysql_query_results(sql::Connection* con, const char* query, bool convert_query_encoding, bool convert_result_encoding)
	{
		string query_s = query;
		if (convert_query_encoding == true) {
			//string query_before_encode = query;
			query_s = convertToUTF8(query);
		}

		try {
			sql::Statement* stmt;
			sql::ResultSet* res;
			stmt = con->createStatement();
			res = stmt->executeQuery(query_s);
			sql::ResultSetMetaData* rsmd = res->getMetaData();

			int num_rows = res->rowsCount();
			if (num_rows == 0) {
				delete res;
				delete stmt;
				return nullptr;

			}
			int num_cols = rsmd->getColumnCount();

			char*** data = new char** [num_rows + 1];
			int i = 0;
			int jj = 0;
			while (res->next()) {
				data[i] = new char* [num_cols + 1];
				jj = 0;
				for (int j = 1; j <= num_cols; j++) {
					//sql::SQLString columnType = res->getMetaData()->getColumnTypeName(j);
					string single_result = res->getString(j);
					string col_val = single_result;
					if (convert_result_encoding == true) {
						col_val = convertToWindows1252(single_result);
					}
					//std::string col_val = convertToWindows1252(res->getString(j));
					//cout << col_val << endl;
					int num_chars = col_val.length();
					data[i][jj] = new char[num_chars + 1];
					//const char* cstr = col_val.c_str();
					//strcpy_s(data[i][jj], num_chars, cstr);
					for (int k = 0; k < num_chars; k++) {
						data[i][jj][k] = col_val[k];
					}
					data[i][jj][num_chars] = '\0';
					//cout << col_val << endl;
					jj++;
				}
				data[i][num_cols] = nullptr;
				i++;
			}
			data[num_rows] = nullptr;



			//std::cout << "Data has been populated in the DLL." << std::endl;
			delete res;
			delete stmt; //
			//delete rsmd;
			return data;

		}
		catch (sql::SQLException e) {
			cout << "error" << e.what() << endl;
		}
		return nullptr;
	}

	__declspec(dllexport) void free_mysql_data_ptr(char*** arr) {

		// Step 1: Delete each char** and its associated char* arrays
		for (int i = 0; arr[i] != nullptr; i++) {
			for (int j = 0; arr[i][j] != nullptr; j++) {
				delete[] arr[i][j]; // Delete each char* array
			}
			delete[] arr[i]; // Delete each char** array
		}

		// Step 2: Delete the char*** itself
		delete[] arr;

	}

}




//

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

