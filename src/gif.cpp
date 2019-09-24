#include "gif.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace gif {

uint8_t hi_byte(uint16_t word) { return (word >> 8) & 0xff; }
uint8_t lo_byte(uint16_t word) { return word & 0xff; }

// MARK: IO

FileDevice::~FileDevice() { close(); }

bool FileDevice::open(OpenMode mode) {
  if (m != not_open) {
    return false;
  }

  if (mode == read_only) {
    f = fopen(n.c_str(), "rb");
    if (!f) {
      return false;
    }
    m = mode;
    return true;
  }
  if (mode == write_only) {
    f = fopen(n.c_str(), "wb");
    if (!f) {
      return false;
    }
    m = mode;
    return true;
  }
  return false;
}

void FileDevice::close() {
  if (m == not_open) {
    return;
  }
  fclose(f);
  m = not_open;
}

size_t FileDevice::write(const void *buf, size_t len) {
  return fwrite(buf, 1, len, f);
}

size_t FileDevice::read(void *buf, size_t len) { return fread(buf, 1, len, f); }

size_t MemDevice::write(const void *buf, size_t len) {
  size_t bytesLeft = l - p;
  if (len > bytesLeft) len = bytesLeft;
  memcpy(d + p, buf, len);
  p += len;
  return len;
}

size_t MemDevice::read(void *buf, size_t len) {
  size_t bytesLeft = l - p;
  if (len > bytesLeft) len = bytesLeft;
  memcpy(buf, d + p, len);
  p += len;
  return len;
}

int MemDevice::open_for_read(const uint8_t *src, size_t len) {
  init((uint8_t *)src, len);
  m = read_only;
  return true;
}

void MemDevice::open_for_write(uint8_t *dst, size_t maxLen) {
  init((uint8_t *)dst, maxLen);
  m = write_only;
}

class HashTable {
 public:
  HashTable() { clear(); }
  void clear() { memset(table, 0xFF, sizeof(table)); }

  void insert(uint32_t key, int code) {
    size_t n_key = norm_key(key);
    while (get_key(table[n_key]) != 0xFFFFFL) {
      n_key = (n_key + 1) & key_mask;
    }
    table[n_key] = put_key(key) | put_code(code);
  }

  int get(uint32_t key) {
    size_t n_key = norm_key(key);
    uint32_t htKey;
    while ((htKey = get_key(table[n_key])) != 0xFFFFFL) {
      if (key == htKey) return get_code(table[n_key]);
      n_key = (n_key + 1) & key_mask;
    }
    return -1;
  }

 private:
  static uint32_t get_key(uint32_t l) { return l >> 12; }
  static uint32_t get_code(uint32_t l) { return l & 0x0FFF; }
  static uint32_t put_key(uint32_t l) { return l << 12; }
  static uint32_t put_code(uint32_t l) { return l & 0x0FFF; }
  static const uint32_t key_mask = 0x1FFF;
  size_t norm_key(uint32_t key) { return ((key >> 12) ^ key) & key_mask; }
  uint32_t table[8192];
};

struct LZ {
  enum CodecConsts {
    bits = 12,
    max_code = 4095,
    flush_output,
    first_code,
    no_such_code
  };
};

class LZEncoder {
 public:
  LZEncoder(GifIO &file, size_t pixel_count, int color_res)
      : f(file),
        col_res(color_res),
        clear_code(1 << color_res),
        eof_code(clear_code + 1),
        run_code(eof_code + 1),
        run_bits(color_res + 1),
        max_code(1 << run_bits),
        crnt_code(LZ::first_code),
        crnt_shift_state(0),
        crnt_shift_dword(0),
        pix_count(pixel_count) {
    buf[0] = 0;
    encode(clear_code);
  }

  int put_line(uint8_t *line, int len) {
    if (pix_count < (unsigned)len) {
      f.set_error(ErrorCode::data_too_big);
      return 0;
    }
    pix_count -= len;

    static const uint8_t code_mask[] = {0x00, 0x01, 0x03, 0x07, 0x0f,
                                        0x1f, 0x3f, 0x7f, 0xff};
    uint8_t mask = code_mask[col_res];
    for (int i = 0; i < len; i++) {
      line[i] &= mask;
    }
    return encode_line(line, len);
  }

