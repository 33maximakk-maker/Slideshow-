// slideshow.java
import java.awt.*;
import java.awt.image.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import javax.imageio.*;

public class slideshow {
    private static final String RESET = "\u001B[0m";
    private static final String BOLD = "\u001B[1m";
    private static final String GREEN = "\u001B[92m";
    private static final String YELLOW = "\u001B[93m";
    private static final String BLUE = "\u001B[94m";

    private static String rgbToAnsi(int r, int g, int b) {
        return String.format("\u001B[38;2;%d;%d;%dm", r, g, b);
    }

    private static String[] getImageFiles(String[] paths, boolean recursive) {
        String[] exts = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".webp"};
        Set<String> extSet = new HashSet<>(Arrays.asList(exts));
        List<String> files = new ArrayList<>();
        for (String p : paths) {
            File f = new File(p);
            if (f.isFile()) {
                files.add(p);
            } else if (f.isDirectory()) {
                try {
                    Files.walk(f.toPath())
                        .filter(Files::isRegularFile)
                        .forEach(path -> {
                            String name = path.toString();
                            String ext = name.substring(name.lastIndexOf('.')).toLowerCase();
                            if (extSet.contains(ext)) files.add(name);
                        });
                } catch (IOException e) {}
            }
        }
        Collections.sort(files);
        return files.toArray(new String[0]);
    }

    private static BufferedImage resize(BufferedImage img, int newW, int newH) {
        Image scaled = img.getScaledInstance(newW, newH, Image.SCALE_SMOOTH);
        BufferedImage resized = new BufferedImage(newW, newH, BufferedImage.TYPE_INT_RGB);
        Graphics2D g2d = resized.createGraphics();
        g2d.drawImage(scaled, 0, 0, null);
        g2d.dispose();
        return resized;
    }

    private static void displayImage(BufferedImage img, String name, int idx, int total, int delay, boolean paused, boolean shuffle, boolean asciiMode, int termW, int termH) {
        System.out.print("\033[H\033[2J");
        int w = img.getWidth(), h = img.getHeight();
        int maxW = termW - 2, maxH = termH - 8;
        double scaleX = (double)maxW / w, scaleY = (double)maxH / h;
        double scale = Math.min(scaleX, scaleY);
        if (scale > 1.0) scale = 1.0;
        int newW = Math.max(1, (int)(w * scale));
        int newH = Math.max(1, (int)(h * scale));
        BufferedImage resized = resize(img, newW, newH);

        String info = BOLD + "📸 " + name + "  |  " + (idx+1) + "/" + total + "  |  Задержка: " + delay + "с" + RESET;
        if (paused) info += " " + YELLOW + "[ПАУЗА]" + RESET;
        if (shuffle) info += " " + BLUE + "[СЛУЧ]" + RESET;
        System.out.println(info);
        System.out.println("Управление: ← → (нав)  Space (пауза)  +/- (скорость)  s (случ)  f (режим)  r (рестарт)  q (выход)");

        if (asciiMode) {
            String chars = " .:-=+*#%@";
            for (int y = 0; y < newH; y++) {
                for (int x = 0; x < newW; x++) {
                    int rgb = resized.getRGB(x, y);
                    int r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
                    double gray = 0.299 * r + 0.587 * g + 0.114 * b;
                    int idxChar = (int)(gray / 255 * (chars.length() - 1));
                    System.out.print(chars.charAt(idxChar));
                }
                System.out.println();
            }
        } else {
            for (int y = 0; y < newH; y++) {
                for (int x = 0; x < newW; x++) {
                    int rgb = resized.getRGB(x, y);
                    int r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
                    System.out.print(rgbToAnsi(r, g, b) + "██");
                }
                System.out.println(RESET);
            }
        }
    }

    public static void main(String[] args) throws Exception {
        List<String> paths = new ArrayList<>();
        int delay = 3;
        boolean shuffle = false, asciiMode = false, recursive = false;

        for (int i = 0; i < args.length; i++) {
            if (args[i].equals("-d") && i+1 < args.length) delay = Integer.parseInt(args[++i]);
            else if (args[i].equals("-s")) shuffle = true;
            else if (args[i].equals("-a")) asciiMode = true;
            else if (args[i].equals("-r")) recursive = true;
            else if (args[i].equals("-h") || args[i].equals("--help")) {
                System.out.println("Usage: slideshow <path> [options]\n  -d <sec>   Delay\n  -s         Shuffle\n  -a         ASCII mode\n  -r         Recursive");
                return;
            } else paths.add(args[i]);
        }
        if (paths.isEmpty()) { System.err.println("Укажите папку или файлы."); System.exit(1); }

        String[] files = getImageFiles(paths.toArray(new String[0]), recursive);
        if (files.length == 0) { System.err.println("Нет изображений."); System.exit(1); }
        if (shuffle) {
            List<String> list = Arrays.asList(files);
            Collections.shuffle(list);
            files = list.toArray(new String[0]);
        }
        int total = files.length, idx = 0;
        boolean paused = false;
        int termW = 80, termH = 24; // fallback

        while (true) {
            BufferedImage img = ImageIO.read(new File(files[idx]));
            String name = new File(files[idx]).getName();
            displayImage(img, name, idx, total, delay, paused, shuffle, asciiMode, termW, termH);

            long start = System.currentTimeMillis();
            int key = -1;
            while (System.currentTimeMillis() - start < delay * 1000 && !paused) {
                if (System.in.available() > 0) {
                    key = System.in.read();
                    break;
                }
                Thread.sleep(50);
            }

            if (key == 'q') break;
            else if (key == ' ') paused = !paused;
            else if (key == '+') delay = Math.min(10, delay + 1);
            else if (key == '-') delay = Math.max(1, delay - 1);
            else if (key == 's') {
                shuffle = !shuffle;
                if (shuffle) {
                    List<String> list = Arrays.asList(files);
                    Collections.shuffle(list);
                    files = list.toArray(new String[0]);
                    idx = 0;
                }
            } else if (key == 'f') asciiMode = !asciiMode;
            else if (key == 'r') idx = 0;
            else if (key == 27) {
                byte[] seq = new byte[2];
                System.in.read(seq);
                if (seq[0] == '[' && seq[1] == 'D') idx = (idx - 1 + total) % total;
                else if (seq[0] == '[' && seq[1] == 'C') idx = (idx + 1) % total;
            } else if (key == -1 && !paused) {
                idx = (idx + 1) % total;
            }
        }
        System.out.println(GREEN + "\n✅ Слайд-шоу завершено." + RESET);
    }
}
