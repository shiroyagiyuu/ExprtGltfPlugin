#pragma once
#include "stdio.h"

class JSonWriter
{
public:
	JSonWriter();
	~JSonWriter();

	bool open(const wchar_t* filename);
	int node(const char* key, const char* value);
	int node(const char* key, int value);
	int node(const char* key, float value);
	int node(const char* key, bool value);
	int writeArray(const char* key, int value);
	int writeArray(const char* key, int value1, int value2, int value3);
	int writeArray(const char* key, float value1, float value2);
	int writeArray(const char* key, float value1, float value2, float value3);
	int writeArray(const char* key, float value1, float value2, float value3, float value4);

	int beginArray(const char* key);
	int endArray();
	int beginObject();
	int beginObject(const char *key);
	int endObject();
	
	int close();



private:
	void startNodes();
	void endNodes();
	void insertIndent();
	void insertLastIndent();

	FILE   *fh;
	int		indent;
	bool	first_element;
};
