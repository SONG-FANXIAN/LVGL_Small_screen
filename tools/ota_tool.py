#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32 WiFi 照片工具
支持 RGB565 图像转换、打包和验证（配合 wifi_image_sender.py 使用）
"""

import os
import sys
import struct
import zlib
import argparse
from pathlib import Path

# 图像规格
IMAGE_WIDTH  = 240
IMAGE_HEIGHT = 240
IMAGE_SIZE   = IMAGE_WIDTH * IMAGE_HEIGHT * 2  # 115200 bytes

# 图像包头魔数
IMG_MAGIC    = 0xAABB5650
HEADER_SIZE  = 16


def calculate_crc32(data):
    """计算数据的CRC32校验值"""
    return zlib.crc32(data) & 0xffffffff


def convert_jpg_to_rgb565(jpg_path, output_path):
    """
    将 JPG/PNG 等图像转换为 240x240 RGB565 原始文件。
    需要 PIL/Pillow 库。
    """
    try:
        from PIL import Image
    except ImportError:
        print("[!] 需要安装 Pillow 库: pip install Pillow")
        sys.exit(1)

    print(f"[*] 加载图像: {jpg_path}")
    img = Image.open(jpg_path).convert("RGB")
    img = img.resize((IMAGE_WIDTH, IMAGE_HEIGHT), Image.LANCZOS)

    pixels = img.load()
    data = bytearray(IMAGE_SIZE)
    idx = 0
    for y in range(IMAGE_HEIGHT):
        for x in range(IMAGE_WIDTH):
            r, g, b = pixels[x, y]
            r5 = (r * 31) // 255
            g6 = (g * 63) // 255
            b5 = (b * 31) // 255
            rgb565 = (r5 << 11) | (g6 << 5) | b5
            data[idx] = rgb565 & 0xFF
            data[idx + 1] = (rgb565 >> 8) & 0xFF
            idx += 2

    with open(output_path, "wb") as f:
        f.write(data)

    print(f"[✓] 转换完成!")
    print(f"    输出: {output_path}")
    print(f"    尺寸: {IMAGE_WIDTH}×{IMAGE_HEIGHT} RGB565")
    print(f"    大小: {IMAGE_SIZE} 字节")
    print(f"    CRC32: 0x{calculate_crc32(bytes(data)):08X}")
    return output_path


def pack_image(rgb565_path, output_path):
    """将 RGB565 原始文件打包为带校验头的图像包"""
    with open(rgb565_path, "rb") as f:
        raw = f.read()

    if len(raw) != IMAGE_SIZE:
        print(f"[!] 图像大小错误: {len(raw)} != {IMAGE_SIZE}")
        sys.exit(1)

    crc32 = calculate_crc32(raw)
    header = struct.pack("<IIII", IMG_MAGIC, 0, IMAGE_SIZE, crc32)
    package = header + raw

    with open(output_path, "wb") as f:
        f.write(package)

    print(f"[✓] 图像包生成成功!")
    print(f"    魔数:   0x{IMG_MAGIC:08X}")
    print(f"    大小:   {IMAGE_SIZE} 字节")
    print(f"    CRC32:  0x{crc32:08X}")
    print(f"    输出:   {output_path}")
    print(f"    总大小: {len(package)} 字节")
    return output_path


def verify_package(package_path):
    """验证图像包"""
    with open(package_path, "rb") as f:
        data = f.read()

    if len(data) < HEADER_SIZE:
        print("[!] 包太小，无法解析")
        return False

    magic, version, size, crc32_stored = struct.unpack("<IIII", data[:HEADER_SIZE])

    if magic != IMG_MAGIC:
        print(f"[!] 无效魔数: 0x{magic:08X}, 期望: 0x{IMG_MAGIC:08X}")
        return False

    print(f"[*] 图像包信息:")
    print(f"    魔数:   0x{magic:08X}")
    print(f"    版本:   {version}")
    print(f"    大小:   {size}")
    print(f"    存储CRC32: 0x{crc32_stored:08X}")

    payload = data[HEADER_SIZE:]
    if len(payload) != size:
        print(f"[!] 大小不匹配: {len(payload)} != {size}")
        return False

    crc32_calc = calculate_crc32(payload)
    print(f"    计算CRC32: 0x{crc32_calc:08X}")

    if crc32_calc != crc32_stored:
        print("[!] CRC32 校验失败!")
        return False

    print("[✓] 图像包验证通过!")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="ESP32 WiFi 照片工具 - RGB565 图像转换/打包/验证",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 将 JPG 转换为 RGB565 原始文件
  python ota_tool.py --convert photo.jpg --output photo.rgb565

  # 将 RGB565 原始文件打包
  python ota_tool.py --pack photo.rgb565 --output photo.imgpkg

  # 验证图像包
  python ota_tool.py --verify photo.imgpkg

  # 一步转换+打包
  python ota_tool.py --convert photo.jpg --pack --output photo.imgpkg

  # 然后使用 wifi_image_sender.py 上传:
  python wifi_image_sender.py --raw photo.rgb565
  python wifi_image_sender.py --from-ota photo.imgpkg
        """,
    )

    parser.add_argument("--convert", "-c", help="JPG/PNG 图像路径（转换为 RGB565）")
    parser.add_argument("--pack", "-p", nargs="?", const="__AUTO__", help="打包 RGB565 文件")
    parser.add_argument("--verify", "-v", help="验证图像包")
    parser.add_argument("--output", "-o", help="输出文件路径")

    args = parser.parse_args()

    if args.convert:
        if not os.path.exists(args.convert):
            print(f"[!] 文件不存在: {args.convert}")
            sys.exit(1)
        out = args.output or "output.rgb565"
        convert_jpg_to_rgb565(args.convert, out)

        if args.pack:
            pkg_out = args.pack if args.pack != "__AUTO__" else out + ".imgpkg"
            pack_image(out, pkg_out)

    elif args.pack:
        pkg_input = args.pack if args.pack != "__AUTO__" else None
        if not pkg_input:
            print("[!] 请指定要打包的文件: --pack photo.rgb565")
            sys.exit(1)
        if not os.path.exists(pkg_input):
            print(f"[!] 文件不存在: {pkg_input}")
            sys.exit(1)
        out = args.output or pkg_input + ".imgpkg"
        pack_image(pkg_input, out)

    elif args.verify:
        if not os.path.exists(args.verify):
            print(f"[!] 文件不存在: {args.verify}")
            sys.exit(1)
        if not verify_package(args.verify):
            sys.exit(1)

    else:
        parser.print_help()


if __name__ == "__main__":
    main()
