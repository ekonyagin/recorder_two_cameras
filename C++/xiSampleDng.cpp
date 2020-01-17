#include "stdafx.h"
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <memory.h>
#include <time.h>
#include <sys/timeb.h>
#include <m3api/xiapi_dng_store.h> // Linux, OSX


#define HandleResult(res,place) if (res!=XI_OK) {printf("Error after %s (%d)\n",place,res);exit(1);}
#define CAMERAS_ON_SAME_CONTROLLER 2
#define SAFE_MARGIN_PERCENTS 10
#define PORT 50009

float timedelta(const timeb& finish, const timeb& start){
	float t_diff = (float) (1.0 * (finish.time - start.time)
        	+ 0.001*(finish.millitm - start.millitm));
	return t_diff;
}

int MsgDecode(const std::string& data, 
			  std::string& name, 
			  int& length, 
			  int& fps){
    std::stringstream container(data);
    container >> name;
    if (name == "close"){
        	//shutdown(server_fd, SHUT_RDWR);
        	//close(new_socket);
        	//close(server_fd);
        	exit(0);
    }
    container >> length;
    container >> fps;
    return 0;
}


XI_RETURN InitializeCameras(HANDLE& cam1, HANDLE& cam2, const int& fps) {
	XI_RETURN stat = XI_OK;

	xiSetParamInt(0, XI_PRM_AUTO_BANDWIDTH_CALCULATION, XI_OFF);
	// open the cameras
	xiOpenDevice(0, &cam1);
	xiOpenDevice(1, &cam2);
	
	// set interface data rate
	int interface_data_rate=2400; // when USB3 hub is used
	// calculate datarate for each camera

	int camera_data_rate = interface_data_rate / CAMERAS_ON_SAME_CONTROLLER;
	// each camera should send less data to keep transfer reliable
	
	camera_data_rate -= camera_data_rate*SAFE_MARGIN_PERCENTS/100;
	// set data rate
	xiSetParamInt(cam1, XI_PRM_LIMIT_BANDWIDTH , camera_data_rate);
	xiSetParamInt(cam2, XI_PRM_LIMIT_BANDWIDTH , camera_data_rate);

	stat = xiSetParamInt(cam1, XI_PRM_IMAGE_DATA_FORMAT, XI_RAW8);
	HandleResult(stat,"xiSetParam (image format)");
	stat = xiSetParamInt(cam1, XI_PRM_IMAGE_DATA_FORMAT, XI_RAW8);
	HandleResult(stat,"xiSetParam (image format)");
	
	xiSetParamInt(cam1, XI_PRM_ACQ_TIMING_MODE, XI_ACQ_TIMING_MODE_FRAME_RATE);
	xiSetParamFloat(cam1, XI_PRM_FRAMERATE, fps);
	xiSetParamInt(cam2, XI_PRM_ACQ_TIMING_MODE, XI_ACQ_TIMING_MODE_FRAME_RATE);
	xiSetParamFloat(cam2, XI_PRM_FRAMERATE, fps);

	stat = xiSetParamInt(cam1, XI_PRM_EXPOSURE, 10000);
	HandleResult(stat,"xiSetParam (exposure set)");
	stat = xiSetParamInt(cam2, XI_PRM_EXPOSURE, 10000);
	HandleResult(stat,"xiSetParam (exposure set)");

	return XI_OK;
}

void SaveFiles(const std::string& name){
	std::string create_dir1 = "mkdir " + name + "cam1";
	std::string create_dir2 = "mkdir " + name + "cam2";
	
	const char *dir1 = create_dir1.c_str();
	const char *dir2 = create_dir2.c_str();
	
	system(dir1);
	system(dir2);

	std::string move1 = "mv cam1*.dng "+name+"cam1/";
	std::string move2 = "mv cam2*.dng "+name+"cam2/";
	
	const char *mv1 = move1.c_str();
	const char *mv2 = move2.c_str();
	
	system(mv1);
	system(mv2);

	return;
}

