#include <iostream>
#include <thread>
#include <mutex>
#include <cpr/cpr.h>
#include "sqlite3.h"
using namespace std;

//=================================
//���������
//�������� ����� ����
const char* DB_NAME = "sites.db";
//����-��� ������� (��)
const cpr::Timeout tmOut = 5000;
//������� ������ (����� ����)
const int deep = 3;
//��������� ����� �������
const int numOfThreads = 500;
//=================================

mutex mtx;
int currentURL = 0;	//��������� �������� URL

//�������� �� ����������� � ����
#define SQL_CHECK_CONNECTION if (result != SQLITE_OK) { cout << "Error: " << sqlite3_errmsg(db) << endl; sqlite3_close(db); return; }

//�������� �� ����������� � ���� (� ������������� �������� ��� ������)
#define SQL_CHECK_CONNECTION_MTX if (result != SQLITE_OK) { cout << "Error: " << sqlite3_errmsg(db) << endl; sqlite3_close(db); mtx.unlock(); return; }

//����� ��������� �� ������
#define SQL_ERROR_COUT cout << "Error adding URL in sites: " << sqlite3_errmsg(db) << endl;

void createTable()
{
	sqlite3* db;
	int result = sqlite3_open(DB_NAME, &db);
	char* err_msg = 0;
	SQL_CHECK_CONNECTION	//�������� �� ����������� � ����

	const char* sql_createTable = "DROP TABLE IF EXISTS 'sites';"	//������� ���������� ������� 'sites', ���� ��� ����������
		"CREATE TABLE 'sites' ('id' INTEGER PRIMARY KEY AUTOINCREMENT, 'URL' TEXT UNIQUE, 'title' TEXT, 'is_empty' TEXT);"	//������ ����� ������� 'sites' � 4 ������
		"VACUUM;";	//������� ������ ������ ����� ����� ������
	result = sqlite3_exec(db, sql_createTable, 0, 0, &err_msg);

	if (result != SQLITE_OK) SQL_ERROR_COUT
	else cout << "Table 'sites' created." << endl;

	sqlite3_free(err_msg);
	sqlite3_close(db);
}

void get(vector<string>* _URL, int _numberOfThread)
{
	int usingURL;
	while (true)
	{
		mtx.lock();
		usingURL = currentURL;
		currentURL++;
		mtx.unlock();
		if (usingURL < _URL->size())
		{
			char* err_msg = 0;	//SQL-������
			//��������� �������� URL�
			//��������� ������ �� �����
			cpr::Url URL = _URL->at(usingURL);
			string data;
			try
			{
				data = cpr::Get(URL, tmOut).text;
			}
			catch (exception e)
			{
				//cout << "Exception! " << e.what() << endl;
				data = "None";
			}
			if (data != "")
			{
				mtx.lock();
				cout << usingURL << ": " << URL << ": getting data." << endl;
				//����������� title ��������
				string find_begin = "<title>", find_end = "</title>";
				size_t search_begin = data.find(find_begin), search_end = data.find(find_end);
				string title;
				if (search_begin != string::npos && search_end != string::npos)
					title = data.substr(search_begin + find_begin.size(), search_end - search_begin - find_begin.size());
				else
					title = "TITLE NOT FOUND!";

				//���������� ������ ����� � ����
				sqlite3* db;
				sqlite3_stmt* res;
				int result = sqlite3_open(DB_NAME, &db);
				SQL_CHECK_CONNECTION	//�������� �� ����������� � ����

				string sql_query = "UPDATE sites SET title = ?, is_empty = 'NOT EMPTY' WHERE URL = '" + _URL->at(usingURL) + "';";
				result = sqlite3_prepare_v2(db, sql_query.c_str(), -1, &res, 0);
				if (result == SQLITE_OK)
				{
					sqlite3_bind_text(res, 1, title.c_str(), -1, SQLITE_STATIC);
					sqlite3_step(res);
				}
				else SQL_ERROR_COUT

				sqlite3_finalize(res);
				sqlite3_close(db);
				mtx.unlock();
			}
			else
			{
				mtx.lock();
				cout << usingURL << ": " << URL << ": empty or TimeOut." << endl;
				string sql_query = "UPDATE sites SET is_empty = 'EMPTY' WHERE URL = '" + _URL->at(usingURL) + "';";
				sqlite3* db;
				int result = sqlite3_open(DB_NAME, &db);
				SQL_CHECK_CONNECTION_MTX	//�������� �� ����������� � ����
					result = sqlite3_exec(db, sql_query.c_str(), 0, 0, &err_msg);
				if (result != SQLITE_OK) SQL_ERROR_COUT

				sqlite3_close(db);
				mtx.unlock();
			}
			sqlite3_free(err_msg);
		}
		else break;
	}
}

void generate_URL(char first, char last, string _domain, vector<string>* _URL)
{
	cout << "Generating URLs..." << endl;
	//������������ ��������
	for (int i = 1; i <= deep; i++)
	{
		string word(i, first);
		bool finished = false;
		while (true)
		{
			//��������� ������
			string str_sql_query = "https://" + word + "." + _domain;
			_URL->push_back(str_sql_query);

			string::reverse_iterator it = word.rbegin();
			while (*it == last)
			{
				*it = first;
				++it;
				if (it == word.rend())
				{
					finished = true;
					break;
				}
			}
			if (finished) break;
			++ * it;
		}
	}
	cout << "Generated " << _URL->size() << " URLs." << endl;
	cout << "Generation complete." << endl;
}

void writeURLToDB(vector<string>* _URL)
{
	cout << "Writing URLs to DB..." << endl;
	//������ � ����
	sqlite3* db;
	int result = sqlite3_open(DB_NAME, &db);
	char* err_msg = 0;	//SQL-������
	SQL_CHECK_CONNECTION	//�������� �� ����������� � ����
	for (int i = 0; i < _URL->size(); i++)
	{
		string query = "INSERT OR IGNORE INTO sites ('URL') VALUES ('" + _URL->at(i) + "');";
		result = sqlite3_exec(db, query.c_str(), 0, 0, 0);
		if (result != SQLITE_OK) SQL_ERROR_COUT
	}
	sqlite3_close(db);
	cout << " Writing URLs to DB complete." << endl;
}

int main()
{
	vector<string> URL;	//URL ������

	//�������� �������
	createTable();

	string domain;
	cout << "Type domain to search in (ex: https://abc.XXX - type \"XXX\": ";
	cin >> domain;

	//����� ������ ��������
	chrono::time_point<chrono::system_clock> startTime = chrono::system_clock::now();
	//��������� URL �������� � ������� �� ����� ������� ��������� �� ����� ������� ���������
	generate_URL('a', 'z', domain, &URL);
	//������ URL�� � ����
	writeURLToDB(&URL);

	//��������� ������ �� URL:
	thread t[numOfThreads];
	for (int i = 0; i < numOfThreads; i++)
		t[i] = thread(static_cast<void(*)(vector<string>*, int)> (get), &URL, i);
	for (int i = 0; i < numOfThreads; i++)
		t[i].join();

	//����� ��������� ��������
	chrono::time_point<chrono::system_clock> finishTime = chrono::system_clock::now();
	//����� ������� ��������
	double elapsedTime = chrono::duration_cast<chrono::milliseconds>(finishTime - startTime).count();
	cout << endl << "Time process: " << elapsedTime / 1000 << " seconds." << endl << endl;
}