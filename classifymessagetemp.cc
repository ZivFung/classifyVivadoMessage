#include "classifyMessage.h"


#ifdef __linux__
  #include <sys/stat.h>
#endif


using std::string;


#define KEY_WORD_NUM 3
#define PROCESS_NUM 2

U64 file_size(char* filename){
	struct stat statbuf;
	stat(filename, &statbuf);
	U64 size = statbuf.st_size;
	return size;
}

int Reader::read(char* buf, int n) {
  int i=0, c;
  while (i<n && (c=get())>=0)
    buf[i++]=c;
  return i;
}

void Writer::write(const char* buf, int n) {
  for (int i=0; i<n; ++i)
    put(U8(buf[i]));
}

struct File: public Reader, public Writer {
  FILE* f;
  int get() {return getc(f);}
  void put(int c) {putc(c, f);}
  int read(char* buf, int n) {return fread(buf, 1, n, f);}
  void write(const char* buf, int n) {fwrite(buf, 1, n, f);}
};


static struct option const long_opts[] = {
        {"project",            		required_argument,  NULL,   'p'},
        {"synthesis_number",   		required_argument,  NULL,   's'},
        {"implementation_number",   required_argument,  NULL,   'i'},
        {"help",    no_argument,        NULL,   'h'},
        {0, 0, 0, 0}
};

struct FileNameInfoProperty{
	string fileName;
	int fileNameLength = 0;
	int infoAppearedNum = 0;
	int infoStartPosInOutFile = 0;
	int infoEndPosInOutFile = 0;
};

static void usage(const char *name)
{
    int i = 0;
    fprintf(stdout, "%s\n\n", name);
    fprintf(stdout, "usage: %s [OPTIONS]\n\n", name);
    fprintf(stdout, "Classify the warning, critical warning, error message of Vivado project.\n\n");

    fprintf(stdout, "  -%c (--%s) specify project directory\n",
            long_opts[i].val, long_opts[i].name);
    i++;
    fprintf(stdout, "  -%c (--%s) synthesis job number\n",
            long_opts[i].val, long_opts[i].name);
    i++;
    fprintf(stdout, "  -%c (--%s) implementation job number\n",
            long_opts[i].val, long_opts[i].name);
    i++;

    fprintf(stdout, "  -%c (--%s) print this help.\n",
            long_opts[i].val, long_opts[i].name);
    i++;
}

