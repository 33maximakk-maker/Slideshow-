// slideshow.go
package main

import (
	"bufio"
	"flag"
	"fmt"
	"image"
	"image/color"
	_ "image/gif"
	_ "image/jpeg"
	_ "image/png"
	"math/rand"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"github.com/disintegration/imaging"
	"golang.org/x/term"
)

const (
	reset  = "\033[0m"
	bold   = "\033[1m"
	green  = "\033[92m"
	yellow = "\033[93m"
	blue   = "\033[94m"
)

func rgbToAnsi(r, g, b uint8) string {
	return fmt.Sprintf("\033[38;2;%d;%d;%dm", r, g, b)
}

func getImageFiles(paths []string, recursive bool) []string {
	var files []string
	exts := map[string]bool{".jpg": true, ".jpeg": true, ".png": true, ".gif": true, ".bmp": true, ".tiff": true, ".webp": true}
	for _, p := range paths {
		info, err := os.Stat(p)
		if err != nil {
			continue
		}
		if !info.IsDir() {
			files = append(files, p)
		} else if recursive {
			filepath.Walk(p, func(path string, info os.FileInfo, err error) error {
				if err != nil {
					return nil
				}
				if !info.IsDir() && exts[strings.ToLower(filepath.Ext(path))] {
					files = append(files, path)
				}
				return nil
			})
		} else {
			entries, _ := os.ReadDir(p)
			for _, e := range entries {
				if !e.IsDir() && exts[strings.ToLower(filepath.Ext(e.Name()))] {
					files = append(files, filepath.Join(p, e.Name()))
				}
			}
		}
	}
	sort.Strings(files)
	return files
}

func displayImage(img image.Image, name string, idx, total, delay int, paused, shuffle, asciiMode bool, termW, termH int) {
	fmt.Print("\033[H\033[2J")
	bounds := img.Bounds()
	w, h := bounds.Dx(), bounds.Dy()
	maxW := termW - 2
	maxH := termH - 8
	scaleX := float64(maxW) / float64(w)
	scaleY := float64(maxH) / float64(h)
	scale := min(scaleX, scaleY)
	if scale > 1.0 {
		scale = 1.0
	}
	newW := int(float64(w) * scale)
	newH := int(float64(h) * scale)
	if newW < 1 {
		newW = 1
	}
	if newH < 1 {
		newH = 1
	}
	resized := imaging.Resize(img, newW, newH, imaging.Lanczos)

	info := fmt.Sprintf("%s📸 %s  |  %d/%d  |  Задержка: %dс%s", bold, name, idx+1, total, delay, reset)
	if paused {
		info += fmt.Sprintf(" %s[ПАУЗА]%s", yellow, reset)
	}
	if shuffle {
		info += fmt.Sprintf(" %s[СЛУЧ]%s", blue, reset)
	}
	fmt.Println(info)
	fmt.Println("Управление: ← → (нав)  Space (пауза)  +/- (скорость)  s (случ)  f (режим)  r (рестарт)  q (выход)")

	if asciiMode {
		chars := " .:-=+*#%@"
		for y := 0; y < newH; y++ {
			for x := 0; x < newW; x++ {
				c := resized.At(x, y)
				r, g, b, _ := c.RGBA()
				gray := 0.299*float64(r>>8) + 0.587*float64(g>>8) + 0.114*float64(b>>8)
				idxChar := int(gray / 255.0 * float64(len(chars)-1))
				fmt.Print(string(chars[idxChar]))
			}
			fmt.Println()
		}
	} else {
		for y := 0; y < newH; y++ {
			for x := 0; x < newW; x++ {
				c := resized.At(x, y)
				r, g, b, _ := c.RGBA()
				fmt.Print(rgbToAnsi(uint8(r>>8), uint8(g>>8), uint8(b>>8)) + "██")
			}
			fmt.Println(reset)
		}
	}
}

func min(a, b float64) float64 {
	if a < b {
		return a
	}
	return b
}

func getch() byte {
	oldState, _ := term.MakeRaw(int(os.Stdin.Fd()))
	defer term.Restore(int(os.Stdin.Fd()), oldState)
	var buf [1]byte
	os.Stdin.Read(buf[:])
	return buf[0]
}

func main() {
	var (
		paths     string
		delay     int
		shuffle   bool
		asciiMode bool
		recursive bool
	)
	flag.StringVar(&paths, "p", "", "Папки или файлы (через запятую)")
	flag.IntVar(&delay, "d", 3, "Задержка между слайдами")
	flag.BoolVar(&shuffle, "s", false, "Случайный порядок")
	flag.BoolVar(&asciiMode, "a", false, "ASCII-режим")
	flag.BoolVar(&recursive, "r", false, "Рекурсивный обход")
	flag.Parse()

	if paths == "" && flag.NArg() > 0 {
		paths = strings.Join(flag.Args(), ",")
	}
	if paths == "" {
		fmt.Println("Укажите папки или файлы.")
		flag.Usage()
		os.Exit(1)
	}
	pathList := strings.Split(paths, ",")
	files := getImageFiles(pathList, recursive)
	if len(files) == 0 {
		fmt.Println("Нет изображений.")
		os.Exit(1)
	}

	if shuffle {
		rand.Seed(time.Now().UnixNano())
		rand.Shuffle(len(files), func(i, j int) { files[i], files[j] = files[j], files[i] })
	}

	total := len(files)
	idx := 0
	paused := false
	termW, termH, _ := term.GetSize(int(os.Stdin.Fd()))

	for {
		img, err := imaging.Open(files[idx])
		if err != nil {
			fmt.Printf("Ошибка загрузки %s\n", files[idx])
			idx = (idx + 1) % total
			continue
		}
		name := filepath.Base(files[idx])
		displayImage(img, name, idx, total, delay, paused, shuffle, asciiMode, termW, termH)

		start := time.Now()
		key := byte(0)
		for time.Since(start).Seconds() < float64(delay) && !paused {
			time.Sleep(50 * time.Millisecond)
			if term.IsTerminal(int(os.Stdin.Fd())) {
				// проверка на наличие ввода
				key = getch()
				break
			}
		}

		if key == 'q' {
			break
		} else if key == ' ' {
			paused = !paused
		} else if key == '+' {
			delay = minInt(10, delay+1)
		} else if key == '-' {
			delay = maxInt(1, delay-1)
		} else if key == 's' {
			shuffle = !shuffle
			if shuffle {
				rand.Shuffle(len(files), func(i, j int) { files[i], files[j] = files[j], files[i] })
				idx = 0
			}
		} else if key == 'f' {
			asciiMode = !asciiMode
		} else if key == 'r' {
			idx = 0
		} else if key == 27 {
			// стрелка (не обрабатываем в этой версии, т.к. нужен последовательный ввод)
		} else if key == 0 && !paused {
			idx = (idx + 1) % total
		}
	}
	fmt.Printf("%s\n✅ Слайд-шоу завершено.%s\n", green, reset)
}

func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func maxInt(a, b int) int {
	if a > b {
		return a
	}
	return b
}
