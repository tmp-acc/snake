#ifndef GIF_H
#define GIF_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gif {

class IODevice {
 public:
  enum OpenMode { not_open = 0x0000, read_only = 0x0001, write_only = 0x0002 };

  IODevice() : m(not_open) {}
  virtual ~IODevice(){};

  IODevice(IODevice const &) = delete;
  IODevice &operator=(IODevice const &) = delete;

  virtual size_t read(void *buf, size_t len) = 0;
  virtual size_t write(const void *buf, size_t len) = 0;

 protected:
  OpenMode m;
};

class FileDevice : public IODevice {
 public:
  FileDevice(const std::string &name) : n(name), f(0) {}
  ~FileDevice() override;

  bool open(OpenMode mode);
  size_t read(void *buf, size_t len) override;
  size_t write(const void *buf, size_t len) override;

 private:
  void close();

  std::string n;
  FILE *f;
};

class MemDevice : public IODevice {
 public:
  size_t read(void *buf, size_t len) override;
  size_t write(const void *buf, size_t len) override;

  int open_for_read(const uint8_t *src, size_t len);
  void open_for_write(uint8_t *dst, size_t maxLen);

 private:
  void init(uint8_t *data, size_t len) {
    d = data;
    l = len;
    p = 0;
  }

  uint8_t *d;
  size_t p;
  size_t l;
};

class Size {
 public:
  Size() : wd(0), ht(0) {}
  Size(uint16_t width, uint16_t height) : wd(width), ht(height) {}
  uint16_t width() const { return wd; }
  uint16_t height() const { return ht; }
  void set_width(uint16_t width) { wd = width; }
  void set_height(uint16_t height) { ht = height; }
  void set(uint16_t width, uint16_t height) {
    wd = width;
    ht = height;
  }
  unsigned area() const { return wd * ht; }

 private:
  uint16_t wd;
  uint16_t ht;
};

class Point {
 public:
  Point() : x_val(0), y_val(0) {}
  Point(uint16_t x, uint16_t y) : x_val(x), y_val(y) {}
  uint16_t x() const { return x_val; }
  uint16_t y() const { return y_val; }
  void set_x(uint16_t x) { x_val = x; }
  void set_y(uint16_t y) { y_val = y; }
  void set(uint16_t x, uint16_t y) {
    x_val = x;
    y_val = y;
  }

 private:
  uint16_t x_val;
  uint16_t y_val;
};

class Rect {
 public:
  Rect() {}
  Rect(const Size &size) : s(size) {}
  Rect(const Point &pos, const Size &size) : p(pos), s(size) {}

  Size size() const { return s; }
  Size &rsize() { return s; }
  void set_ize(Size size) { s = size; }
  unsigned square() const { return s.area(); }
  uint16_t width() const { return s.width(); }
  uint16_t height() const { return s.height(); }
  void set_width(uint16_t width) { s.set_width(width); }
  void set_height(uint16_t height) { s.set_height(height); }
  unsigned area() const { return s.area(); }

  Point pos() const { return p; }
  Point &rpos() { return p; }
  void set_pos(const Point &pos) { p = pos; }
  uint16_t x() const { return p.x(); }
  uint16_t y() const { return p.y(); }
  void set_x(uint16_t x) { p.set_x(x); }
  void set_y(uint16_t y) { p.set_y(y); }
  void set(const Point &pos, const Size &size) {
    p = pos;
    s = size;
  }

 private:
  Point p;
  Size s;
};

class RGB {
 public:
  RGB() : r_val(0), g_val(0), b_val(0) {}
  RGB(uint8_t r, uint8_t g, uint8_t b) : r_val(r), g_val(g), b_val(b) {}
  uint8_t r() const { return r_val; }
  uint8_t g() const { return g_val; }
  uint8_t b() const { return b_val; }

 private:
  uint8_t r_val;
  uint8_t g_val;
  uint8_t b_val;
};

enum class ErrorCode {
  ok,
  write_failed,
  read_failed,
  not_gif_file,
  eof_too_soon,
  data_too_big,
  wrong_record,
  no_color_map,
  no_scrn_dscr,
  image_defect
};

enum GifRecordType { undefined, screen_desc, image_desc, extension, terminate };

class GifIO {
 public:
  GifIO(IODevice &dev) : e(ErrorCode::ok), d(dev) {}
  bool probe();
  bool write_terminator();
  void set_error(ErrorCode err) { e = err; }

