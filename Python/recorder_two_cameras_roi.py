
import cv2
from ximea import xiapi
import os
import socket
import datetime
import sys
import time

HOST = "0.0.0.0"           
PORT = 50009
CAMERAS_ON_SAME_CONTROLLER = 2

cam1 = xiapi.Camera(dev_id=0)
cam2 = xiapi.Camera(dev_id=1)

def PrintOut(conn, message):
    msg = message + "\n"
    conn.send(msg.encode())
    print(message)

def MsgDecode(data):
    task = data.decode().split()
    name = task[0]
    rec_length = int(task[1])
    if len(task) == 3:
        fps = int(task[2])
    else:
        fps = 30
    
    return name, rec_length, fps

def WriteFiles(name, counter, img1, img2):
    name1 = name + "_cam1"
    name2 = name + "_cam2"
    cv2.imwrite(name1+'/IMG'+str(100000 + counter)+'.png', 
                        img1.get_image_data_numpy())
    cv2.imwrite(name2+'/IMG'+str(100000 + counter)+'.png', 
                        img2.get_image_data_numpy())
    return 0

def MakeRecording(name, rec_length, fps=30):
    cam1.open_device()
    cam2.open_device()
    
    PrintOut(connection, 'RECORDING BEING LAUNCHED')
    #set interface data rate
    interface_data_rate=cam1.get_limit_bandwidth();
    camera_data_rate = int(interface_data_rate / CAMERAS_ON_SAME_CONTROLLER) ;

    #set data rate
    cam1.set_limit_bandwidth(camera_data_rate);
    cam2.set_limit_bandwidth(camera_data_rate);
    
    cam1.set_imgdataformat('XI_RAW8')
    cam2.set_imgdataformat('XI_RAW8');
    
    cam1.set_exposure(10000)
    cam2.set_exposure(15000)
    
    offsetX = 512
    
    cam1.set_param("downsampling", "XI_DWN_2x2")
    cam2.set_param("downsampling", "XI_DWN_2x2")
    #cam1.set_param('XI_PRM_OFFSET_X', offsetX)
    #cam2.set_param('XI_PRM_OFFSET_X', offsetX)

    #cam1.set_param('XI_PRM_OFFSET_Y', 500)
    #cam2.set_param('XI_PRM_OFFSET_Y', 500)
    width = 256
    #cam1.set_param('XI_PRM_WIDTH', width)
    #cam2.set_param('XI_PRM_WIDTH', width)

    #cam1.set_param('XI_PRM_HEIGHT', 300)
    #cam2.set_param('XI_PRM_HEIGHT', 300)

    img1 = xiapi.Image()
    img2 = xiapi.Image()

    #cam.set_param('aeag', True)
    #time.sleep(1)
    
    cam1.start_acquisition()
    cam2.start_acquisition()                    
    
    print(name, " ", rec_length)
                        
    d = datetime.datetime.now()
                     
    time_prev = time.time()
    
    try:
        name1 = name + "_cam1"
        name2 = name + "_cam2"
        os.mkdir(name1)
        os.mkdir(name2)
    except:
        print("Unable to create folders. Recording aborted")
        return -1
    N = 0
    while (datetime.datetime.now() - d).total_seconds() < rec_length:
        if (time.time() - time_prev > 1./fps):
            time_prev = time.time()
            
            cam1.get_image(img1)
            cam2.get_image(img2)
            
            if WriteFiles(name, N, img1, img2) == 0:
                N += 1
            else:
                PrintOut("Failed")
                return -1
    PrintOut(connection, "Recording finished")
    cam1.stop_acquisition()
    cam2.stop_acquisition()
    
    cam1.close_device()
    cam2.close_device()
    return 0
    

if __name__ == "__main__":
    
    if len(sys.argv) > 1:
        PORT = int(sys.argv[1])

    print("Welcome to remote NUC recorder at port", PORT)
    
    server_info = (HOST, PORT)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(server_info)
    sock.listen(5)

    
    try:
        while True:
            connection, client_address = sock.accept()
            connection.send(b"Ready\n")
            
            try:
                data = connection.recv(1024)
                print('message received:' , data.decode())
                    
                if (len(data.decode()) != 0):
                    
                    name, rec_length, fps = MsgDecode(data)

                    if MakeRecording(name, rec_length, fps) == 0:
                        PrintOut(connection, "Ready")
                    else:
                        PrintOut(connection, "Failed")
                        
                    
            except Exception as e:
                PrintOut(connection, "INVALID TASK FORMAT. TRY AGAIN")
                print(e)
                
            finally:
                connection.close()
                #sock.close()
                
    finally:
        print('closing socket')
        sock.close()