 private:
  bool encode(int code) {
    bool res = true;
    if (code == LZ::flush_output) {
      while (crnt_shift_state > 0) {
        if (!write_buf(crnt_shift_dword & 0xff)) {
          res = false;
        }
        crnt_shift_dword >>= 8;
        crnt_shift_state -= 8;
      }
      crnt_shift_state = 0;
      if (!write_buf(LZ::flush_output)) {
        res = false;
      }
    } else {
      crnt_shift_dword |= ((long)code) << crnt_shift_state;
      crnt_shift_state += run_bits;
      while (crnt_shift_state >= 8) {
        if (!write_buf(crnt_shift_dword & 0xff)) {
          res = false;
        }
        crnt_shift_dword >>= 8;
        crnt_shift_state -= 8;
      }
    }
    if (run_code >= max_code && code <= LZ::max_code) {
      max_code = 1 << ++run_bits;
    }
    return res;
  }

  int encode_line(uint8_t *line, int len) {
    int i = 0;
    int code = (crnt_code == LZ::LZ::first_code) ? line[i++] : crnt_code;
    while (i < len) {
      uint8_t pixel = line[i++];
      unsigned long new_key = (((uint32_t)code) << 8) + pixel;
      int new_code;

      if ((new_code = ht.get(new_key)) >= 0) {
        code = new_code;
      } else {
        if (!encode(code)) {
          f.set_error(ErrorCode::write_failed);
          return 0;
        }
        code = pixel;
        if (run_code >= LZ::max_code) {
          if (!encode(clear_code)) {
            f.set_error(ErrorCode::write_failed);
            return 0;
          }
          run_code = eof_code + 1;
          run_bits = col_res + 1;
          max_code = 1 << run_bits;
          ht.clear();
        } else {
          ht.insert(new_key, run_code++);
        }
      }
    }
    crnt_code = code;
    if (pix_count == 0) {
      if (!encode(code) || !encode(eof_code) || !encode(LZ::flush_output)) {
        f.set_error(ErrorCode::write_failed);
        return 0;
      }
    }
    return 1;
  }

  int write_buf(int c) {
    if (c == LZ::flush_output) {
      if (buf[0] != 0 && !f.write(buf, buf[0] + 1)) {
        f.set_error(ErrorCode::write_failed);
        return 0;
      }
      buf[0] = 0;
      if (!f.write(buf, 1)) {
        f.set_error(ErrorCode::write_failed);
        return 0;
      }
    } else {
      if (buf[0] == 255) {
        if (!f.write(buf, buf[0] + 1)) {
          f.set_error(ErrorCode::write_failed);
          return 0;
        }
        buf[0] = 0;
      }
      buf[++buf[0]] = c;
    }
    return 1;
  }

 private:
  GifIO &f;
  int col_res;
  const int clear_code;
  const int eof_code;
  int run_code;
  int run_bits;
  int max_code;
  int crnt_code;
  int crnt_shift_state;
  unsigned long crnt_shift_dword;
  size_t pix_count;
  uint8_t buf[256];
  HashTable ht;
};

class LZDecoder {
 public:
  LZDecoder(GifIO &file, size_t pixel_count, int color_res)
      : f(file),
        col_res(color_res),
        clear_code(1 << color_res),
        eof_code(clear_code + 1),
        run_code(eof_code + 1),
        run_bits(color_res + 1),
        max_code(1 << run_bits),
        last_code(LZ::no_such_code),
        crnt_shift_state(0),
        crnt_shift_dword(0),
        pix_count(pixel_count),
        stack_ptr(0) {
    buf[0] = 0;
    clear_prefix();
  }

  void clear_prefix() {
    for (int i = 0; i <= LZ::max_code; i++) {
      prefix[i] = LZ::no_such_code;
    }
  }

  int getLine(uint8_t *line, int lineLen) {
    uint8_t *dummy;

    if ((pix_count -= lineLen) > 0xffff0000UL) {
      f.set_error(ErrorCode::data_too_big);
      return 0;
    }

    if (!decode_line(line, lineLen)) return 0;
    if (pix_count == 0) {
      do {
        if (!next_code(&dummy)) return 0;
      } while (dummy != NULL);
    }
    return 1;
  }

