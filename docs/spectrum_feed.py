#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""频谱 UI 测试源：向 stdout 输出 @AUDIO T 正弦扫频 PCM（模拟 TTS 音频）。"""
import base64
import math
import struct
import time

RATE = 24000
CHUNK = 2400  # 100ms
FREQS = [200, 500, 1000, 2000, 3000, 4500, 6000]
index = 0
phase = 0.0

while True:
    freq = FREQS[index % len(FREQS)]
    samples = []
    for _ in range(CHUNK):
        samples.append(int(9000 * math.sin(phase)))
        phase += 2.0 * math.pi * freq / RATE
    if phase > 2.0 * math.pi:
        phase -= 2.0 * math.pi
    data = struct.pack("<%dh" % len(samples), *samples)
    print("\n@AUDIO T " + base64.b64encode(data).decode("ascii"), flush=True)
    index += 1
    time.sleep(0.1)
