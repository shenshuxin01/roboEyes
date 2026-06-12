import time
import unittest
import json
import socket


 # ESP32 TCP JSON 协议测试
 # 连接 192.168.0.108:82，发送 JSON 命令并验证返回结果
class MyTestCase(unittest.TestCase):
    HOST = "192.168.0.108"
    PORT = 82

    # 发送 JSON 命令到 ESP32，并解析返回的 JSON 响应
    def send_cmd(self, payload: dict):
        with socket.create_connection((self.HOST, self.PORT), timeout=3) as sock:
            # Arduino 服务端使用 readStringUntil('\n') 读取请求
            # 因此这里需要追加换行符
            request = json.dumps(payload) + "\n"
            sock.sendall(request.encode("utf-8"))

            # 持续读取响应，直到服务端关闭连接
            response = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk

            return json.loads(response.decode("utf-8").strip())

    # 测试开启数码管显示
    def test_display_enable(self):
        resp = self.send_cmd({"cmd": "displayEnable"})
        self.assertTrue(resp["success"])

    # 测试关闭数码管显示
    def test_display_disable(self):
        resp = self.send_cmd({"cmd": "displayDisable"})
        self.assertTrue(resp["success"])

    # 测试显示数字和小数点位置
    def test_set_number(self):
        resp = self.send_cmd({
            "cmd": "setNumber",
            "value": 1234,
            "dotPos": 1
        })
        self.assertTrue(resp["success"])

    # 测试切换到网络时间显示模式
    def test_display_time(self):
        resp = self.send_cmd({"cmd": "displayTime"})
        self.assertTrue(resp["success"])

    # 测试唤醒 ST7789 屏幕
    def test_screen_wakeup(self):
        resp = self.send_cmd({"cmd": "screenWakeup"})
        self.assertTrue(resp["success"])

    # 测试关闭 ST7789 屏幕
    def test_screen_sleep(self):
        resp = self.send_cmd({"cmd": "screenSleep"})
        self.assertTrue(resp["success"])

    # 测试获取摇杆方向
    #     RIGHT=1,LEFT=2,BOTTOM=3,TOP=4,DOWN=5,NONE=0
    def test_get_direction(self):
        for _ in range(5):
            time.sleep(1)
            resp = self.send_cmd({"cmd": "getDirection"})

            self.assertTrue(resp["success"])
            self.assertIn("direction", resp)
            print(resp)


if __name__ == '__main__':
    unittest.main()