 private:
  bool decode(int *code) {
    static const unsigned short code_masks[] = {
        0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f,
        0x007f, 0x00ff, 0x01ff, 0x03ff, 0x07ff, 0x0fff};

    if (run_bits > LZ::bits) {
      f.set_error(ErrorCode::image_defect);
      return false;
    }

    while (crnt_shift_state < run_bits) {
      uint8_t next_byte;
      if (!read_buf(&next_byte)) {
        return false;
      }
      crnt_shift_dword |= ((unsigned long)next_byte) << crnt_shift_state;
      crnt_shift_state += 8;
    }
    *code = crnt_shift_dword & code_masks[run_bits];
    crnt_shift_dword >>= run_bits;
    crnt_shift_state -= run_bits;

    if (run_code < LZ::max_code + 2 && ++run_code > max_code &&
        run_bits < LZ::bits) {
      max_code <<= 1;
      run_bits++;
    }
    return true;
  }

  int prefix_char(unsigned int *prefix, int code, int clear_code) {
    int i = 0;
    while (code > clear_code && i++ <= LZ::max_code) {
      if (code > LZ::max_code) {
        return LZ::no_such_code;
      }
      code = prefix[code];
    }
    return code;
  }

  bool decode_line(uint8_t *line, int line_len) {
    if (stack_ptr > LZ::max_code) {
      return false;
    }

    int i = 0;
    while (stack_ptr && i < line_len) {
      line[i++] = stack[--stack_ptr];
    }

    while (i < line_len) {
      int crnt_code;
      if (!decode(&crnt_code)) {
        return false;
      }

      if (crnt_code == eof_code) {
        f.set_error(ErrorCode::eof_too_soon);
        return false;
      } else if (crnt_code == clear_code) {
        clear_prefix();
        run_code = eof_code + 1;
        run_bits = col_res + 1;
        max_code = 1 << run_bits;
        last_code = last_code = LZ::no_such_code;
      } else {
        if (crnt_code < clear_code) {
          line[i++] = crnt_code;
        } else {
          int crnt_prefix;
          if (prefix[crnt_code] == LZ::no_such_code) {
            if (crnt_code == run_code - 2) {
              crnt_prefix = last_code;
              suffix[run_code - 2] = stack[stack_ptr++] =
                  prefix_char(prefix, last_code, clear_code);
            } else {
              f.set_error(ErrorCode::image_defect);
              return false;
            }
          } else {
            crnt_prefix = crnt_code;
          }

          while (stack_ptr < LZ::max_code && crnt_prefix > clear_code &&
                 crnt_prefix <= LZ::max_code) {
            stack[stack_ptr++] = suffix[crnt_prefix];
            crnt_prefix = prefix[crnt_prefix];
          }
          if (stack_ptr >= LZ::max_code || crnt_prefix > LZ::max_code) {
            f.set_error(ErrorCode::image_defect);
            return false;
          }
          stack[stack_ptr++] = crnt_prefix;

          while (stack_ptr != 0 && i < line_len) {
            line[i++] = stack[--stack_ptr];
          }
        }
        if ((last_code != LZ::no_such_code) &&
            (prefix[run_code - 2] == LZ::no_such_code)) {
          prefix[run_code - 2] = last_code;

          if (crnt_code == run_code - 2) {
            suffix[run_code - 2] = prefix_char(prefix, last_code, clear_code);
          } else {
            suffix[run_code - 2] = prefix_char(prefix, crnt_code, clear_code);
          }
        }
        last_code = crnt_code;
      }
    }
    return true;
  }

  bool next_code(uint8_t **code_block) {
    uint8_t len;
    if (!f.read(&len)) {
      f.set_error(ErrorCode::read_failed);
      return false;
    }
    if (len > 0) {
      *code_block = buf;
      (*code_block)[0] = len;
      if (!f.read(&((*code_block)[1]), len)) {
        f.set_error(ErrorCode::read_failed);
        return false;
      }
    } else {
      *code_block = 0;
      buf[0] = 0;
      pix_count = 0;
    }
    return true;
  }

  bool read_buf(uint8_t *nextByte) {
    if (buf[0] == 0) {
      if (!f.read(buf, 1)) {
        f.set_error(ErrorCode::read_failed);
        return false;
      }
      if (buf[0] == 0) {
        f.set_error(ErrorCode::image_defect);
        return false;
      }
      if (buf[0] == 0) {
        f.set_error(ErrorCode::image_defect);
        return false;
      }
      if (!f.read(&buf[1], buf[0])) {
        f.set_error(ErrorCode::read_failed);
        return false;
      }
      *nextByte = buf[1];
      buf[1] = 2;
      buf[0]--;
    } else {
      *nextByte = buf[buf[1]++];
      buf[0]--;
    }
    return true;
  }