U8 sythesisNum, implementNum;
char *projectDirectory = NULL;
int main(int argc, char **argv)
{
	int cmd_opt;
    while((cmd_opt = getopt_long(argc, argv, "hi:s:p:", long_opts, NULL)) != -1)
    {
        switch (cmd_opt) {
        case 'p':
        	projectDirectory = strdup(optarg);
            break;
        case 's':
        	sythesisNum = atoll(optarg);
            break;
        case 'i':
        	implementNum = atoll(optarg);
            break;
        case 'h':
        default:
            usage(argv[0]);
            exit(0);
            break;
        }
    }
    if(projectDirectory == NULL){
    	fprintf(stdout,"You must specify the project directory!\n");
    	exit(0);
    }

    FILE *log_in[PROCESS_NUM], *log_out_temp[KEY_WORD_NUM], *log_out[KEY_WORD_NUM];	//in: synthesis, implementation.
    							//out:warning,critical warning, error.
    string Directory = projectDirectory;
    string projectName;
    int projectDirectoryLenth = Directory.length() - 1;
    int pos;
    if(Directory[projectDirectoryLenth] == 47){						//Fix input project directory different situation
    	for(int i = 1; ;i++){
    		if(Directory[projectDirectoryLenth-i] == '/'){
    			pos = projectDirectoryLenth - i + 1;
    			break;
    		}
    	}
    	projectName = Directory.substr(pos, projectDirectoryLenth-pos);
    }
    else{
    	for(int i = 0; ;i++){
    		if(Directory[projectDirectoryLenth-i] == '/'){
    			pos = projectDirectoryLenth - i;
    			break;
    		}
    	}
    	projectName = Directory.substr(pos);
    }
    std::cout << projectName << std::endl;
    string synth_log = projectDirectory + projectName + ".runs/synth_" + std::to_string(sythesisNum) + "/runme.log";
    string impl_log = projectDirectory + projectName + ".runs/impl_" + std::to_string(implementNum) + "/runme.log";
    std::cout << synth_log << std::endl;
    if(!(log_in[0] = fopen(synth_log.c_str(), "rb"))){				//open input log file
    	perror(synth_log.c_str()), exit(1);
    }
    if(!(log_in[1] = fopen(impl_log.c_str(), "rb"))){				//open input log file
    	perror(impl_log.c_str()), exit(1);
    }

    string outFilename[KEY_WORD_NUM] = {
    	"warning",
    	"criticalWarning",
    	"error"
    };

    for(int i = 0; i < KEY_WORD_NUM; i++){
    	string fn = outFilename[i] + ".log";
    	log_out[i] = fopen(fn.c_str(), "wb");
    	if (!log_out[i]) perror(fn.c_str()), exit(1);
    }

    string outTempFilename[KEY_WORD_NUM] = {
    	"warning_temp",
    	"criticalWarning_temp",
    	"error_temp"
    };

    for(int i = 0; i < KEY_WORD_NUM; i++){
    	string fn = outTempFilename[i] + ".log";
    	log_out_temp[i] = fopen(fn.c_str(), "wb");
    	if (!log_out_temp[i]) perror(fn.c_str()), exit(1);
    }

    char frontCharacterExp[20] = {};				//string temporary memory
    char c;
    char* frontPtr = frontCharacterExp;
    U8 breakLineFlag = 0;
    U32 frontCharNum = 0;


    string keyWord[KEY_WORD_NUM] = {
		"WARNING: ",
		"CRITICAL WARNING: ",
		"ERROR: "
    };

    U8 keyWordNum[KEY_WORD_NUM];

    for(int i = 0; i < KEY_WORD_NUM; i++){
    	keyWordNum[i] = keyWord[i].length();
    }

    string processWord[PROCESS_NUM] = {
		"Synthesis information\n",
		"Implementation information\n"
    };

    std::cout << "Start to classify the message." << std::endl;
    U8 frontFinishFlag = 0;
    for(int filein = 0; filein < PROCESS_NUM; filein++){
		for(int j = 0; j < KEY_WORD_NUM; j++){						//Write Process info to output file.
			for(int i = 0; i < processWord[filein].length(); i++){
        		putc(processWord[filein][i], log_out_temp[j]);
    		}
			putc('\n', log_out_temp[j]);
    	}
		std::cout << processWord[filein] << "classifying..." << std::endl;

		while((c = getc(log_in[filein])) != EOF){
			if(!frontFinishFlag){
				*frontPtr = c;
				*++frontPtr = '\0';
				frontCharNum++;
			}
			if(c == 10){							// Arrive at LF
				breakLineFlag = 1;
				frontPtr = frontCharacterExp;		//Change ptr to the head of memory.
				frontCharacterExp[0] = '\0';
				frontCharNum = 0;
				frontFinishFlag = 0;
			}


			switch(frontCharNum){
				case 9:			//WARNING
					if(frontCharacterExp == keyWord[0]){
						for(int i = 0; i < keyWordNum[0]; i++){
							putc(keyWord[0][i], log_out_temp[0]);
						}
						while((c = getc(log_in[filein])) != 10){
							putc(c, log_out_temp[0]);
						}
						putc(10, log_out_temp[0]);
						frontPtr = frontCharacterExp;
						frontCharacterExp[0] = '\0';
						frontCharNum = 0;
					}
					break;
				case 18:			//CRITICAL WARNING
					if(frontCharacterExp == keyWord[1]){
						for(int i = 0; i < keyWordNum[1]; i++){
							putc(keyWord[1][i], log_out_temp[1]);
						}
						while((c = getc(log_in[filein])) != 10){
							putc(c, log_out_temp[1]);
						}
						putc(10, log_out_temp[1]);
						frontPtr = frontCharacterExp;
						frontCharacterExp[0] = '\0';
						frontCharNum = 0;
					}
					else{
						frontFinishFlag = 1;	//
						frontCharNum++;
					}
					break;
				case 7:			//ERROR
					if(frontCharacterExp == keyWord[2]){
						for(int i = 0; i < keyWordNum[2]; i++){
							putc(keyWord[2][i], log_out_temp[2]);
						}
						while((c = getc(log_in[filein])) != 10){
							putc(c, log_out_temp[2]);
						}
						putc(10, log_out_temp[2]);
						frontPtr = frontCharacterExp;
						frontCharacterExp[0] = '\0';
						frontCharNum = 0;
					}
					break;
				default:
					break;
			}
		}
		for(int k = 0; k < KEY_WORD_NUM; k++){
			putc('\n', log_out_temp[k]);
			putc('\n', log_out_temp[k]);
			putc('\n', log_out_temp[k]);
			putc('\n', log_out_temp[k]);
		}

		fclose(log_in[filein]);
    }

    //fseek(in1.f,fileSize[0],SEEK_SET);      //move second co's file pointer to half of input file
    										//fseek(file, offset, fromwhere);
    int readPos = 0, latestReadPos = 0, lastLeftBracketPos = 0, lastColonPos = 0;
    int fileNameLength = 0;
    char lastChar;
    char fileNameExp[100] = {};				//string temporary memory
    char* fileNamePtr = fileNameExp;
    char fileNameAndLineExp[100] = {};				//string temporary memory
    char* fileNameAndLinePtr = fileNameAndLineExp;

    int appearedFileNameNum = 0;
    int lineStartPos = 0;

    FileNameInfoProperty* appearedInfo;
    U8 alreadyExistFlag = 0;
    for(int fileOutNum = 0; fileOutNum < KEY_WORD_NUM; fileOutNum++){
    	std::cout << "Classify the information in "<< keyWord[fileOutNum] << "by file name" << std::endl;

    	while((c = getc(log_out_temp[fileOutNum])) != EOF){
//    		readPos++;

    		if(c == "["){
    			lastLeftBracketPos = ftell(log_out_temp[fileOutNum]);
    		}
    		if(c == ":"){
    			lastColonPos = ftell(log_out_temp[fileOutNum]);
    		}

    		if(c == 10){										//end of one line
    			if(lastChar == "]"){							// This info contains file name info.
    				latestReadPos = ftell(log_out_temp[fileOutNum]);
    				fseek(log_out_temp[fileOutNum], lastLeftBracketPos, SEEK_SET);
    				fread(fileNameAndLineExp, sizeof(char), latestReadPos-lastLeftBracketPos-2, log_out_temp[fileOutNum]+1);	//Read file name with line num
    				fileNameLength = lastColonPos-lastLeftBracketPos-1;
    				fread(fileNameExp, sizeof(char), fileNameLength, log_out_temp[fileOutNum]+1);			//Read file name

    				int curNamePos = 0;
    				for(int i = 0; i < appearedFileNameNum; i++){
    					if(appearedInfo[appearedFileNameNum].fileName == fileNameExp && appearedInfo[appearedFileNameNum].fileNameLength == fileNameLength){
    						alreadyExistFlag = 1;
    						curNamePos = i;
    						break;
    					}

    				}

    				if(!alreadyExistFlag){										//This file name doesn't show before.
    					appearedInfo[appearedFileNameNum] = malloc(sizeof(FileNameInfoProperty));
    					appearedInfo[appearedFileNameNum].fileName = fileNameExp;
    					appearedInfo[appearedFileNameNum].fileNameLength = fileNameLength;
    					fwrite(fileNameExp, sizeof(char), fileNameLength, log_out[fileOutNum]);
    					appearedInfo[appearedFileNameNum].infoStartPosInOutFile = ftell(log_out[fileOutNum]);
    					appearedInfo[appearedFileNameNum].infoEndPosInOutFile = appearedInfo[appearedFileNameNum].infoStartPosInOutFile + 1;
    					appearedInfo[appearedFileNameNum].infoAppearedNum++;

    					fseek(log_out_temp[fileOutNum],lineStartPos, SEEK_SET);				//need to set ptr to end of out file everytime write one info
						while((c = getc(log_out_temp[filein])) != 10){
							putc(c, log_out[fileOutNum]);
						}
    				}
    				else{														//This file name showed before.
    					fseek(log_out[fileOutNum], appearedInfo[curNamePos].infoEndPosInOutFile, SEEK_SET);
    					fseek(log_out_temp[fileOutNum],lineStartPos, SEEK_SET);
						while((c = getc(log_out_temp[filein])) != 10){
							putc(c, log_out[fileOutNum]);
						}
    					appearedInfo[curNamePos].infoEndPosInOutFile = ftell(log_out[fileOutNum]) + 1;
    					appearedInfo[curNamePos].infoAppearedNum++;

    					alreadyExistFlag == 0;
    				}
    			}
    			else{											//This info doesn't have any file name info at end of line.


    			}

    			lineStartPos = ftell(log_out_temp[fileOutNum]);
    		}
    		lastChar = c;
    	}

    }




    std::cout << "Classify finish." << std::endl;
    for(int i = 0; i < KEY_WORD_NUM; i++){
        fclose(log_out[i]);
    }

}
