// slideshow.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.Processing;
using SixLabors.ImageSharp.PixelFormats;

class Slideshow
{
    static string Colorize(string text, string color) => color + text + "\x1b[0m";
    static string RgbToAnsi(byte r, byte g, byte b) => $"\x1b[38;2;{r};{g};{b}m";

    static List<string> GetImageFiles(string[] paths, bool recursive)
    {
        var exts = new HashSet<string>{".jpg",".jpeg",".png",".gif",".bmp",".tiff",".webp"};
        var files = new List<string>();
        foreach (var p in paths)
        {
            if (File.Exists(p))
            {
                files.Add(p);
            }
            else if (Directory.Exists(p))
            {
                var options = recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
                files.AddRange(Directory.GetFiles(p, "*.*", options)
                    .Where(f => exts.Contains(Path.GetExtension(f).ToLower())));
            }
        }
        files.Sort();
        return files;
    }

    static void DisplayImage(Image<Rgb24> img, string name, int idx, int total, int delay, bool paused, bool shuffle, bool asciiMode, int termW, int termH)
    {
        Console.Clear();
        int w = img.Width, h = img.Height;
        int maxW = termW - 2, maxH = termH - 8;
        double scaleX = (double)maxW / w, scaleY = (double)maxH / h;
        double scale = Math.Min(scaleX, scaleY);
        if (scale > 1.0) scale = 1.0;
        int newW = Math.Max(1, (int)(w * scale));
        int newH = Math.Max(1, (int)(h * scale));
        var clone = img.Clone();
        clone.Mutate(ctx => ctx.Resize(newW, newH));

        string info = $"\x1b[1m📸 {name}  |  {idx+1}/{total}  |  Задержка: {delay}с\x1b[0m";
        if (paused) info += $" \x1b[93m[ПАУЗА]\x1b[0m";
        if (shuffle) info += $" \x1b[94m[СЛУЧ]\x1b[0m";
        Console.WriteLine(info);
        Console.WriteLine("Управление: ← → (нав)  Space (пауза)  +/- (скорость)  s (случ)  f (режим)  r (рестарт)  q (выход)");

        if (asciiMode)
        {
            string chars = " .:-=+*#%@";
            for (int y = 0; y < newH; y++)
            {
                for (int x = 0; x < newW; x++)
                {
                    var pixel = clone[x, y];
                    double gray = 0.299 * pixel.R + 0.587 * pixel.G + 0.114 * pixel.B;
                    int idxChar = (int)((gray / 255) * (chars.Length - 1));
                    Console.Write(chars[idxChar]);
                }
                Console.WriteLine();
            }
        }
        else
        {
            for (int y = 0; y < newH; y++)
            {
                for (int x = 0; x < newW; x++)
                {
                    var pixel = clone[x, y];
                    Console.Write(RgbToAnsi(pixel.R, pixel.G, pixel.B) + "██");
                }
                Console.WriteLine("\x1b[0m");
            }
        }
    }

    static int Main(string[] args)
    {
        var paths = new List<string>();
        int delay = 3;
        bool shuffle = false, asciiMode = false, recursive = false;

        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "-d" && i+1 < args.Length) delay = int.Parse(args[++i]);
            else if (args[i] == "-s") shuffle = true;
            else if (args[i] == "-a") asciiMode = true;
            else if (args[i] == "-r") recursive = true;
            else if (args[i] == "-h" || args[i] == "--help")
            {
                Console.WriteLine("Usage: slideshow <path> [options]\n  -d <sec>   Delay\n  -s         Shuffle\n  -a         ASCII mode\n  -r         Recursive");
                return 0;
            }
            else paths.Add(args[i]);
        }
        if (paths.Count == 0) { Console.WriteLine("Укажите папку или файлы."); return 1; }

        var files = GetImageFiles(paths.ToArray(), recursive);
        if (files.Count == 0) { Console.WriteLine("Нет изображений."); return 1; }
        if (shuffle)
        {
            var rnd = new Random();
            files = files.OrderBy(x => rnd.Next()).ToList();
        }
        int total = files.Count, idx = 0;
        bool paused = false;
        int termW = Console.WindowWidth, termH = Console.WindowHeight;

        while (true)
        {
            var img = Image.Load<Rgb24>(files[idx]);
            string name = Path.GetFileName(files[idx]);
            DisplayImage(img, name, idx, total, delay, paused, shuffle, asciiMode, termW, termH);

            var start = DateTime.Now;
            var key = Console.ReadKey(true);
            while ((DateTime.Now - start).TotalSeconds < delay && !paused)
            {
                if (Console.KeyAvailable) { key = Console.ReadKey(true); break; }
                System.Threading.Thread.Sleep(100);
            }

            if (key.KeyChar == 'q') break;
            else if (key.KeyChar == ' ') paused = !paused;
            else if (key.KeyChar == '+') delay = Math.Min(10, delay + 1);
            else if (key.KeyChar == '-') delay = Math.Max(1, delay - 1);
            else if (key.KeyChar == 's')
            {
                shuffle = !shuffle;
                if (shuffle)
                {
                    var rnd = new Random();
                    files = files.OrderBy(x => rnd.Next()).ToList();
                    idx = 0;
                }
            }
            else if (key.KeyChar == 'f') asciiMode = !asciiMode;
            else if (key.KeyChar == 'r') idx = 0;
            else if (key.Key == ConsoleKey.RightArrow) idx = (idx + 1) % total;
            else if (key.Key == ConsoleKey.LeftArrow) idx = (idx - 1 + total) % total;
            else if (!key.KeyChar.Equals('\0') && !paused) idx = (idx + 1) % total;
        }
        Console.WriteLine($"\x1b[92m\n✅ Слайд-шоу завершено.\x1b[0m");
        return 0;
    }
}
