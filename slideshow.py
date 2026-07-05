# slideshow.py
#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import time
import random
import argparse
import threading
from PIL import Image
import platform

# ANSI-цвета
COLORS = {
    'reset': '\033[0m',
    'bold': '\033[1m',
    'green': '\033[92m',
    'yellow': '\033[93m',
    'blue': '\033[94m'
}

def rgb_to_ansi(r, g, b):
    return f'\033[38;2;{r};{g};{b}m'

def get_terminal_size():
    try:
        import shutil
        cols, rows = shutil.get_terminal_size()
        return cols, rows
    except:
        return 80, 24

class Slideshow:
    def __init__(self, files, delay=3, shuffle=False, ascii_mode=False, recursive=False):
        self.files = []
        for f in files:
            if os.path.isdir(f) and recursive:
                self.files.extend(self.walk_dir(f))
            elif os.path.isfile(f):
                self.files.append(f)
            elif os.path.isdir(f):
                self.files.extend([os.path.join(f, x) for x in os.listdir(f) if self.is_image(x)])
        self.files = [f for f in self.files if self.is_image(f)]
        self.delay = delay
        self.shuffle = shuffle
        self.ascii_mode = ascii_mode
        self.index = 0
        self.paused = False
        self.running = True
        self.term_w, self.term_h = get_terminal_size()
        self.total = len(self.files)
        self.current_image = None
        if shuffle:
            random.shuffle(self.files)
        self.lock = threading.Lock()

    def is_image(self, filename):
        exts = ('.jpg', '.jpeg', '.png', '.gif', '.bmp', '.tiff', '.webp')
        return filename.lower().endswith(exts)

    def walk_dir(self, path):
        result = []
        for root, dirs, files in os.walk(path):
            for f in files:
                if self.is_image(f):
                    result.append(os.path.join(root, f))
        return result

    def load_image(self, idx):
        try:
            img = Image.open(self.files[idx]).convert('RGB')
            return img
        except Exception as e:
            print(f"Ошибка загрузки {self.files[idx]}: {e}", file=sys.stderr)
            return None

    def scale_image(self, img):
        w, h = img.size
        max_w = self.term_w - 2
        max_h = self.term_h - 8
        scale_x = max_w / w
        scale_y = max_h / h
        scale = min(scale_x, scale_y, 1.0)
        new_w = int(w * scale)
        new_h = int(h * scale)
        if new_w < 1: new_w = 1
        if new_h < 1: new_h = 1
        return img.resize((new_w, new_h), Image.Resampling.LANCZOS)

    def to_ascii(self, img):
        chars = [' ', '.', ':', '-', '=', '+', '*', '#', '%', '@']
        gray = img.convert('L')
        pixels = gray.getdata()
        w, h = img.size
        lines = []
        for i in range(h):
            row = ''.join(chars[int(pixels[i*w+j] / 255 * (len(chars)-1))] for j in range(w))
            lines.append(row)
        return lines

    def to_color_blocks(self, img):
        pixels = img.getdata()
        w, h = img.size
        lines = []
        for i in range(h):
            row = []
            for j in range(w):
                r, g, b = pixels[i*w + j]
                row.append(rgb_to_ansi(r, g, b) + '██')
            row.append(COLORS['reset'])
            lines.append(''.join(row))
        return lines

    def display(self, img):
        os.system('clear' if platform.system() != 'Windows' else 'cls')
        scaled = self.scale_image(img)
        name = os.path.basename(self.files[self.index])
        info = f"{COLORS['bold']}📸 {name}  |  {self.index+1}/{self.total}  |  Задержка: {self.delay}с{COLORS['reset']}"
        if self.paused:
            info += f"  {COLORS['yellow']}[ПАУЗА]{COLORS['reset']}"
        if self.shuffle:
            info += f"  {COLORS['blue']}[СЛУЧ]{COLORS['reset']}"
        print(info)
        print(f"Управление: ← → (нав)  Space (пауза)  +/- (скорость)  s (случ)  f (режим)  r (рестарт)  q (выход)")

        if self.ascii_mode:
            lines = self.to_ascii(scaled)
            for line in lines:
                print(line)
        else:
            lines = self.to_color_blocks(scaled)
            for line in lines:
                print(line)

    def next_slide(self):
        self.index = (self.index + 1) % self.total

    def prev_slide(self):
        self.index = (self.index - 1) % self.total

    def restart(self):
        self.index = 0

    def toggle_pause(self):
        self.paused = not self.paused

    def toggle_shuffle(self):
        self.shuffle = not self.shuffle
        if self.shuffle:
            random.shuffle(self.files)
            self.index = 0

    def toggle_mode(self):
        self.ascii_mode = not self.ascii_mode

    def run(self):
        import termios
        import tty
        import sys
        import select

        def getch():
            fd = sys.stdin.fileno()
            old = termios.tcgetattr(fd)
            try:
                tty.setraw(fd)
                ch = sys.stdin.read(1)
                if ch == '\x1b':
                    ch2 = sys.stdin.read(2)
                    if ch2 == '[A': return 'up'
                    elif ch2 == '[B': return 'down'
                    elif ch2 == '[D': return 'left'
                    elif ch2 == '[C': return 'right'
                    else: return ch + ch2
                return ch
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old)

        while self.running and self.total > 0:
            img = self.load_image(self.index)
            if img is None:
                self.next_slide()
                continue
            self.display(img)

            start_time = time.time()
            key = None
            while time.time() - start_time < self.delay and not self.paused:
                if select.select([sys.stdin], [], [], 0.1)[0]:
                    key = getch()
                    break
                time.sleep(0.05)

            if key is None and not self.paused:
                self.next_slide()
                continue

            if key == 'q':
                self.running = False
                break
            elif key == 'left':
                self.prev_slide()
            elif key == 'right':
                self.next_slide()
            elif key == ' ':
                self.toggle_pause()
            elif key == '+':
                self.delay = min(10, self.delay + 1)
            elif key == '-':
                self.delay = max(1, self.delay - 1)
            elif key == 's':
                self.toggle_shuffle()
            elif key == 'f':
                self.toggle_mode()
            elif key == 'r':
                self.restart()

        print(COLORS['green'] + "\n✅ Слайд-шоу завершено." + COLORS['reset'])

def main():
    parser = argparse.ArgumentParser(description="Slideshow – слайд-шоу в терминале")
    parser.add_argument('paths', nargs='+', help='Папки или файлы с изображениями')
    parser.add_argument('-d', '--delay', type=int, default=3, help='Задержка между слайдами (сек)')
    parser.add_argument('-s', '--shuffle', action='store_true', help='Случайный порядок')
    parser.add_argument('-a', '--ascii', action='store_true', help='ASCII-режим')
    parser.add_argument('-r', '--recursive', action='store_true', help='Рекурсивный обход папок')
    args = parser.parse_args()

    slideshow = Slideshow(args.paths, args.delay, args.shuffle, args.ascii, args.recursive)
    if slideshow.total == 0:
        print("Нет изображений для показа.", file=sys.stderr)
        sys.exit(1)

    try:
        slideshow.run()
    except KeyboardInterrupt:
        print("\nВыход.")
        sys.exit(0)

if __name__ == '__main__':
    main()