 private:
  GifIO &f;
  int col_res;
  const int clear_code;
  const int eof_code;
  int run_code;
  int run_bits;
  int max_code;
  int last_code;
  int crnt_shift_state;
  unsigned long crnt_shift_dword;
  size_t pix_count;
  int stack_ptr;
  uint8_t stack[LZ::max_code];
  uint8_t buf[256];
  uint8_t suffix[LZ::max_code + 1];
  unsigned int prefix[LZ::max_code + 1];
};

// MARK: GifIO

bool GifIO::probe() {
  enum { magic_len = 6 };
  uint8_t buf[magic_len];
  if (d.read(buf, magic_len) != magic_len) {
    set_error(ErrorCode::read_failed);
    return false;
  }
  if (memcmp(buf, "GIF", 3) != 0) {
    set_error(ErrorCode::not_gif_file);
    return false;
  }
  return true;
}

bool GifIO::write_terminator() {
  uint8_t buf = Gif::terminator;
  return d.write(&buf, 1) == 1;
}

bool GifIO::read(uint8_t *buf, size_t len) { return d.read(buf, len) == len; }

bool GifIO::read(uint8_t *byte) { return d.read(byte, 1) == 1; }

bool GifIO::read(uint16_t *word) {
  uint8_t buf[2];
  if (d.read(buf, 2) != 2) {
    return false;
  }
  uint16_t lo = buf[0];
  uint16_t hi = buf[1];
  *word = lo | (hi << 8);
  return 1;
}

bool GifIO::read(RGB *rgb) {
  uint8_t buf[3];
  if (d.read(buf, 3) != 3) {
    return false;
  }
  *rgb = RGB(buf[0], buf[1], buf[2]);
  return true;
}

bool GifIO::read(Size *size) {
  uint16_t width;
  uint16_t height;
  if (!read(&width) || !read(&height)) {
    return false;
  }
  size->set(width, height);
  return true;
}

bool GifIO::read(Point *pos) {
  uint16_t x;
  uint16_t y;
  if (!read(&x) || !read(&y)) {
    return false;
  }
  pos->set(x, y);
  return true;
}

bool GifIO::read(Rect *rect) {
  Point pos;
  Size size;
  if (!read(&pos) || !read(&size)) {
    return false;
  }
  rect->set(pos, size);
  return true;
}

bool GifIO::read(GifRecordType *type) {
  uint8_t data_type;

  if (!read(&data_type)) {
    set_error(ErrorCode::read_failed);
    return false;
  }

  switch (data_type) {
    case Gif::descriptor:
      *type = GifRecordType::image_desc;
      break;
    case Gif::extension:
      *type = GifRecordType::extension;
      break;
    case Gif::terminator:
      *type = GifRecordType::terminate;
      break;
    default:
      *type = GifRecordType::undefined;
      set_error(ErrorCode::wrong_record);
      return false;
  }
  return true;
}

bool GifIO::write(const uint8_t *buf, size_t len) {
  return d.write(buf, len) == len;
}

bool GifIO::write(uint8_t byte) { return d.write(&byte, 1) == 1; }

bool GifIO::write(uint16_t word) {
  uint8_t buf[2];
  buf[0] = word & 0xff;
  buf[1] = (word >> 8) & 0xff;
  return d.write(buf, 2) == 2;
}

bool GifIO::write(RGB rgb) {
  uint8_t buf[3];
  buf[0] = rgb.r();
  buf[1] = rgb.g();
  buf[2] = rgb.b();
  return write(buf, 3);
}

bool GifIO::write(const Size &size) {
  return write(size.width()) && write(size.height());
}

bool GifIO::write(const Point &pos) { return write(pos.x()) && write(pos.y()); }

bool GifIO::write(const Rect &rect) {
  return write(rect.pos()) && write(rect.size());
}

// MARK: Extension

Extension create_animation_mark(uint16_t replays) {
  Extension res(Extension::application);
  std::string magic = "NETSCAPE2.0";
  res.append(std::vector<uint8_t>(magic.begin(), magic.end()));
  res.append({1, lo_byte(replays), hi_byte(replays)});
  return res;
}

