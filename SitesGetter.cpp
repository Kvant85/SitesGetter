#include <iostream>
#include <cpr/cpr.h>
#include "sqlite3.h"

using namespace std;

//Название файла базы
const char* DB_NAME = "test.db";
//Тайм-аут запроса (мс)
cpr::Timeout tmOut = 5000;

void createTable()
{
	sqlite3* db;
	int result = sqlite3_open(DB_NAME, &db);
	char* err_msg = 0;
	if (result != SQLITE_OK)
	{
		cout << "Error: " << sqlite3_errmsg(db) << endl;
		sqlite3_close(db);
		return;
	}

	const char* sql_createTable = "DROP TABLE IF EXISTS 'sites';"	//Убивает содержимое таблицы 'sites', если она существует
		"CREATE TABLE 'sites' ('id' INTEGER PRIMARY KEY AUTOINCREMENT, 'URL' TEXT UNIQUE, 'title' TEXT, 'is_empty' TEXT);"	//Создаёт новую таблицу 'sites' с 4 полями
		"VACUUM;";	//Очищает старое пустое место после дропов
	result = sqlite3_exec(db, sql_createTable, 0, 0, &err_msg);

	if (result != SQLITE_OK)	//Получение ошибки
	{
		cout << "Error creating tables: " << err_msg << endl;
		sqlite3_free(err_msg);
		sqlite3_close(db);
	}
	else cout << "Table 'sites' created." << endl;
}

void get(string _URL)
{
	//Добавление сайта в базу
	sqlite3* db;
	int result = sqlite3_open(DB_NAME, &db);
	char* err_msg = 0;	//SQL-ошибка
	if (result != SQLITE_OK)
	{
		cout << "Error: " << sqlite3_errmsg(db) << endl;	//Ошибка открытия файла базы
		sqlite3_close(db);
		return;
	}

	cpr::Url URL = _URL;
	string data = cpr::Get(URL, tmOut).text;
	string str_sql_query = "INSERT OR IGNORE INTO sites ('URL') VALUES ('" + _URL + "');";
	if (data != "")
	{
		cout << URL << ": getting data." << endl;
		//Вытаскиваем title страницы
		string find_begin = "<title>", find_end = "</title>";
		int search_begin = data.find(find_begin), search_end = data.find(find_end);
		string result = data.substr(search_begin + find_begin.size(), search_end - search_begin - find_begin.size());

		str_sql_query = "INSERT OR IGNORE INTO sites ('URL', 'title', 'is_empty') VALUES ('" + _URL + "', '" + result + "', 'NOT EMPTY');";
	}
	else	{

		cout << URL << ": empty or TimeOut." << endl;
		str_sql_query = "INSERT OR IGNORE INTO sites ('URL', 'is_empty') VALUES ('" + _URL + "', NULL);";
	}

	const char* sql_query_author = str_sql_query.c_str();
	result = sqlite3_exec(db, sql_query_author, 0, 0, &err_msg);
	if (result != SQLITE_OK) cout << "Error adding '" << _URL << "' in sites: " << err_msg << endl;	//Получение ошибки

	cout << "=====================================" << endl;
	sqlite3_close(db);
}

void generate_URL(char first, char last, int n)
{
	string word(n, first);
	while (true)
	{
		//Формируем ссылку и переходим по ней
		string str_URL = "https://" + word + ".com";
		get(str_URL);
		string::reverse_iterator it = word.rbegin();
		while (*it == last)
		{
			*it = first;
			++it;
			if (it == word.rend()) return;
		}
		++ * it;
	}
}

int main()
{
	//Создание таблицы
	createTable();
	for (int i = 1; i <= 2; i++)
		generate_URL('a', 'z', i);
}