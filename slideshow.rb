#!/usr/bin/env ruby
# slideshow.rb
# encoding: UTF-8

require 'rmagick'
include Magick
require 'io/console'
require 'optparse'

COLORS = {
  reset: "\e[0m",
  bold: "\e[1m",
  green: "\e[92m",
  yellow: "\e[93m",
  blue: "\e[94m"
}

def rgb_to_ansi(r, g, b)
  "\e[38;2;#{r};#{g};#{b}m"
end

def get_image_files(paths, recursive)
  exts = %w[.jpg .jpeg .png .gif .bmp .tiff .webp]
  files = []
  paths.each do |p|
    if File.file?(p)
      files << p
    elsif File.directory?(p)
      pattern = recursive ? '**/*' : '*'
      Dir.glob(File.join(p, pattern)).each do |f|
        files << f if File.file?(f) && exts.include?(File.extname(f).downcase)
      end
    end
  end
  files.sort
end

def display_image(img, name, idx, total, delay, paused, shuffle, ascii_mode, term_w, term_h)
  system('clear') || system('cls')
  w, h = img.columns, img.rows
  max_w = term_w - 2
  max_h = term_h - 8
  scale_x = max_w.to_f / w
  scale_y = max_h.to_f / h
  scale = [scale_x, scale_y, 1.0].min
  new_w = [(w * scale).to_i, 1].max
  new_h = [(h * scale).to_i, 1].max
  scaled = img.scale(new_w, new_h)

  info = "#{COLORS[:bold]}📸 #{name}  |  #{idx+1}/#{total}  |  Задержка: #{delay}с#{COLORS[:reset]}"
  info += " #{COLORS[:yellow]}[ПАУЗА]#{COLORS[:reset]}" if paused
  info += " #{COLORS[:blue]}[СЛУЧ]#{COLORS[:reset]}" if shuffle
  puts info
  puts "Управление: ← → (нав)  Space (пауза)  +/- (скорость)  s (случ)  f (режим)  r (рестарт)  q (выход)"

  if ascii_mode
    chars = ' .:-=+*#%@'
    pixels = scaled.export_pixels_to_str(0, 0, new_w, new_h, 'I')
    pixels.each_byte.each_slice(new_w) do |row|
      puts row.map { |b| chars[(b.to_f / 255 * (chars.length-1)).to_i] }.join
    end
  else
    pixels = scaled.get_pixels(0, 0, new_w, new_h)
    new_h.times do |y|
      line = ''
      new_w.times do |x|
        pixel = pixels[y * new_w + x]
        line += rgb_to_ansi(pixel.red / 257, pixel.green / 257, pixel.blue / 257) + '██'
      end
      puts line + COLORS[:reset]
    end
  end
end

def getch
  STDIN.getch
end

def main
  paths = []
  delay = 3
  shuffle = false
  ascii_mode = false
  recursive = false

  OptionParser.new do |opts|
    opts.banner = "Usage: slideshow.rb [options] <path>"
    opts.on('-d SEC', Integer, 'Delay') { |v| delay = v }
    opts.on('-s', 'Shuffle') { shuffle = true }
    opts.on('-a', 'ASCII mode') { ascii_mode = true }
    opts.on('-r', 'Recursive') { recursive = true }
    opts.on('-h', 'Help') { puts opts; exit }
  end.parse!
  paths = ARGV if ARGV.any?
  if paths.empty?
    puts "Укажите папку или файлы."
    exit 1
  end

  files = get_image_files(paths, recursive)
  if files.empty?
    puts "Нет изображений."
    exit 1
  end
  files.shuffle! if shuffle
  total = files.size
  idx = 0
  paused = false
  term_w = IO.console.winsize[1]
  term_h = IO.console.winsize[0]

  loop do
    img = Image.read(files[idx]).first
    name = File.basename(files[idx])
    display_image(img, name, idx, total, delay, paused, shuffle, ascii_mode, term_w, term_h)

    start = Time.now
    key = nil
    while Time.now - start < delay && !paused
      if STDIN.ready?
        key = getch
        break
      end
      sleep 0.05
    end

    case key
    when 'q' then break
    when ' ' then paused = !paused
    when '+' then delay = [10, delay + 1].min
    when '-' then delay = [1, delay - 1].max
    when 's'
      shuffle = !shuffle
      files.shuffle! if shuffle
      idx = 0 if shuffle
    when 'f' then ascii_mode = !ascii_mode
    when 'r' then idx = 0
    when "\e"
      seq = STDIN.read_nonblock(3) rescue nil
      if seq == '[D'
        idx = (idx - 1 + total) % total
      elsif seq == '[C'
        idx = (idx + 1) % total
      end
    else
      idx = (idx + 1) % total if !key && !paused
    end
  end
  puts "#{COLORS[:green]}\n✅ Слайд-шоу завершено.#{COLORS[:reset]}"
end

main if __FILE__ == $0