Extension create_delay_mark(uint16_t delay) {
  Extension res(Extension::graphics);
  delay += 2;
  res.append({4, lo_byte(delay), hi_byte(delay), 0});
  return res;
}

void Extension::append(const std::vector<uint8_t> &data) {
  list.push_back(data);
}

bool Extension::load_chunk(GifIO &io, ExtensionChunk *chunk, bool *last) {
  uint8_t size;
  *last = false;

  if (!io.read(&size)) {
    io.set_error(ErrorCode::read_failed);
    return false;
  }
  if (size == 0) {
    *last = true;
    return true;
  }

  chunk->resize(size);
  if (!io.read(&((*chunk)[0]), size)) {
    io.set_error(ErrorCode::read_failed);
    return false;
  }
  return true;
}

bool Extension::save_chunk(GifIO &io, const ExtensionChunk &chunk) {
  uint8_t sz = chunk.size();
  return io.write(sz) && io.write(&chunk[0], chunk.size());
}

bool Extension::load(GifIO &io) {
  uint8_t fnc;
  if (!io.read(&fnc)) {
    io.set_error(ErrorCode::read_failed);
    return false;
  }
  fn = (FuncCode)fnc;

  bool last;
  while (true) {
    ExtensionChunk chunk;
    if (!load_chunk(io, &chunk, &last)) {
      return false;
    }
    if (last) {
      break;
    }
    list.push_back(std::move(chunk));
  };
  return true;
}

bool Extension::save_leader(GifIO &io) {
  uint8_t buf[2];
  buf[0] = Gif::extension;
  buf[1] = fn;
  return io.write(buf, 2);
}

bool Extension::save_trailer(GifIO &io) {
  uint8_t trailer = 0;
  return io.write(trailer);
}

bool Extension::save(GifIO &io) {
  if (!save_leader(io)) {
    return false;
  }
  for (auto c : list) {
    if (!save_chunk(io, c)) {
      return false;
    }
  }
  if (!save_trailer(io)) {
    return false;
  }
  return true;
}

bool ColorMap::load(GifIO &io) {
  size_t color_count = 1 << r;
  for (size_t i = 0; i < color_count; ++i) {
    c.emplace_back();
    if (!io.read(&c.back())) {
      return false;
    }
  }
  return true;
}

bool ColorMap::save(GifIO &io) {
  size_t color_count = 1 << r;
  for (auto i : c) {
    if (!io.write(i)) {
      io.set_error(ErrorCode::write_failed);
      return false;
    }
  }
  return true;
}

bool Gif::save_scr_desc(GifIO &io) {
  const char *ver = "GIF89a";
  if (!io.write((uint8_t *)ver, strlen(ver))) {
    io.set_error(ErrorCode::write_failed);
    return false;
  }

  io.write(sz);
  uint8_t buf[3];
  const int stored_color_res = cm ? cm->color_res() - 1 : 0;
  buf[0] = (cm ? 0x80 : 0x00) | (stored_color_res << 4) | stored_color_res;

  buf[1] = bg;
  buf[2] = 0;
  io.write(buf, 3);

  if (cm && !cm->save(io)) {
    return false;
  }
  return true;
}

bool get_color_res(std::optional<ColorMap> local_cm,
                   std::optional<ColorMap> global_cm, uint8_t *color_res) {
  if (!local_cm && !global_cm) {
    return false;
  }

  *color_res = local_cm ? local_cm->color_res() : global_cm->color_res();
  if (*color_res < 2) {
    *color_res = 2;
  }
  return true;
}

bool Image::save(GifIO &io, std::optional<ColorMap> global_colormap) {
  if (rect.area() == 0) {
    return true;
  }

  for (auto ext : exts) {
    if (!ext.save(io)) {
      io.set_error(ErrorCode::write_failed);
      return false;
    }
  }

  if (!save_descr(io)) {
    return false;
  }

  uint8_t color_res;
  if (!get_color_res(cm, global_colormap, &color_res)) {
    io.set_error(ErrorCode::no_color_map);
    return false;
  }
  io.write(color_res);

  LZEncoder encoder(io, rect.area(), color_res);

  unsigned height = rect.height();
  unsigned width = rect.width();

  if (interlace) {
    int offset[] = {0, 4, 2, 1};
    int jumps[] = {8, 8, 4, 2};
    for (int i = 0; i < 4; i++)
      for (int j = offset[i]; j < height; j += jumps[i]) {
        if (!encoder.put_line(&b[j * width], width)) {
          return false;
        }
      }
  } else {
    for (int i = 0; i < height; i++) {
      if (!encoder.put_line(&b[i * width], width)) {
        return false;
      }
    }
  }
  return true;
}