XI_RETURN MakeRecording(const std::string& name, 
						const int& rec_length, 
						const int& fps, 
						const HANDLE& cam1, 
						const HANDLE& cam2) {
	XI_IMG img1, img2;

	memset(&img1,0,sizeof(img1));
	memset(&img2,0,sizeof(img2));
	img1.size = sizeof(XI_IMG);
	img2.size = sizeof(XI_IMG);
	
	XI_RETURN stat;
	time_t begin, end;
	float t_diff = 0;
	struct timeb t_start, t_current;
	double seconds = 0.;
	
	int i = 0;
	time(&begin);
	ftime(&t_start);
	while(seconds < (float)rec_length){
		printf("%f\n", seconds);
		std::string fname1 = "cam1" + name + std::to_string(10000+i) + ".dng";
		std::string fname2 = "cam2" + name + std::to_string(10000+i) + ".dng";
		
		const char *c1 = fname1.c_str();
		const char *c2 = fname2.c_str();
		
		time(&t_current);
		
		if (timedelta(t_current, t_start) >= (float)(1.0/fps)){
			t_start = t_current;
			
			stat = xiGetImage(cam1, 5000, &img1);
			HandleResult(stat,"xiGetImage (img)");
			stat = xiGetImage(cam2, 5000, &img2);
			HandleResult(stat,"xiGetImage (img)");
			
			XI_DNG_METADATA metadata1, metadata2;
			xidngFillMetadataFromCameraParams(cam1, &metadata1);
			xidngFillMetadataFromCameraParams(cam2, &metadata2);
			
			stat = xidngStore(c1, &img1, &metadata1);
			stat = xidngStore(c2, &img2, &metadata2);
			
			time(&end);
	      	seconds = difftime(end, begin);
	      	++i;
      	}
	}
	return stat;
}

void LaunchRec(const std::string& name, 
			   const int& rec_length, 
			   const int& fps){
	HANDLE cam1 = NULL, cam2 = NULL;
	
	XI_RETURN stat = XI_OK;
	
	// Retrieving a handle to the camera device
	printf("Opening cameras...\n");
	
	stat = InitializeCameras(cam1, cam2, fps);
	HandleResult(stat,"InitializeCameras");
	
	std::cout << "Starting acquisition..." << std::endl;
	
	stat = xiStartAcquisition(cam1);
	HandleResult(stat,"xiStartAcquisition");
	stat = xiStartAcquisition(cam2);
	HandleResult(stat,"xiStartAcquisition");
	
	// Make a recording with n_frames
	stat = MakeRecording(name, rec_length, fps, cam1, cam2);
	
	std::cout<<"Stopping acquisition"<<std::endl;
	
	xiStopAcquisition(cam1);
	xiStopAcquisition(cam2);
	xiCloseDevice(cam1);
	xiCloseDevice(cam2);

	SaveFiles(name);
	std::cout << "Done" << std::endl;
}

void Server(){
	int server_fd, new_socket, valread; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 
    char buffer[1024] = {0}; 
    char *hello = "RECORDING BEING LAUNCHED\n";

    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){ 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT ); 

    if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    }
    printf("Welcome to remote NUC recorder at port  %d\n", PORT);

    while(1) {
        if (listen(server_fd, 3) < 0){ 
            perror("listen"); 
            exit(EXIT_FAILURE); 
        }

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                           (socklen_t*)&addrlen))<0) { 
            perror("accept"); 
            exit(EXIT_FAILURE); 
        }

        printf("ACCEPTING...\n");
        send(new_socket , "Ready\n" , 7 , 0 );
        
        valread = read(new_socket , buffer, 1024); 
        int length = 0, fps = 0;
        std::string name;
        
        MsgDecode(buffer, name, length, fps);
        
        LaunchRec(name, length, fps);
        send(new_socket , "Ready\n" , 7 , 0 );
    	//shutdown(server_fd, SHUT_RDWR);
        close(new_socket);
    }
}

int main(int argc, char* argv[]){
	Server();
	return 0;
}

