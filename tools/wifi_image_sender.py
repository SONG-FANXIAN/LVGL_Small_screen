#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32 WiFi 照片上传工具
通过 HTTP 将 RGB565 图像上传到 ESP32 的 WiFi AP
支持 JPG/PNG → RGB565 自动转换，也支持直接上传 .rgb565 原始文件
"""

import os
import sys
import struct
import argparse
import http.client
import urllib.parse
from pathlib import Path

# 默认配置
DEFAULT_ESP32_IP = "192.168.4.1"
DEFAULT_PORT = 80
IMAGE_WIDTH = 240
IMAGE_HEIGHT = 240
IMAGE_SIZE = IMAGE_WIDTH * IMAGE_HEIGHT * 2  # 115200 bytes


def convert_image_to_rgb565(image_path):
    """
    将任意图像转换为 240x240 RGB565 字节数据。
    需要 PIL/Pillow 库。
    """
    try:
        from PIL import Image
    except ImportError:
        print("[!] 需要安装 Pillow 库: pip install Pillow")
        sys.exit(1)

    print(f"[*] 正在转换图像: {image_path}")
    img = Image.open(image_path).convert("RGB")
    img = img.resize((IMAGE_WIDTH, IMAGE_HEIGHT), Image.LANCZOS)

    pixels = img.load()
    data = bytearray(IMAGE_SIZE)
    idx = 0
    for y in range(IMAGE_HEIGHT):
        for x in range(IMAGE_WIDTH):
            r, g, b = pixels[x, y]
            # RGB888 → RGB565
            r5 = (r * 31) // 255
            g6 = (g * 63) // 255
            b5 = (b * 31) // 255
            rgb565 = (r5 << 11) | (g6 << 5) | b5
            # 小端序存储 (ESP32 期望)
            data[idx] = rgb565 & 0xFF
            data[idx + 1] = (rgb565 >> 8) & 0xFF
            idx += 2

    print(f"[✓] 转换完成: {IMAGE_WIDTH}x{IMAGE_HEIGHT} RGB565, {IMAGE_SIZE} 字节")
    return bytes(data)


def upload_raw_rgb565(data, esp_ip, port):
    """上传原始 RGB565 字节数据到 ESP32"""
    print(f"[*] 连接到 ESP32: http://{esp_ip}:{port}")
    print(f"[*] 上传数据: {len(data)} 字节")

    conn = http.client.HTTPConnection(esp_ip, port, timeout=30)
    try:
        conn.request(
            "POST",
            "/upload",
            body=data,
            headers={
                "Content-Type": "application/octet-stream",
                "Content-Length": str(len(data)),
            },
        )
        response = conn.getresponse()
        result = response.read().decode("utf-8", errors="ignore")
        print(f"[*] 服务器响应 ({response.status}): {result}")

        if response.status == 200:
            print("\n[✓] 照片上传成功！屏幕将自动刷新显示新照片。")
            return True
        else:
            print(f"\n[!] 上传失败 (HTTP {response.status})")
            return False
    except Exception as e:
        print(f"\n[!] 连接失败: {e}")
        print("[*] 请确认:")
        print("    1. ESP32 已上电并运行")
        print(f"    2. 已连接到 WiFi: ESP32_Photo")
        print(f"    3. ESP32 IP 地址正确: {esp_ip}")
        return False
    finally:
        conn.close()


def main():
    parser = argparse.ArgumentParser(
        description="ESP32 WiFi 照片上传工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 上传 JPG/PNG 图像（自动转换为 RGB565）
  python wifi_image_sender.py photo.jpg

  # 上传已转换的 RGB565 原始文件
  python wifi_image_sender.py --raw photo.rgb565

  # 指定 ESP32 IP 地址
  python wifi_image_sender.py --ip 192.168.4.1 photo.jpg

  # 使用 ota_tool.py 打包后再上传
  python ota_tool.py --convert photo.jpg --output photo.rgb565
  python ota_tool.py --pack photo.rgb565 --output photo.imgpkg
  python wifi_image_sender.py --from-ota photo.imgpkg
        """,
    )

    parser.add_argument("image", nargs="?", help="图像文件路径 (JPG/PNG/BMP 等)")
    parser.add_argument("--raw", action="store_true", help="直接上传 RGB565 原始文件（不转换）")
    parser.add_argument("--from-ota", metavar="OTA_FILE", help="从 OTA 包提取图像数据上传")
    parser.add_argument("--ip", default=DEFAULT_ESP32_IP, help=f"ESP32 IP 地址 (默认: {DEFAULT_ESP32_IP})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"HTTP 端口 (默认: {DEFAULT_PORT})")
    parser.add_argument("--output", "-o", metavar="FILE", help="转换后保存为 RGB565 文件（不上传）")

    args = parser.parse_args()

    # 处理 --from-ota
    if args.from_ota:
        if not os.path.exists(args.from_ota):
            print(f"[!] OTA 文件不存在: {args.from_ota}")
            sys.exit(1)
        with open(args.from_ota, "rb") as f:
            ota_data = f.read()
        # OTA包格式: 16字节头 + 图像数据
        if len(ota_data) < 16:
            print("[!] 无效的 OTA 包")
            sys.exit(1)
        magic = struct.unpack("<I", ota_data[0:4])[0]
        if magic != 0xAABB5650:
            print(f"[!] 不是图像 OTA 包 (魔数: 0x{magic:08X})")
            sys.exit(1)
        img_data = ota_data[16:]
        print(f"[*] 从 OTA 包提取图像: {len(img_data)} 字节")

        if args.output:
            with open(args.output, "wb") as f:
                f.write(img_data)
            print(f"[✓] 已保存: {args.output}")
        else:
            upload_raw_rgb565(img_data, args.ip, args.port)
        return

    # 需要图像文件
    if not args.image:
        parser.print_help()
        sys.exit(1)

    if not os.path.exists(args.image):
        print(f"[!] 文件不存在: {args.image}")
        sys.exit(1)

    # 加载图像数据
    if args.raw:
        with open(args.image, "rb") as f:
            img_data = f.read()
        if len(img_data) != IMAGE_SIZE:
            print(f"[!] RGB565 文件大小错误: {len(img_data)} != {IMAGE_SIZE}")
            print(f"    期望 240×240 RGB565 原始文件 ({IMAGE_SIZE} 字节)")
            sys.exit(1)
        print(f"[*] 加载 RGB565 原始文件: {len(img_data)} 字节")
    else:
        img_data = convert_image_to_rgb565(args.image)

    if args.output:
        with open(args.output, "wb") as f:
            f.write(img_data)
        print(f"[✓] 已保存: {args.output}")
    else:
        upload_raw_rgb565(img_data, args.ip, args.port)


if __name__ == "__main__":
    main()
