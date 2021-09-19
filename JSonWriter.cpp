#include "stdafx.h"
#include "JsonWriter.h"
#include <string>
#include <vector>

#if 0
#define dbg_flush(f) fflush(f)
#else
#define dbg_flush(f) ;
#endif

JSonWriter::JSonWriter()
{
	this->fh = NULL;
	this->indent = 0;
	this->first_element = true;
}

JSonWriter::~JSonWriter()
{
	;
}

bool JSonWriter::open(const wchar_t* filename)
{
#ifdef WIN32
	errno_t val = _wfopen_s(&fh, filename, L"wb");
	if (val != 0) {
		return false;
	}
#else
	FILE* fh = fopen(MQEncoding::WideToUtf8(filename).c_str(), "w");
	if (fh == NULL)
		return FALSE;
#endif
	return TRUE;
}

int JSonWriter::node(const char* key, int value)
{
	insertIndent();
	fprintf(fh, "\"%s\": %d", key, value);
	dbg_flush(fh);

	return 0;
}

int JSonWriter::node(const char *key, float value)
{
	insertIndent();
	fprintf(fh, "\"%s\": %.5f", key, value);
	dbg_flush(fh);

	return 0;
}

int JSonWriter::node(const char* key, const char* value)
{
	insertIndent();
	fprintf(fh, "\"%s\": \"%s\"", key, value);
	dbg_flush(fh);

	return 0;
}

int JSonWriter::node(const char* key, bool value)
{
	insertIndent();
	fprintf(fh, "\"%s\": %s", key, (value)?"true":"false");
	dbg_flush(fh);

	return 0;
}

int JSonWriter::writeArray(const char *key, int value)
{
	beginArray(key);
	insertIndent();
	fprintf(fh, "%d", value);
	endArray();
	dbg_flush(fh);

	return 0;
}

int JSonWriter::writeArray(const char* key, int value1, int value2, int value3)
{
	beginArray(key);
	insertIndent();
	fprintf(fh, "%d", value1);
	insertIndent();
	fprintf(fh, "%d", value2);
	insertIndent();
	fprintf(fh, "%d", value3);
	endArray();
	dbg_flush(fh);

	return 0;
}

int JSonWriter::writeArray(const char* key, float value1, float value2)
{
	beginArray(key);
	insertIndent();
	fprintf(fh, "%.5f", value1);
	insertIndent();
	fprintf(fh, "%.5f", value2);
	endArray();
	dbg_flush(fh);

	return 0;
}

int JSonWriter::writeArray(const char* key, float value1, float value2, float value3)
{
	beginArray(key);
	insertIndent();
	fprintf(fh, "%.5f", value1);
	insertIndent();
	fprintf(fh, "%.5f", value2);
	insertIndent();
	fprintf(fh, "%.5f", value3);
	endArray();
	dbg_flush(fh);

	return 0;
}

int JSonWriter::writeArray(const char* key, float value1, float value2, float value3, float value4)
{
	beginArray(key);
	insertIndent();
	fprintf(fh, "%.5f", value1);
	insertIndent();
	fprintf(fh, "%.5f", value2);
	insertIndent();
	fprintf(fh, "%.5f", value3);
	insertIndent();
	fprintf(fh, "%.5f", value4);
	endArray();
	dbg_flush(fh);

	return 0;
}

int JSonWriter::beginArray(const char* key)
{
	insertIndent();
	fprintf(fh, "\"%s\": [", key);
	startNodes();
	dbg_flush(fh);

	return 0;
}

int JSonWriter::endArray()
{
	endNodes();
	insertLastIndent();
	fputs("]", fh);
	dbg_flush(fh);

	return 0;
}

int JSonWriter::beginObject()
{
	insertIndent();
	fputs("{", fh);
	startNodes();
	dbg_flush(fh);

	return 0;
}

int JSonWriter::beginObject(const char* key)
{
	insertIndent();
	fprintf(fh, "\"%s\": {", key);
	startNodes();
	dbg_flush(fh);

	return 0;
}

int JSonWriter::endObject()
{
	endNodes();
	insertLastIndent();
	fputs("}", fh);
	dbg_flush(fh);

	return 0;
}

void JSonWriter::insertIndent() {
	int i;

	/* delim last line */
	if (this->first_element) {
		this->first_element = false;
		if (indent != 0) {
			fputs("\n", fh);
		}
	}
	else {
		fputs(",\n", fh);
	}

	if (this->indent > 0) {
		for (i = 0; i < this->indent; i++) {
			fputs("  ", fh);
		}
	}

	dbg_flush(fh);
}

void JSonWriter::insertLastIndent() {
	int i;

	/* delim last line */
	fputs("\n", fh);

	if (this->indent > 0) {
		for (i = 0; i < this->indent; i++) {
			fputs("  ", fh);
		}
	}
	dbg_flush(fh);
}

int JSonWriter::close()
{
	return fclose(this->fh);
}

void JSonWriter::startNodes()
{
	this->first_element = true;
	this->indent++;
}

void JSonWriter::endNodes()
{
	if (this->indent > 0) {
		this->indent--;
	}
}