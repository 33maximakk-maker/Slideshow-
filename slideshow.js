// slideshow.js
#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const sharp = require('sharp');
const readline = require('readline');
const { execSync } = require('child_process');

const COLORS = {
    reset: '\x1b[0m',
    bold: '\x1b[1m',
    green: '\x1b[92m',
    yellow: '\x1b[93m',
    blue: '\x1b[94m'
};

function rgbToAnsi(r, g, b) {
    return `\x1b[38;2;${r};${g};${b}m`;
}

function getImageFiles(paths, recursive) {
    const exts = ['.jpg', '.jpeg', '.png', '.gif', '.bmp', '.tiff', '.webp'];
    let files = [];
    for (const p of paths) {
        if (fs.statSync(p).isFile()) {
            files.push(p);
        } else if (fs.statSync(p).isDirectory()) {
            const walk = (dir) => {
                const entries = fs.readdirSync(dir);
                for (const e of entries) {
                    const full = path.join(dir, e);
                    const stat = fs.statSync(full);
                    if (stat.isDirectory() && recursive) {
                        walk(full);
                    } else if (stat.isFile() && exts.includes(path.extname(full).toLowerCase())) {
                        files.push(full);
                    }
                }
            };
            walk(p);
        }
    }
    return files.sort();
}

async function displayImage(imgPath, name, idx, total, delay, paused, shuffle, asciiMode, termW, termH) {
    console.clear();
    const meta = await sharp(imgPath).metadata();
    let w = meta.width, h = meta.height;
    const maxW = termW - 2;
    const maxH = termH - 8;
    const scaleX = maxW / w;
    const scaleY = maxH / h;
    let scale = Math.min(scaleX, scaleY, 1);
    const newW = Math.max(1, Math.round(w * scale));
    const newH = Math.max(1, Math.round(h * scale));

    const buffer = await sharp(imgPath)
        .resize(newW, newH, { kernel: sharp.kernel.lanczos2 })
        .raw()
        .toBuffer({ resolveWithObject: true });

    const data = buffer.data;
    const sw = buffer.info.width;
    const sh = buffer.info.height;

    let info = `${COLORS.bold}📸 ${name}  |  ${idx+1}/${total}  |  Задержка: ${delay}с${COLORS.reset}`;
    if (paused) info += ` ${COLORS.yellow}[ПАУЗА]${COLORS.reset}`;
    if (shuffle) info += ` ${COLORS.blue}[СЛУЧ]${COLORS.reset}`;
    console.log(info);
    console.log('Управление: ← → (нав)  Space (пауза)  +/- (скорость)  s (случ)  f (режим)  r (рестарт)  q (выход)');

    if (asciiMode) {
        const chars = ' .:-=+*#%@';
        for (let y = 0; y < sh; y++) {
            let line = '';
            for (let x = 0; x < sw; x++) {
                const idx2 = (y * sw + x) * 3;
                const r = data[idx2], g = data[idx2+1], b = data[idx2+2];
                const gray = 0.299 * r + 0.587 * g + 0.114 * b;
                const ci = Math.floor((gray / 255) * (chars.length - 1));
                line += chars[ci];
            }
            console.log(line);
        }
    } else {
        for (let y = 0; y < sh; y++) {
            let line = '';
            for (let x = 0; x < sw; x++) {
                const idx2 = (y * sw + x) * 3;
                const r = data[idx2], g = data[idx2+1], b = data[idx2+2];
                line += rgbToAnsi(r, g, b) + '██';
            }
            line += COLORS.reset;
            console.log(line);
        }
    }
}

function getKey() {
    return new Promise(resolve => {
        const rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout,
            terminal: true
        });
        process.stdin.setRawMode(true);
        process.stdin.once('data', (data) => {
            process.stdin.setRawMode(false);
            rl.close();
            resolve(data.toString());
        });
    });
}

async function main() {
    const args = process.argv.slice(2);
    let paths = [];
    let delay = 3;
    let shuffle = false;
    let asciiMode = false;
    let recursive = false;

    for (let i = 0; i < args.length; i++) {
        if (args[i] === '-d' && i+1 < args.length) {
            delay = parseInt(args[++i], 10);
        } else if (args[i] === '-s') {
            shuffle = true;
        } else if (args[i] === '-a') {
            asciiMode = true;
        } else if (args[i] === '-r') {
            recursive = true;
        } else if (args[i] === '-h' || args[i] === '--help') {
            console.log('Usage: node slideshow.js <path> [options]\n' +
                '  -d <sec>   Delay (default 3)\n' +
                '  -s         Shuffle\n' +
                '  -a         ASCII mode\n' +
                '  -r         Recursive');
            process.exit(0);
        } else {
            paths.push(args[i]);
        }
    }
    if (paths.length === 0) {
        console.log('Укажите папку или файлы.');
        process.exit(1);
    }

    let files = getImageFiles(paths, recursive);
    if (files.length === 0) {
        console.log('Нет изображений.');
        process.exit(1);
    }
    if (shuffle) {
        for (let i = files.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [files[i], files[j]] = [files[j], files[i]];
        }
    }
    const total = files.length;
    let idx = 0;
    let paused = false;
    const termW = process.stdout.columns || 80;
    const termH = process.stdout.rows || 24;

    while (true) {
        const name = path.basename(files[idx]);
        await displayImage(files[idx], name, idx, total, delay, paused, shuffle, asciiMode, termW, termH);
        const start = Date.now();
        let key = '';
        while (Date.now() - start < delay * 1000 && !paused) {
            const k = await getKey();
            if (k) { key = k; break; }
            await new Promise(r => setTimeout(r, 100));
        }
        if (key === 'q') break;
        else if (key === ' ') paused = !paused;
        else if (key === '+') delay = Math.min(10, delay + 1);
        else if (key === '-') delay = Math.max(1, delay - 1);
        else if (key === 's') {
            shuffle = !shuffle;
            if (shuffle) {
                for (let i = files.length - 1; i > 0; i--) {
                    const j = Math.floor(Math.random() * (i + 1));
                    [files[i], files[j]] = [files[j], files[i]];
                }
                idx = 0;
            }
        } else if (key === 'f') asciiMode = !asciiMode;
        else if (key === 'r') idx = 0;
        else if (key === '\x1b') {
            const seq = await getKey();
            if (seq === '[D') idx = (idx - 1 + total) % total;
            else if (seq === '[C') idx = (idx + 1) % total;
        } else if (!key && !paused) {
            idx = (idx + 1) % total;
        }
    }
    console.log(`${COLORS.green}\n✅ Слайд-шоу завершено.${COLORS.reset}`);
}

main().catch(console.error);