  bool read(uint8_t *buf, size_t len);
  bool read(uint8_t *byte);
  bool read(uint16_t *word);
  bool read(RGB *rgb);
  bool read(Size *size);
  bool read(Point *pos);
  bool read(Rect *rect);
  bool read(GifRecordType *record_type);

  bool write(const uint8_t *buf, size_t len);
  bool write(uint8_t byte);
  bool write(uint16_t word);
  bool write(RGB rgb);
  bool write(const Size &size);
  bool write(const Point &pos);
  bool write(const Rect &rect);

 private:
  ErrorCode e;
  IODevice &d;
};

using ExtensionChunk = std::vector<uint8_t>;

class Extension {
 public:
  enum FuncCode {
    next = 0x00,
    comment = 0xfe,
    graphics = 0xf9,
    plaintext = 0x01,
    application = 0xff
  };

  Extension() {}
  Extension(FuncCode func) : fn(func) {}
  void append(const std::vector<uint8_t> &data);

  bool load(GifIO &io);
  bool save(GifIO &io);

  FuncCode function() const { return fn; }

  bool empty() const { return list.empty(); }

 private:
  bool load_chunk(GifIO &io, ExtensionChunk *chunk, bool *last);
  bool save_chunk(GifIO &io, const ExtensionChunk &chunk);

  bool save_leader(GifIO &io);
  bool save_trailer(GifIO &io);

  FuncCode fn = next;
  std::vector<ExtensionChunk> list;
};

Extension create_animation_mark(uint16_t replays);
Extension create_delay_mark(uint16_t delay);

class ColorMap {
 public:
  ColorMap(uint8_t color_res) : r(color_res) {}

  uint8_t color_res() const { return r; }

  bool load(GifIO &io);
  bool save(GifIO &io);

 private:
  uint8_t r;  // number of colors = 2^color_res; maximum 2^8 = 256 colors
  std::vector<RGB> c;
};

class Image {
 public:
  Image() {}
  Image(const Size &size) : rect(size), b(size.area()) {}

  const uint8_t *bits() const { return &b[0]; }
  const uint8_t *bits(uint16_t x, uint16_t y) const {
    return &b[(size_t)rect.width() * y + x];
  }
  const uint8_t *bits(const Point &pos) const {
    return &b[(size_t)rect.width() * pos.y() + pos.x()];
  }

  uint8_t *rbits() { return &b[0]; }
  uint8_t *rbits(const Point &pos) {
    return &b[(size_t)rect.width() * pos.y() + pos.x()];
  }
  uint8_t *rbits(uint16_t x, uint16_t y) {
    return &b[(size_t)rect.width() * y + x];
  }

  Size size() const { return rect.size(); }
  void set_extensions(const std::vector<Extension> &extensions) {
    exts = extensions;
  }
  void set_extensions(std::vector<Extension> &&extensions) {
    exts = std::move(extensions);
  }
  bool save(GifIO &io, std::optional<ColorMap> global_colormap);
  bool load(GifIO &io);

 private:
  bool load_desc(GifIO &io);
  bool save_descr(GifIO &io);

  bool interlace = false;
  std::vector<Extension> exts;
  Rect rect;
  std::vector<uint8_t> b;
  std::optional<ColorMap> cm;
};

class Gif {
 public:
  enum DataIntroducer {
    extension = 0x21,
    descriptor = 0x2c,
    terminator = 0x3b
  };

  Gif() {}
  Gif(const Size &size, uint8_t backGround) : sz(size), bg(backGround) {}

  void set_color_map(const ColorMap &color_map) { cm = color_map; }
  std::optional<ColorMap> color_map() const { return cm; }

  bool load(IODevice &dev, ErrorCode *err = nullptr);
  bool save(IODevice &dev, ErrorCode *err = nullptr);

  void append(std::unique_ptr<Image> image);

  const std::vector<std::shared_ptr<Image>> &images() const { return imgs; }

 private:
  bool load_scr_desc(GifIO &io);
  bool save_scr_desc(GifIO &io);

  Size sz;
  uint8_t bg;
  std::vector<std::shared_ptr<Image>> imgs;  // TODO: std::unique_ptr
  std::vector<Extension> exs;
  std::optional<ColorMap> cm;
};

}  // namespace gif

#endif  // GIF_H
