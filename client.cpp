/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Devan Patel
	UIN: 733006310
	Date: 09/21/25
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <fstream>
#include <iomanip> 
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <cstdio>
#include <algorithm>



using namespace std;


int main (int argc, char *argv[]) {

	//necessary variables
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int buffer_capacity = MAX_MESSAGE;
	bool do_new_channel = false;
	string filename = "";
	vector<FIFORequestChannel*> channels;


	//arguments
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				buffer_capacity = atoi(optarg);
				break;
			case 'c':
				do_new_channel = true;
				break;
		}
	}

	// fork and child calls server
	pid_t pid = fork();

	if(pid < 0){
		perror("fork");
		return 1;
	}

	if(pid == 0){
		std::string mstr = std::to_string(buffer_capacity);
		execlp("./server", "./server", "-m", mstr.c_str(), (char*)NULL);
		perror("execlp");
		_exit(1);
	}


	// use vector to use last channel added
    FIFORequestChannel* control = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(control);

	if(do_new_channel){
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
		channels[0]->cwrite(&nc, sizeof(nc));

		char namebuf[64] = {0};
		int n = channels[0]->cread(namebuf, sizeof(namebuf));
		if(n <= 0 || namebuf[0] == '\0'){
			fprintf(stderr, "failed to get new channel\n");
		}else{
			channels.push_back(new FIFORequestChannel(string(namebuf), FIFORequestChannel::CLIENT_SIDE));
		}
	}

	FIFORequestChannel* chan = channels.back();






	// if person is specified, no filename is present, and time or e reading is specifiecd, then get the readings for that person at that time
	if(p != -1 && (t != -1 || e != -1)){
		datamsg req(p, t, e);
		chan->cwrite(&req, sizeof(req));
		double val = 0.0;
		chan->cread(&val, sizeof(double));
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << setprecision(10) << val << endl;
	}



	// if person is specified, no filename is present, and no time or e reading is specifiecd, then read first thousand lines of x1 for e1 and e2 and write to recieved x1.csv
	else if (p != -1 && filename.empty()){
		ofstream out("received/x1.csv");
		if(!out){
			perror("open x1.csv");
		}

		double time_s = 0.0;
		for(int i = 0; i < 1000; i++, time_s += 0.004){
			datamsg r1(p, time_s, 1);
			chan->cwrite(&r1, sizeof(r1));
			double v1 = 0.0;
			chan->cread(&v1, sizeof(double));

			datamsg r2(p, time_s, 2);
			chan->cwrite(&r2, sizeof(r2));
			double v2 = 0.0;
			chan->cread(&v2, sizeof(double));

			out << setprecision(10) << time_s << "," << v1 << "," << v2 << "\n";
		}
		out.close();
	}

	if(!filename.empty()){
		//request buffer
		filemsg fm0(0, 0);
		int buffer_request = sizeof(filemsg) + (int)filename.size() + 1;
		vector<char> req(buffer_request);
		memcpy(req.data(), &fm0, sizeof(filemsg));
		strcpy(req.data() + sizeof(filemsg), filename.c_str());

		// ask for file size
		chan->cwrite(req.data(), buffer_request);
		int64_t fsize = 0;
		chan->cread(&fsize, sizeof(fsize));

		// open binary output
		string outpath = "received/" + filename;
		FILE* fp = fopen(outpath.c_str(), "wb");
		if(!fp){
			perror("fopen file");
			return 1;
		}

		// read chunks in buffer capacity size until whole file is read
		vector<char> rbuf(buffer_capacity);
		int64_t off = 0;
		while(off < fsize){
			// next chunk length
			int len = (int)min<int64_t>(buffer_capacity, fsize - off);
			filemsg fmhdr(off, len);
			memcpy(req.data(), &fmhdr, sizeof(filemsg));
			chan->cwrite(req.data(), buffer_request);

			int n = chan->cread(rbuf.data(), len);
			if(n != len){
				fprintf(stderr, "short read (%d != %d)\n", n, len);
			}

			if((int)fwrite(rbuf.data(), 1, len, fp) != len){
				fprintf(stderr, "short fwrite");
				fclose(fp);
				return 1;
			}

			off += len;
		}
		fclose(fp);
	}
	
	// closing the channel    
    MESSAGE_TYPE q = QUIT_MSG;
    for(int i = (int)channels.size() - 1; i >= 0; i--){
		channels[i]->cwrite(&q, sizeof(q));
	}

	for(FIFORequestChannel* c : channels){
		delete c;
	}

	int status = 0;
	waitpid(pid, &status, 0);
	return 0;
}
