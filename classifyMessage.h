//############################################
//#Date: 2019.12.29
//#Author: Modified by Jiaxiang Feng
//#Version: 2.0
//############################################
#ifndef _CLASSIFYMESSAGE_H_
#define _CLASSIFYMESSAGE_H_
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string>
#include "string.h"
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <iostream>
#include <assert.h>
#include <getopt.h>
#include <fstream>
#include <climits>
#include <vector>

typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

class Reader {
public:
  virtual int get() = 0;  // should return 0..255, or -1 at EOF
  virtual int read(char* buf, int n); // read to buf[n], return no. read
  virtual ~Reader() {}
};

class Writer {
public:
  virtual void put(int c) = 0;  // should output low 8 bits of c
  virtual void write(const char* buf, int n);  // write buf[n]
  virtual ~Writer() {}
};

class FileNameInfoProperty{
public:
	std::string fileName;
	std::vector<long long int> infoPosInTempFile;
	int fileNameLength = 0;
	int infoAppearedNum = 0;
	FileNameInfoProperty();
};

#endif