bool Image::load(GifIO &io) {
  if (!load_desc(io)) {
    return false;
  }

  uint8_t code_size;
  if (!io.read(&code_size)) {
    io.set_error(ErrorCode::read_failed);
    return false;
  }

  LZDecoder decoder(io, rect.area(), code_size);
  b.resize(rect.area());

  if (interlace) {
    int offset[] = {0, 4, 2, 1};
    int jumps[] = {8, 8, 4, 2};
    int w = rect.width();
    int h = rect.height();
    for (int i = 0; i < 4; i++) {
      for (int j = offset[i]; j < h; j += jumps[i]) {
        if (!decoder.getLine(&b[j * w], w)) {
          return false;
        }
      }
    }
  } else {
    if (!decoder.getLine(&b[0], b.size())) {
      return false;
    }
  }
  return true;
}

bool Image::load_desc(GifIO &io) {
  uint8_t flags;
  if (!io.read(&rect) || !io.read(&flags)) {
    io.set_error(ErrorCode::read_failed);
    return false;
  }

  uint8_t color_res = (flags & 0x07) + 1;
  interlace = flags & 0x40;
  bool local_colormap = flags & 0x80;
  if (local_colormap) {
    cm = ColorMap(color_res);
    if (!cm->load(io)) {
      cm.reset();
      return false;
    }
  }
  return true;
}

bool Image::save_descr(GifIO &io) {
  uint8_t intro = Gif::descriptor;
  io.write(intro);
  io.write(rect);

  uint8_t color_map_flag = cm ? 0x80 : 0x00;
  uint8_t interlace_flag = interlace ? 0x40 : 0x00;
  uint8_t color_res = cm ? cm->color_res() - 1 : 0;
  uint8_t flags = color_map_flag | interlace_flag | color_res;
  io.write(flags);
  if (cm && !cm->save(io)) {
    return false;
  }
  return true;
}

// MARK: Gif

bool Gif::load_scr_desc(GifIO &io) {
  uint8_t buf[3];

  if (!io.read(&sz) || !io.read(buf, 3)) {
    return false;
  }

  uint8_t color_res = (buf[0] & 0x07) + 1;
  bg = buf[1];

  bool has_global_colormap = buf[0] & 0x80;
  if (has_global_colormap) {
    cm = ColorMap(color_res);
    if (!cm->load(io)) {
      return false;
    }
  }
  return true;
}

bool Gif::load(IODevice &dev, ErrorCode *err) {
  GifIO io(dev);
  GifRecordType recordType;
  std::vector<Extension> ext_lists;

  if (!io.probe()) {
    return false;
  }

  if (!load_scr_desc(io)) {
    io.set_error(ErrorCode::no_scrn_dscr);
    return false;
  }

  do {
    if (!io.read(&recordType)) {
      io.set_error(ErrorCode::wrong_record);
      return false;
    }

    switch (recordType) {
      case GifRecordType::image_desc: {
        auto img = std::make_shared<Image>();
        if (!img->load(io)) {
          return false;
        }

        if (!ext_lists.empty()) {
          img->set_extensions(std::move(ext_lists));
          ext_lists.clear();
        }
        imgs.push_back(img);
      } break;

      case GifRecordType::extension: {
        Extension ext;
        if (!ext.load(io)) {
          return false;
        }
        ext_lists.push_back(ext);
      }
      case GifRecordType::terminate:
        break;

      default:
        break;
    }
  } while (recordType != GifRecordType::terminate);

  if (!ext_lists.empty()) {
    exs = std::move(ext_lists);
  }
  return true;
}

bool Gif::save(IODevice &dev, ErrorCode *err) {
  GifIO io(dev);

  if (!save_scr_desc(io)) {
    return false;
  }

  for (auto i : imgs) {
    if (!i->save(io, cm)) return false;
  }

  for (auto e : exs) {
    if (!e.save(io)) {
      io.set_error(ErrorCode::write_failed);
      return false;
    }
  }
  return io.write_terminator();
}  // namespace gif

void Gif::append(std::unique_ptr<Image> image) {
  imgs.push_back(std::move(image));
}

}  // namespace gif
