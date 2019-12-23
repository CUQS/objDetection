import socket
import numpy as np
import cv2
import time

class Server:
    buf_size = 800*600*3
    start_time = 0
    read_num = 0
    fps = 0
    
    def run(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(('192.168.1.134', 4097))
        server.listen()
        print("wait connect...")
        conn, addr = server.accept()
        print("connected, ", addr)
        print("wait info...")
        cv2.namedWindow("img", cv2.WINDOW_NORMAL)
        cv2.resizeWindow("img", 800, 600)
        font = cv2.FONT_HERSHEY_SIMPLEX
        count = 200
        while count:
            stringData = self.recv_size(conn, self.buf_size)
            self.read_num += 1
            if count==200:
                self.start_time = time.time()
            if (time.time() - self.start_time) > 1 :
                if self.fps==0:
                    self.fps = (self.read_num - 1) / (time.time() - self.start_time)
                else:
                    self.fps = self.read_num / (time.time() - self.start_time)
                self.read_num = 0
                self.start_time = time.time()
            # data convert
            data = np.frombuffer(stringData, np.uint8)
            data = data.reshape(600,800,3)
            msg_num = "{:3d}/200".format((200-count))
            msg_fps = "FPS: {:>4.2f}".format(self.fps)
            img = cv2.putText(data, msg_num, (530, 15), font, 0.5, (0, 0, 255), 1)
            img = cv2.putText(img, msg_fps, (530, 30), font, 0.5, (0, 0, 255), 1)
            cv2.imshow("img", data)
            cv2.waitKey(10)
            count -= 1

        conn.close()
        server.close()
        print("closed")

    def recv_size(self, sokt, size):
        buf = b""
        while size:
            newbuf = sokt.recv(size)
            if not newbuf:
                return None
            buf += newbuf
            size -= len(newbuf)
        return buf

if __name__=="__main__":
    server = Server()
    server.run()
