import unittest
import socket


 # ESP32 UDP 协议测试
 # 连接 192.168.0.108:82，发送 JSON 命令并验证返回结果
class MyTestCase(unittest.TestCase):
    HOST = "192.168.0.107"
    PORT = 9999

    # 发送 命令到 ESP32
    def send_cmd(self, payload: str):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(3)

            request = payload.encode("utf-8")
            sock.sendto(request, (self.HOST, self.PORT))


    # 测试自定义颜色位置
    def test_custom(self):
        effect = []

        import random
        import time

        # 模拟音乐频谱动态效果
        for _ in range(100):
            effect.clear()

            # 单向频谱（从左往右）
            level = random.randint(0, 16)

            for i in range(level):
                # 彩虹渐变频谱
                if i < 3:
                    color = (0, 255, 0)          # 绿
                elif i < 6:
                    color = (0, 255, 255)        # 青
                elif i < 9:
                    color = (0, 0, 255)          # 蓝
                elif i < 12:
                    color = (255, 0, 255)        # 紫
                elif i < 14:
                    color = (255, 255, 0)        # 黄
                else:
                    color = (255, 0, 0)          # 红

                r, g, b = color
                effect.append(f"{i}:{r}:{g}:{b}")

            payload = "120,8,2," + ";".join(effect)
            self.send_cmd(payload)
            time.sleep(0.15)






if __name__ == '__main__':
    unittest.main()
