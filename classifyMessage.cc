//############################################
//#Date: 2019.12.30
//#Author: Modified by Jiaxiang Feng
//#Version: 3.1
//############################################
#include "classifyMessage.h"
#include "dirent.h"
#include "sys/types.h"

#ifdef __linux__
  #include <sys/stat.h>
#endif

using std::string;

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

FileNameInfoProperty::FileNameInfoProperty(){
	infoPosInTempFile.clear();
	fileNameLength = 0;
	infoAppearedNum = 0;
}

ocInfoProperty::ocInfoProperty(){
	for(int i = 0; i < KEY_WORD_NUM; i++){
		infoPosInTempFile[i].clear();
	}
	ocNameLength = 0;
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
    if(!(log_in[0] = fopen(synth_log.c_str(), "rt"))){				//open input log file
    	perror(synth_log.c_str()), exit(1);
    }
    if(!(log_in[1] = fopen(impl_log.c_str(), "rt"))){				//open input log file
    	perror(impl_log.c_str()), exit(1);
    }

    string outFilename[KEY_WORD_NUM] = {
    	"warning",
    	"criticalWarning",
    	"error"
    };

    for(int i = 0; i < KEY_WORD_NUM; i++){
    	string fn = outFilename[i] + ".log";
    	log_out[i] = fopen(fn.c_str(), "wt");
    	if (!log_out[i]) perror(fn.c_str()), exit(1);
    }

    string outTempFilename[KEY_WORD_NUM] = {
    	"warning_temp",
    	"criticalWarning_temp",
    	"error_temp"
    };

    for(int i = 0; i < KEY_WORD_NUM; i++){
    	string fn = outTempFilename[i] + ".log";
    	log_out_temp[i] = fopen(fn.c_str(), "wt+");
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
    long long int endOfProcessInfoPos[PROCESS_NUM][KEY_WORD_NUM] = {0};
    for(int filein = 0; filein < PROCESS_NUM; filein++){
		std::cout << processWord[filein] << "classifying..." << std::endl;

		while((c = getc(log_in[filein])) != EOF){
			if(!frontFinishFlag){
				*frontPtr = c;
				*++frontPtr = '\0';
				frontCharNum++;
			}
			if(c == 10){							// Arrive at LF
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
		fclose(log_in[filein]);
		endOfProcessInfoPos[filein][0] = ftell(log_out_temp[0]);
		endOfProcessInfoPos[filein][1] = ftell(log_out_temp[1]);
		endOfProcessInfoPos[filein][2] = ftell(log_out_temp[2]);
    }

    long long int latestReadPos = 0, lastLeftBracketPos = 0, lastColonPos = 0;
    int fileNameLength = 0;
    char lastChar;
    char fileNameExp[300] = {};				//string temporary memory
    char fileNameAndLineExp[300] = {};				//string temporary memory
    long int appearedFileNameNum[KEY_WORD_NUM] = {0};
    long long int lineStartPos = 0, curNamePos = 0;
	std::vector<FileNameInfoProperty> appearedInfo[KEY_WORD_NUM];
	FileNameInfoProperty infoTemp;
    U8 alreadyExistFlag = 0;
    std::vector<long long> reservedMessage[KEY_WORD_NUM];
    int maxNameLength[KEY_WORD_NUM] = {0};

    for(int fileOutNum = 0; fileOutNum < KEY_WORD_NUM; fileOutNum++){
    	std::cout << "Searching the information in "<< keyWord[fileOutNum] << "by file name" << std::endl;
    	fseek(log_out_temp[fileOutNum], 0, SEEK_SET);
    	while((c = getc(log_out_temp[fileOutNum])) != EOF){
    		if(c == '['){
    			lastLeftBracketPos = ftell(log_out_temp[fileOutNum]);
    		}
    		if(c == ':'){
    			lastColonPos = ftell(log_out_temp[fileOutNum]);
    		}
    		if(c == 10){										//end of one line
    			if(lastChar == ']'){							//	This info contains [] in the end.
    				latestReadPos = ftell(log_out_temp[fileOutNum]);
    				if(lastLeftBracketPos < lastColonPos && lastColonPos < latestReadPos){		// This info contains file name info.
        				memset(fileNameAndLineExp, '\0', 300);
        				memset(fileNameExp, '\0', 300);
        				fseek(log_out_temp[fileOutNum], lastLeftBracketPos, SEEK_SET);
        				fread(fileNameAndLineExp, sizeof(char), latestReadPos-lastLeftBracketPos-2, log_out_temp[fileOutNum]);	//Read file name with line num
        				fileNameLength = lastColonPos-lastLeftBracketPos-1;
    					fseek(log_out_temp[fileOutNum], lastLeftBracketPos, SEEK_SET);
        				fread(fileNameExp, sizeof(char), fileNameLength, log_out_temp[fileOutNum]);			//Read file name
        				for(int i = 0; i < appearedFileNameNum[fileOutNum]; i++){										//Search the memeory for checking is this file appeared before
        					if(appearedInfo[fileOutNum][i].fileName == fileNameExp && appearedInfo[fileOutNum][i].fileNameLength == fileNameLength){
        						alreadyExistFlag = 1;
        						curNamePos = i;
        						break;
        					}
        				}
        				if(!alreadyExistFlag){										//This file name didn't show before.
        					infoTemp.fileNameLength = fileNameLength;
        					infoTemp.fileName = fileNameExp;
        					infoTemp.infoPosInTempFile.push_back(lineStartPos);
        					infoTemp.infoAppearedNum = 1;
        					appearedInfo[fileOutNum].push_back(infoTemp);
        					infoTemp.infoPosInTempFile.clear();
        					appearedFileNameNum[fileOutNum]++;
    						fseek(log_out_temp[fileOutNum], latestReadPos, SEEK_SET);
    						if(maxNameLength[fileOutNum] < fileNameLength) maxNameLength[fileOutNum] = fileNameLength;
        				}
        				else{														//This file name showed before.
							appearedInfo[fileOutNum][curNamePos].infoPosInTempFile.push_back(lineStartPos);
        					appearedInfo[fileOutNum][curNamePos].infoAppearedNum++;
        					alreadyExistFlag = 0;
        					fseek(log_out_temp[fileOutNum], latestReadPos, SEEK_SET);
        				}
    				}
    				else{									//This info doesn't have any file name info at end of line.
    					reservedMessage[fileOutNum].push_back(lineStartPos);
    				}
    			}
    			else{											//This info doesn't have any file name info at end of line.
    				reservedMessage[fileOutNum].push_back(lineStartPos);
    			}
    			lineStartPos = ftell(log_out_temp[fileOutNum]);
    		}
    		lastChar = c;
    	}
    	lineStartPos = 0;
    }


    std::string target = " [See ";
    std::string targetTemp;
    std::string::iterator it;
    U8 findColonFlag = 0;
    long long int nameStartPos = 0;
    std::vector<int> seefileInfoPosinReservedMeg[KEY_WORD_NUM];
    for(int fileOutNum = 0; fileOutNum < KEY_WORD_NUM; fileOutNum++){			//Search for " [See "
    	seefileInfoPosinReservedMeg[fileOutNum].clear();
    	for(int i = 0; i < reservedMessage[fileOutNum].size(); i++){
    		fseek(log_out_temp[fileOutNum], reservedMessage[fileOutNum][i], SEEK_SET);
    		targetTemp.clear();
    		while((c = getc(log_out_temp[fileOutNum])) != '\n'){

    			if(targetTemp.size() < 6){
    				targetTemp.push_back(c);
    			}
    			else if(targetTemp.size() == 6){
    				it = targetTemp.begin();
    				targetTemp.erase(it);
    				targetTemp.push_back(c);
        			if(targetTemp == target){						//Find " [See "
        				memset(fileNameAndLineExp, '\0', 300);
        				memset(fileNameExp, '\0', 300);
        				nameStartPos = ftell(log_out_temp[fileOutNum]);
        				while((c = getc(log_out_temp[fileOutNum])) != ']'){
        					if(c == ':'){
        						lastColonPos = ftell(log_out_temp[fileOutNum]);
        					}
        				}
        				latestReadPos = ftell(log_out_temp[fileOutNum]);
           				fseek(log_out_temp[fileOutNum], nameStartPos, SEEK_SET);
            			fread(fileNameAndLineExp, sizeof(char), latestReadPos-nameStartPos-1, log_out_temp[fileOutNum]);	//Read file name with line num
            			fileNameLength = lastColonPos-nameStartPos-1;
        				fseek(log_out_temp[fileOutNum], nameStartPos, SEEK_SET);
            			fread(fileNameExp, sizeof(char), fileNameLength, log_out_temp[fileOutNum]);			//Read file name

        				for(int j = 0; j < appearedFileNameNum[fileOutNum]; j++){										//Search the memeory for checking is this file appeared before
        					if(appearedInfo[fileOutNum][j].fileName == fileNameExp && appearedInfo[fileOutNum][j].fileNameLength == fileNameLength){
        						alreadyExistFlag = 1;
        						curNamePos = j;
        						break;
        					}
        				}
        				if(!alreadyExistFlag){										//This file name didn't show before.
        					infoTemp.fileNameLength = fileNameLength;
        					infoTemp.fileName = fileNameExp;
        					infoTemp.infoPosInTempFile.push_back(reservedMessage[fileOutNum][i]);
        					infoTemp.infoAppearedNum = 1;
        					appearedInfo[fileOutNum].push_back(infoTemp);
        					infoTemp.infoPosInTempFile.clear();
        					appearedFileNameNum[fileOutNum]++;
    						if(maxNameLength[fileOutNum] < fileNameLength) maxNameLength[fileOutNum] = fileNameLength;
        				}
        				else{														//This file name showed before.
							appearedInfo[fileOutNum][curNamePos].infoPosInTempFile.push_back(reservedMessage[fileOutNum][i]);
        					appearedInfo[fileOutNum][curNamePos].infoAppearedNum++;
        					alreadyExistFlag = 0;
        				}
        				seefileInfoPosinReservedMeg[fileOutNum].push_back(i);
        			}
    			}
    		}
    	}

    }

    for(int fileOutNum = 0; fileOutNum < KEY_WORD_NUM; fileOutNum++){						//erase "See" info in reserved message.
    	for(int i = 0; i < seefileInfoPosinReservedMeg[fileOutNum].size(); i++){
    		reservedMessage[fileOutNum].erase(reservedMessage[fileOutNum].begin() + seefileInfoPosinReservedMeg[fileOutNum][i] - i);
    	}
    }
    //Finding "constraint file 'xxx' "message function or other characteristic message function can be added here.

    //Write statistical data table.
    int fileNameCharLength = 35;			//Other Information Without File Name
    int quantityCharLength = 8;
    int columnNum = fileNameCharLength + quantityCharLength + 3;//File Name+Quantity+|||
    for(int fileOutNum = 0; fileOutNum < KEY_WORD_NUM; fileOutNum++){
    	if(maxNameLength[fileOutNum] > fileNameCharLength){
    		fileNameCharLength = maxNameLength[fileOutNum];
    		columnNum = fileNameCharLength + quantityCharLength + 3;
    	}
    	fseek(log_out[fileOutNum], 0, SEEK_END);
    	for(int i = 0; i < columnNum; i++){
    		putc('-', log_out[fileOutNum]);
    	}
    	putc('\n', log_out[fileOutNum]);

    	putc('|', log_out[fileOutNum]);
    	fwrite("File Name", sizeof(char), 9, log_out[fileOutNum]);
		for(int m = 0; m < fileNameCharLength - 9; m++){
			putc(' ', log_out[fileOutNum]);
		}
		putc('|', log_out[fileOutNum]);
		fwrite("Quantity", sizeof(char), 8, log_out[fileOutNum]);
		putc('|', log_out[fileOutNum]);putc('\n', log_out[fileOutNum]);

    	for(int fileNameNum = 0; fileNameNum < appearedInfo[fileOutNum].size(); fileNameNum++){
    		putc('|', log_out[fileOutNum]);
    		fwrite(appearedInfo[fileOutNum][fileNameNum].fileName.c_str(), sizeof(char), appearedInfo[fileOutNum][fileNameNum].fileNameLength, log_out[fileOutNum]);
    		for(int m = 0; m < fileNameCharLength - appearedInfo[fileOutNum][fileNameNum].fileNameLength; m++){
    			putc(' ', log_out[fileOutNum]);
    		}
    		putc('|', log_out[fileOutNum]);
    		string infoNum = std::to_string(appearedInfo[fileOutNum][fileNameNum].infoAppearedNum);
    		fwrite(infoNum.c_str(), sizeof(char), infoNum.length(), log_out[fileOutNum]);
    		for(int m = 0; m < 8 - infoNum.length(); m++){
    			putc(' ', log_out[fileOutNum]);
    		}
    		putc('|', log_out[fileOutNum]);putc('\n', log_out[fileOutNum]);
    	}

    	putc('|', log_out[fileOutNum]);
    	fwrite("Other Information Without File Name", sizeof(char), 35, log_out[fileOutNum]);
		for(int m = 0; m < fileNameCharLength - 35; m++){
			putc(' ', log_out[fileOutNum]);
		}
		putc('|', log_out[fileOutNum]);
		string infoNum = std::to_string(reservedMessage[fileOutNum].size());
		fwrite(infoNum.c_str(), sizeof(char), infoNum.length(), log_out[fileOutNum]);
		for(int m = 0; m < 8 - infoNum.length(); m++){
			putc(' ', log_out[fileOutNum]);
		}
		putc('|', log_out[fileOutNum]);putc('\n', log_out[fileOutNum]);

    	for(int i = 0; i < columnNum; i++){
    		putc('-', log_out[fileOutNum]);
    	}
    	putc('\n', log_out[fileOutNum]);
    	putc('\n', log_out[fileOutNum]);
    	putc('\n', log_out[fileOutNum]);
    	putc('\n', log_out[fileOutNum]);
    }

    U8 wroteImplFlag = 0;
    for(int fileOutNum = 0; fileOutNum < KEY_WORD_NUM; fileOutNum++){
    	std::cout << "Classify the information in "<< keyWord[fileOutNum] << "by file name" << std::endl;
        fwrite("------------------------------------------------------------Information with file name------------------------------------------------------------\n", sizeof(char), 147, log_out[fileOutNum]);
        putc(10, log_out[fileOutNum]);putc(10, log_out[fileOutNum]);
        for(int i = 0; i < appearedInfo[fileOutNum].size(); i++){
    		fwrite("Filename: ", sizeof(char), 10, log_out[fileOutNum]);						//Write the name this file
    		for(int m = 0; m < appearedInfo[fileOutNum][i].fileNameLength - 10; m++){
    			putc(' ', log_out[fileOutNum]);
    		}
    		putc('\t', log_out[fileOutNum]);putc('\t', log_out[fileOutNum]);
    		fwrite("Quantity: ", sizeof(char), 10, log_out[fileOutNum]);						//Write the number of the info about this file
    		putc(10, log_out[fileOutNum]);
    		fwrite(appearedInfo[fileOutNum][i].fileName.c_str(), sizeof(char), appearedInfo[fileOutNum][i].fileNameLength, log_out[fileOutNum]);
    		putc('\t', log_out[fileOutNum]);putc('\t', log_out[fileOutNum]);
    		string infoNum = std::to_string(appearedInfo[fileOutNum][i].infoAppearedNum);
    		fwrite(infoNum.c_str(), sizeof(char), infoNum.length(), log_out[fileOutNum]);
    		putc(10, log_out[fileOutNum]);
    		for(int j = 0; j < appearedInfo[fileOutNum][i].infoAppearedNum; j++){				//Write info to the output file
				 fseek(log_out_temp[fileOutNum], appearedInfo[fileOutNum][i].infoPosInTempFile[j], SEEK_SET);
				 if(appearedInfo[fileOutNum][i].infoPosInTempFile[j] > endOfProcessInfoPos[0][fileOutNum]){
					 if(!wroteImplFlag){
						 putc('\n', log_out[fileOutNum]);
						 fwrite("Implementation Information:\n", sizeof(char), 28, log_out[fileOutNum]);			//Write Implementation flag
						 wroteImplFlag = 1;
					 }
				 }
				 else{
					 if(j == 0){
						putc('\n', log_out[fileOutNum]);
				    	fwrite("Synthesis Information:\n", sizeof(char), 23, log_out[fileOutNum]);			//Write Synthesis flag
					 }
				 }
				 while((c = getc(log_out_temp[fileOutNum])) != 10 && (c != EOF)){
				 	putc(c, log_out[fileOutNum]);
				 }
				 putc(10, log_out[fileOutNum]);
    		}
    		fwrite("----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n",
    				sizeof(char), 221, log_out[fileOutNum]);
    		putc(10, log_out[fileOutNum]);putc(10, log_out[fileOutNum]);
    		wroteImplFlag = 0;
    	}
    }

    for(int fileOutNum = 0; fileOutNum < KEY_WORD_NUM; fileOutNum++){
    	std::cout << "Classify other information in "<< keyWord[fileOutNum] << std::endl;
        fwrite("------------------------------------------------------------Other Information------------------------------------------------------------\n", sizeof(char), 138, log_out[fileOutNum]);
        fwrite("Quantity: ", sizeof(char), 10, log_out[fileOutNum]);
		string infoNum = std::to_string(reservedMessage[fileOutNum].size());
		fwrite(infoNum.c_str(), sizeof(char), infoNum.length(), log_out[fileOutNum]);
		putc(10, log_out[fileOutNum]);
        for(int i = 0; i < reservedMessage[fileOutNum].size(); i++){
        	fseek(log_out_temp[fileOutNum], reservedMessage[fileOutNum][i], SEEK_SET);
			if(reservedMessage[fileOutNum][i] > endOfProcessInfoPos[0][fileOutNum]){
				if(!wroteImplFlag){
					putc('\n', log_out[fileOutNum]);
					fwrite("Implementation Information:\n", sizeof(char), 28, log_out[fileOutNum]);			//Write Implementation flag
					wroteImplFlag = 1;
				}
			}
			else{
				if(i == 0){
					putc('\n', log_out[fileOutNum]);
					fwrite("Synthesis Information:\n", sizeof(char), 23, log_out[fileOutNum]);			//Write Synthesis flag
				}
			}
			while((c = getc(log_out_temp[fileOutNum])) != 10 && (c != EOF)){
				putc(c, log_out[fileOutNum]);
			}
			putc(10, log_out[fileOutNum]);
        }
        wroteImplFlag = 0;
    }

    for(int i = 0; i < KEY_WORD_NUM; i++){
        fclose(log_out[i]);
		fclose(log_out_temp[i]);
		string fn = outTempFilename[i] + ".log";
		remove(fn.c_str());					//Delete temporary files.
    }


/************************************OC log**************************************/
    DIR *pDir;
    struct dirent *ent;
    string runs_Dir = projectDirectory + projectName + ".runs";
    pDir = opendir(runs_Dir.c_str());

    std::vector<std::string> ocDirName;

    while((ent=readdir(pDir))!=NULL){					//Search for all the subdirectory in .runs.
    	if(ent->d_type & DT_DIR){
    		if(strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..")==0)
    			continue;
    		if(strcmp(ent->d_name, ".jobs")==0)
    			continue;
    		std::string dirName = ent->d_name;
    		if(dirName.substr(0, 6) == "synth_")
    			continue;
    		if(dirName.substr(0, 5) == "impl_")
    			continue;
    		ocDirName.push_back(dirName);
    	}
    }

    std::cout << "Classify out-of-context module information." << std::endl;

    FILE *oc_log_in, *oc_log_out;
    string ocOutFilename = "oc_message.log";
    oc_log_out = fopen(ocOutFilename.c_str(), "wt+");				//Open oc output log file.
    if (!oc_log_out) perror(ocOutFilename.c_str()), exit(1);

    frontFinishFlag = 0;
	frontPtr = frontCharacterExp;
	frontCharacterExp[0] = '\0';
	frontCharNum = 0;
	std::vector<ocInfoProperty> ocInfo;
	ocInfoProperty ocInfoTemp;
	lineStartPos = 0;
    string oc_log_path;
    int maxOCModuleNameLen = 0;
    for(int ocLogNum = 0; ocLogNum < ocDirName.size(); ocLogNum++){
    	oc_log_path = projectDirectory + projectName + ".runs/" + ocDirName[ocLogNum] + "/runme.log";
    	if(!(oc_log_in= fopen(oc_log_path.c_str(), "rt")))continue;				//open input log file
    	ocInfoTemp.ocName = ocDirName[ocLogNum];
    	if(maxOCModuleNameLen < ocDirName[ocLogNum].length()){
    		maxOCModuleNameLen = ocDirName[ocLogNum].length();
    	}
    	ocInfoTemp.ocNameLength = ocDirName[ocLogNum].length();
    	ocInfo.push_back(ocInfoTemp);											//Add oc module property to vector.
		while((c = getc(oc_log_in)) != EOF){									//Record position of keyword info in the input log respectively.
			if(!frontFinishFlag){
				*frontPtr = c;
				*++frontPtr = '\0';
				frontCharNum++;
			}
			if(c == 10){							// Arrive at LF
				frontPtr = frontCharacterExp;		//Change ptr to the head of memory.
				frontCharacterExp[0] = '\0';
				frontCharNum = 0;
				frontFinishFlag = 0;
				lineStartPos = ftell(oc_log_in);
			}

			switch(frontCharNum){
				case 9:			//WARNING
					if(frontCharacterExp == keyWord[0]){
						frontPtr = frontCharacterExp;
						frontCharacterExp[0] = '\0';
						frontCharNum = 0;
						ocInfo.back().infoPosInTempFile[0].push_back(lineStartPos);
					}
					break;
				case 18:			//CRITICAL WARNING
					if(frontCharacterExp == keyWord[1]){
						frontPtr = frontCharacterExp;
						frontCharacterExp[0] = '\0';
						frontCharNum = 0;
						ocInfo.back().infoPosInTempFile[1].push_back(lineStartPos);
					}
					else{
						frontFinishFlag = 1;	//
						frontCharNum++;
					}
					break;
				case 7:			//ERROR
					if(frontCharacterExp == keyWord[2]){
						frontPtr = frontCharacterExp;
						frontCharacterExp[0] = '\0';
						frontCharNum = 0;
						ocInfo.back().infoPosInTempFile[2].push_back(lineStartPos);
					}
					break;
				default:
					break;
			}
		}
		fclose(oc_log_in);
    }

    string messageType[KEY_WORD_NUM] = {
    		"Warning Information\t\t",
			"Critical Warning Information\t\t",
			"Error Information\t\t"
    };

    string keyWordContent[KEY_WORD_NUM] = {
    		"Warning",
			"Critical Warning",
			"Error"
    };
    string ocInfoNumStr;

    /*************************Write Statistic Table***********************/
    int tableColumSectionLength[KEY_WORD_NUM + 1] = {
    		26,//"Out-of-context Module Name"
			keyWordContent[0].length(),//"Warning"
			keyWordContent[1].length(),//"Critical Warning"
			keyWordContent[2].length()//"Error"

    };
    columnNum = 5;		// |||||
    for(int i = 0; i < KEY_WORD_NUM + 1; i++){
    	columnNum += tableColumSectionLength[i];//| + ocName + | + Warning + | + Critical Warning + | + Error + |
    }
    if(maxOCModuleNameLen > tableColumSectionLength[0]){		//In case for max oc name is too small.
    	tableColumSectionLength[0] = maxOCModuleNameLen;
    	columnNum = 5;
        for(int i = 0; i < KEY_WORD_NUM + 1; i++){
        	columnNum += tableColumSectionLength[i];//| + ocName + | + Warning + | + Critical Warning + | + Error + |
        }
	}

	fseek(oc_log_out, 0, SEEK_END);
	for(int i = 0; i < columnNum; i++){
		putc('-', oc_log_out);
	}
	putc('\n', oc_log_out);

	putc('|', oc_log_out);
	fwrite("Out-of-context Module Name", sizeof(char), 26, oc_log_out);
	for(int m = 0; m < tableColumSectionLength[0] - 26; m++){
		putc(' ', oc_log_out);
	}
	putc('|', oc_log_out);
	fwrite("Warning", sizeof(char), 7, oc_log_out);
	putc('|', oc_log_out);
	fwrite("Critical Warning", sizeof(char), 16, oc_log_out);
	putc('|', oc_log_out);
	fwrite("Error", sizeof(char), 5, oc_log_out);
	putc('|', oc_log_out);
	putc('\n', oc_log_out);
	for(int ocModuleNum = 0; ocModuleNum < ocInfo.size(); ocModuleNum++){
		putc('|', oc_log_out);
		fwrite(ocInfo[ocModuleNum].ocName.c_str(), sizeof(char), ocInfo[ocModuleNum].ocName.length(), oc_log_out);
		for(int m = 0; m < tableColumSectionLength[0] - ocInfo[ocModuleNum].ocName.length(); m++){
			putc(' ', oc_log_out);
		}
		putc('|', oc_log_out);
		for(int keyWordNum = 0; keyWordNum < KEY_WORD_NUM; keyWordNum++){
			ocInfoNumStr = std::to_string(ocInfo[ocModuleNum].infoPosInTempFile[keyWordNum].size());
			fwrite(ocInfoNumStr.c_str(), sizeof(char), ocInfoNumStr.length(), oc_log_out);
			for(int m = 0; m < tableColumSectionLength[keyWordNum+1] - ocInfoNumStr.length(); m++){
				putc(' ', oc_log_out);
			}
			putc('|', oc_log_out);
		}
		putc('\n', oc_log_out);
	}
	for(int i = 0; i < columnNum; i++){
		putc('-', oc_log_out);
	}
	putc('\n', oc_log_out);
	putc('\n', oc_log_out);
	putc('\n', oc_log_out);
	putc('\n', oc_log_out);

	/*************************Write OC Message***********************/
    for(int ocLogNum = 0; ocLogNum < ocInfo.size(); ocLogNum++){				//Search every oc module.
    	oc_log_path = projectDirectory + projectName + ".runs/" + ocInfo[ocLogNum].ocName + "/runme.log";
    	if(!(oc_log_in= fopen(oc_log_path.c_str(), "rt"))){				//open input log file
    		perror(oc_log_path.c_str()), exit(1);
    	}
    	fwrite("Out-of-context Module Name:\t\t", sizeof(char), 29, oc_log_out);
    	fwrite(ocInfo[ocLogNum].ocName.c_str(), sizeof(char), ocInfo[ocLogNum].ocName.length(), oc_log_out);
    	putc(10, oc_log_out);
    	for(int keyWordNum = 0; keyWordNum < KEY_WORD_NUM; keyWordNum++){		//Split Info by keyword.
    		fwrite(messageType[keyWordNum].c_str(), sizeof(char), messageType[keyWordNum].length(), oc_log_out);
    		fwrite("Quantity:", sizeof(char), 9, oc_log_out);
    		ocInfoNumStr = std::to_string(ocInfo[ocLogNum].infoPosInTempFile[keyWordNum].size());
    		fwrite(ocInfoNumStr.c_str(), sizeof(char), ocInfoNumStr.length(), oc_log_out);
    		putc(10, oc_log_out);
    		putc(10, oc_log_out);
    		for(int infoNum = 0; infoNum < ocInfo[ocLogNum].infoPosInTempFile[keyWordNum].size(); infoNum++){		//Write different keyword info respectively.									//search all the Info of this type of keyword.
    			fseek(oc_log_in, ocInfo[ocLogNum].infoPosInTempFile[keyWordNum][infoNum], SEEK_SET);
    			while((c = getc(oc_log_in)) != 10 && (c != EOF)){
    				putc(c, oc_log_out);
    			}
    			putc(10, oc_log_out);
    		}
    		putc(10, oc_log_out);
    	}
    	fclose(oc_log_in);
    	fwrite("----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n",
    		sizeof(char), 221, oc_log_out);
    }

    fclose(oc_log_out);
    std::cout << "Classify finish." << std::endl;


}
