#include "../include/mesh.hpp"

// ==============================
// imports
// ==============================
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <immintrin.h>
#include <limits>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../include/spmc_queue.hpp"
#include "../thirdparty/fast_float/fast_float.h"

// ==============================
// constants + basic types
// ==============================
using idx_t = uint32_t;
static constexpr idx_t sentinel = std::numeric_limits<idx_t>::max();

struct vec2f {
  float u, v;
};
struct vec3f {
  float x, y, z;
};
struct vec3i {
  idx_t i, j, k;
};

struct consumer_store {
  std::vector<vec3f> vertices;
  std::vector<vec2f> textures;
  std::vector<vec3f> normals;
  std::vector<vec3i> face_tape;
  std::vector<idx_t> face_bounds;
};

struct range {
  std::size_t begin, end;
};

struct mesh_config {
  std::size_t batch_size;
  std::size_t num_consumers;
  std::size_t queue_capacity;
};

enum class LineType { Vertex, Texture, Normal, Face, Unknown };

struct object {
  LineType type;
  std::string_view a, b, c;
  std::size_t v_seen, t_seen, n_seen;
};

struct batch {
  const char *data;
  std::size_t size;
  std::size_t batch_id;
  std::size_t v_seen, t_seen, n_seen;
};
static constexpr batch *batch_sentinel = nullptr;

struct batch_artifact {
  std::size_t batch_id;
  std::size_t consumer_id;
  range v, t, n, ft, fb;
};

// ==============================
// general purpose utils
// ==============================
inline const char *skipWs(const char *p, const char *e) {
  while (p < e && (*p == ' ' || *p == '\t')) {
    ++p;
  }
  return p;
}

inline std::string_view stripComment(std::string_view line) {
  std::size_t p = line.find('#');
  return (p == std::string_view::npos) ? line : line.substr(0, p);
}

inline bool normalizeLine(std::string_view &line) {
  std::size_t p = line.find_first_not_of(" \t");
  if (p == std::string_view::npos) {
    return false;
  }
  line.remove_prefix(p);

  line = stripComment(line);

  std::size_t q = line.find_last_not_of(" \t");
  if (q == std::string_view::npos) {
    return false;
  }
  line = line.substr(0, q + 1);

  return !line.empty();
}

inline std::string_view nextToken(std::string_view s, std::size_t &pos) {
  const std::size_t n = s.size();
  while (pos < n && (s[pos] == ' ' || s[pos] == '\t')) {
    ++pos;
  }
  const std::size_t begin = pos;
  while (pos < n && s[pos] != ' ' && s[pos] != '\t') {
    ++pos;
  }
  return s.substr(begin, pos - begin);
}

inline float parseFloat(const char *&p, const char *&end) {
  float value = 0.0f;
  auto result = fast_float::from_chars(p, end, value);
  return (result.ec == std::errc()) ? value : 0.0f;
}

inline long parseLong(std::string_view s, bool &ok) {
  long v = 0;
  auto r = fast_float::from_chars(s.data(), s.data() + s.size(), v);
  ok = (r.ec == std::errc());
  return v;
}

inline idx_t normalizeIndex(long idx, std::size_t seen) {
  if (idx > 0) {
    std::size_t v = static_cast<std::size_t>(idx - 1);
    if (v <= std::numeric_limits<idx_t>::max()) {
      return static_cast<idx_t>(v);
    }
    return sentinel;
  }
  if (idx < 0) {
    long v = static_cast<long>(seen) + idx;
    if (v >= 0) {
      std::size_t u = static_cast<std::size_t>(v);
      if (u <= std::numeric_limits<idx_t>::max()) {
        return static_cast<idx_t>(u);
      }
    }
  }
  return sentinel;
}

inline bool flushFd(int fd, std::string &out) {
  const char *p = out.data();
  std::size_t n = out.size();
  while (n) {
    ssize_t w = ::write(fd, p, n);
    if (w <= 0) {
      return false;
    }
    p += static_cast<std::size_t>(w);
    n -= static_cast<std::size_t>(w);
  }
  out.clear();
  return true;
}

inline void appendU64(std::string &out, std::size_t v) {
  char buf[32];
  auto r = std::to_chars(buf, buf + sizeof(buf), v);
  out.append(buf, static_cast<std::size_t>(r.ptr - buf));
}

inline void appendF32(std::string &out, float v) {
  char buf[64];
  auto r = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::general);
  if (r.ec != std::errc()) {
    out += "0";
    return;
  }
  out.append(buf, static_cast<std::size_t>(r.ptr - buf));
}

// ==============================
// producer utils
// ==============================
struct ring_buffer {
  explicit ring_buffer(std::size_t capacity) : buf(capacity), cap(capacity) {}
  std::vector<batch *> buf;
  const std::size_t cap;
  std::size_t head = 0, tail = 0, count = 0;

  bool empty() const { return count == 0; }
  bool full() const { return count == cap; }
  std::size_t size() const { return count; }

  void push(batch *r) {
    buf[tail] = r;
    tail = (tail + 1) % cap;
    ++count;
  }
  batch *front() const { return buf[head]; }
  void pop() {
    head = (head + 1) % cap;
    --count;
  }

  void drain_to(SPMCQueue<batch *> &queue) {
    while (!empty()) {
      if (!queue.try_push(front())) {
        break;
      }
      pop();
    }
  }
};

struct LineCursor {
  const char *base;
  std::size_t size;
  std::size_t i = 0;

  bool next(const char *&p, const char *&e) {
    if (i >= size) {
      return false;
    }
    p = base + i;
    const char *nl = static_cast<const char *>(std::memchr(p, '\n', size - i));
    std::size_t len = nl ? static_cast<std::size_t>(nl - p) : (size - i);
    e = p + len;
    i += len + (nl ? 1 : 0);
    return true;
  }
};

inline range build_range(const char *data, std::size_t file_size,
                         std::size_t &offset, std::size_t batch_size) {
  const std::size_t begin = offset;
  std::size_t end = std::min(begin + batch_size, file_size);

  if (end < file_size) {
    const char *nl = static_cast<const char *>(
        std::memchr(data + end, '\n', file_size - end));
    end = nl ? static_cast<std::size_t>((nl - data) + 1) : file_size;
  }

  offset = end;
  return range{begin, end};
}

inline void prefixCounts(const char *p, const char *e, std::size_t &v_seen,
                         std::size_t &t_seen, std::size_t &n_seen) {
  p = skipWs(p, e);
  if (p >= e || *p != 'v') {
    return;
  }
  ++p;
  if (p >= e) {
    return;
  }

  const char c1 = *p;
  if (c1 == ' ' || c1 == '\t') {
    ++v_seen;
    return;
  }
  if (c1 == 't') {
    ++p;
    if (p < e && (*p == ' ' || *p == '\t')) {
      ++t_seen;
    }
    return;
  }
  if (c1 == 'n') {
    ++p;
    if (p < e && (*p == ' ' || *p == '\t')) {
      ++n_seen;
    }
    return;
  }
}

// ==============================
// consumer utils
// ==============================
inline LineType classifyLine(std::string_view line) {
  std::size_t pos = 0;
  std::string_view tok = nextToken(line, pos);
  if (tok.empty()) {
    return LineType::Unknown;
  }
  if (tok == "v") {
    return LineType::Vertex;
  }
  if (tok == "vt") {
    return LineType::Texture;
  }
  if (tok == "vn") {
    return LineType::Normal;
  }
  if (tok == "f") {
    return LineType::Face;
  }
  return LineType::Unknown;
}

inline void parseFaceLine(std::string_view s, std::size_t v_seen,
                          std::size_t t_seen, std::size_t n_seen,
                          consumer_store &store) {
  std::size_t pos = 0;
  std::size_t count = 0;

  for (;;) {
    std::string_view tok = nextToken(s, pos);
    if (tok.empty()) {
      break;
    }

    std::size_t i1 = tok.find('/');
    std::size_t i2 = (i1 == std::string_view::npos) ? std::string_view::npos
                                                    : tok.find('/', i1 + 1);

    std::string_view sv =
        (i1 == std::string_view::npos) ? tok : tok.substr(0, i1);
    std::string_view st = (i1 == std::string_view::npos)
                              ? std::string_view{}
                              : (i2 == std::string_view::npos
                                     ? tok.substr(i1 + 1)
                                     : tok.substr(i1 + 1, i2 - (i1 + 1)));
    std::string_view sn = (i2 == std::string_view::npos) ? std::string_view{}
                                                         : tok.substr(i2 + 1);

    bool okv = false, okt = false, okn = false;
    long iv = sv.empty() ? 0 : parseLong(sv, okv);
    long it = st.empty() ? 0 : parseLong(st, okt);
    long in = sn.empty() ? 0 : parseLong(sn, okn);

    idx_t v = (sv.empty() || !okv) ? sentinel : normalizeIndex(iv, v_seen);
    idx_t t = (st.empty() || !okt) ? sentinel : normalizeIndex(it, t_seen);
    idx_t n = (sn.empty() || !okn) ? sentinel : normalizeIndex(in, n_seen);

    store.face_tape.push_back(vec3i{v, t, n});
    ++count;
  }

  if (count > 0) {
    store.face_bounds.push_back(count);
  }
}

// ==============================
// producer
// ==============================
void producerWork(SPMCQueue<batch *> &queue, const void *obj,
                  std::size_t file_size, const mesh_config &config,
                  batch *batches) {
  ring_buffer backlog(std::max<std::size_t>(config.queue_capacity * 4, 64));

  const char *data = static_cast<const char *>(obj);
  std::size_t offset = 0;
  std::size_t batch_id = 0;
  std::size_t v_seen = 0, t_seen = 0, n_seen = 0;

  while (offset < file_size || !backlog.empty()) {
    if (!backlog.empty()) {
      backlog.drain_to(queue);
    }

    while (offset < file_size && !backlog.full()) {
      range r = build_range(data, file_size, offset, config.batch_size);

      const std::size_t id = batch_id++;
      batch *b = &batches[id];
      b->data = data + r.begin;
      b->size = r.end - r.begin;
      b->batch_id = id;
      b->v_seen = v_seen;
      b->t_seen = t_seen;
      b->n_seen = n_seen;

      LineCursor lc{data + r.begin, r.end - r.begin};
      const char *p = nullptr, *e = nullptr;
      while (lc.next(p, e)) {
        prefixCounts(p, e, v_seen, t_seen, n_seen);
      }

      backlog.push(b);
      if (backlog.size() >= 8) {
        backlog.drain_to(queue);
      }
    }

    if (backlog.full()) {
      _mm_pause();
    }
  }

  for (std::size_t i = 0; i < config.num_consumers; ++i) {
    while (!queue.try_push(const_cast<batch *>(batch_sentinel))) {
      _mm_pause();
    }
  }
}

// ==============================
// consumer
// ==============================
void consumerWork(SPMCQueue<batch *> &queue, consumer_store &store,
                  std::vector<batch_artifact> &artifacts,
                  std::size_t consumer_id) {
  batch *b{};
  for (;;) {
    if (!queue.try_pop(b)) {
      _mm_pause();
      continue;
    }
    if (b == batch_sentinel) {
      break;
    }

    const std::size_t v0 = store.vertices.size();
    const std::size_t t0 = store.textures.size();
    const std::size_t n0 = store.normals.size();
    const std::size_t ft0 = store.face_tape.size();
    const std::size_t fb0 = store.face_bounds.size();

    std::size_t v_seen = b->v_seen, t_seen = b->t_seen, n_seen = b->n_seen;

    const char *data = b->data;
    const std::size_t size = b->size;

    std::size_t i = 0;
    while (i < size) {
      const char *line_end =
          static_cast<const char *>(std::memchr(data + i, '\n', size - i));
      std::size_t len = line_end
                            ? static_cast<std::size_t>(line_end - (data + i))
                            : (size - i);

      std::string_view line(data + i, len);
      LineType type = classifyLine(line);
      if (type != LineType::Unknown && normalizeLine(line)) {
        if (type == LineType::Vertex) {
          line.remove_prefix(2);
          std::size_t pos = 0;
          std::string_view x = nextToken(line, pos);
          std::string_view y = nextToken(line, pos);
          std::string_view z = nextToken(line, pos);
          if (!x.empty() && !y.empty() && !z.empty()) {
            const char *p0 = x.data(), *e0 = p0 + x.size();
            const char *p1 = y.data(), *e1 = p1 + y.size();
            const char *p2 = z.data(), *e2 = p2 + z.size();
            store.vertices.push_back(vec3f{
                parseFloat(p0, e0), parseFloat(p1, e1), parseFloat(p2, e2)});
          }
          ++v_seen;
        } else if (type == LineType::Texture) {
          line.remove_prefix(3);
          std::size_t pos = 0;
          std::string_view u = nextToken(line, pos);
          std::string_view v = nextToken(line, pos);
          if (!u.empty()) {
            const char *p0 = u.data(), *e0 = p0 + u.size();
            float uf = parseFloat(p0, e0);
            float vf = 0.0f;
            if (!v.empty()) {
              const char *p1 = v.data(), *e1 = p1 + v.size();
              vf = parseFloat(p1, e1);
            }
            store.textures.push_back(vec2f{uf, vf});
          }
          ++t_seen;
        } else if (type == LineType::Normal) {
          line.remove_prefix(3);
          std::size_t pos = 0;
          std::string_view x = nextToken(line, pos);
          std::string_view y = nextToken(line, pos);
          std::string_view z = nextToken(line, pos);
          if (!x.empty() && !y.empty() && !z.empty()) {
            const char *p0 = x.data(), *e0 = p0 + x.size();
            const char *p1 = y.data(), *e1 = p1 + y.size();
            const char *p2 = z.data(), *e2 = p2 + z.size();
            store.normals.push_back(vec3f{
                parseFloat(p0, e0), parseFloat(p1, e1), parseFloat(p2, e2)});
          }
          ++n_seen;
        } else if (type == LineType::Face) {
          line.remove_prefix(2);
          std::size_t r = line.find_first_not_of(" \t");
          if (r != std::string_view::npos) {
            line.remove_prefix(r);
            parseFaceLine(line, v_seen, t_seen, n_seen, store);
          }
        }
      }

      i += len + (line_end ? 1 : 0);
    }

    batch_artifact a{};
    a.batch_id = b->batch_id;
    a.consumer_id = consumer_id;
    a.v = range{v0, store.vertices.size()};
    a.t = range{t0, store.textures.size()};
    a.n = range{n0, store.normals.size()};
    a.ft = range{ft0, store.face_tape.size()};
    a.fb = range{fb0, store.face_bounds.size()};
    artifacts[b->batch_id] = a;
  }
}

// ==============================
// Mesh
// ==============================
class _MeshImpl {
public:
  mesh_config mConfig;
  std::vector<consumer_store> mConsumerStores;
  std::vector<batch> mBatches;
  std::vector<batch_artifact> mBatchArtifacts;
  SPMCQueue<batch *> mQueue;

  _MeshImpl(mesh_config config)
      : mConfig(config), mQueue(config.queue_capacity) {
    mConsumerStores.resize(config.num_consumers);
  }

  bool importObj(void *obj, std::size_t file_size) {
    const std::size_t num_consumers = mConfig.num_consumers;
    std::vector<std::thread> consumers;
    consumers.reserve(num_consumers);

    for (std::size_t i = 0; i < num_consumers; ++i) {
      mConsumerStores[i].vertices.reserve(file_size / 48);
      mConsumerStores[i].textures.reserve(file_size / 80);
      mConsumerStores[i].normals.reserve(file_size / 48);
      mConsumerStores[i].face_tape.reserve(file_size / 64);
      mConsumerStores[i].face_bounds.reserve(file_size / 48);
      consumers.emplace_back([this, obj, i]() {
        consumerWork(mQueue, mConsumerStores[i], mBatchArtifacts, i);
      });
    }

    std::thread producer([this, obj, file_size]() {
      producerWork(mQueue, obj, file_size, mConfig, mBatches.data());
    });

    producer.join();
    for (auto &t : consumers) {
      t.join();
    }
    return true;
  }

  bool exportObj(int fd) {
    std::string out;
    out.reserve(1 << 20);

    auto flushIf = [&]() -> bool {
      return (out.size() < (1 << 20)) ? true : flushFd(fd, out);
    };

    auto emitV = [&](const vec3f &v) -> bool {
      out += "v ";
      appendF32(out, v.x);
      out += " ";
      appendF32(out, v.y);
      out += " ";
      appendF32(out, v.z);
      out += "\n";
      return flushIf();
    };
    auto emitVT = [&](const vec2f &t) -> bool {
      out += "vt ";
      appendF32(out, t.u);
      out += " ";
      appendF32(out, t.v);
      out += "\n";
      return flushIf();
    };
    auto emitVN = [&](const vec3f &n) -> bool {
      out += "vn ";
      appendF32(out, n.x);
      out += " ";
      appendF32(out, n.y);
      out += " ";
      appendF32(out, n.z);
      out += "\n";
      return flushIf();
    };

    auto emitIndex = [&](idx_t x) {
      appendU64(out, static_cast<std::size_t>(x) + 1);
    };
    auto emitFaceVertex = [&](const vec3i &tri) {
      out += " ";
      if (tri.j == sentinel && tri.k == sentinel) {
        emitIndex(tri.i);
      } else if (tri.j != sentinel && tri.k == sentinel) {
        emitIndex(tri.i);
        out += "/";
        emitIndex(tri.j);
      } else if (tri.j == sentinel && tri.k != sentinel) {
        emitIndex(tri.i);
        out += "//";
        emitIndex(tri.k);
      } else {
        emitIndex(tri.i);
        out += "/";
        emitIndex(tri.j);
        out += "/";
        emitIndex(tri.k);
      }
    };

    const std::size_t nb = mBatchArtifacts.size();

    for (std::size_t bid = 0; bid < nb; ++bid) {
      const batch_artifact &a = mBatchArtifacts[bid];
      const consumer_store &cs = mConsumerStores[a.consumer_id];
      for (std::size_t i = a.v.begin; i < a.v.end; ++i) {
        if (!emitV(cs.vertices[i])) {
          close(fd);
          return false;
        }
      }
    }

    for (std::size_t bid = 0; bid < nb; ++bid) {
      const batch_artifact &a = mBatchArtifacts[bid];
      const consumer_store &cs = mConsumerStores[a.consumer_id];
      for (std::size_t i = a.t.begin; i < a.t.end; ++i) {
        if (!emitVT(cs.textures[i])) {
          close(fd);
          return false;
        }
      }
    }

    for (std::size_t bid = 0; bid < nb; ++bid) {
      const batch_artifact &a = mBatchArtifacts[bid];
      const consumer_store &cs = mConsumerStores[a.consumer_id];
      for (std::size_t i = a.n.begin; i < a.n.end; ++i) {
        if (!emitVN(cs.normals[i])) {
          close(fd);
          return false;
        }
      }
    }

    for (std::size_t bid = 0; bid < nb; ++bid) {
      const batch_artifact &a = mBatchArtifacts[bid];
      const consumer_store &cs = mConsumerStores[a.consumer_id];

      std::size_t ft = a.ft.begin;
      for (std::size_t fb = a.fb.begin; fb < a.fb.end; ++fb) {
        const std::size_t cnt = cs.face_bounds[fb];
        out += "f";
        for (std::size_t k = 0; k < cnt; ++k) {
          emitFaceVertex(cs.face_tape[ft++]);
        }
        out += "\n";
        if (!flushIf()) {
          close(fd);
          return false;
        }
      }
    }

    if (!out.empty() && !flushFd(fd, out)) {
      close(fd);
      return false;
    }
    return (close(fd) == 0);
  }
};

Mesh::Mesh() {
  mesh_config config;
  config.batch_size = 256 * 1024;
  config.num_consumers = std::thread::hardware_concurrency() > 4
                             ? std::thread::hardware_concurrency() - 4
                             : 2;
  config.queue_capacity = 4 * config.num_consumers;
  _impl = std::make_unique<_MeshImpl>(config);
}
Mesh::~Mesh() = default;

bool Mesh::importObj(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    return false;
  }

  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

  struct stat st;
  if (fstat(fd, &st) == -1) {
    close(fd);
    return false;
  }

  readahead(fd, 0, st.st_size);
  std::size_t file_size = static_cast<std::size_t>(st.st_size);
  if (file_size == 0) {
    close(fd);
    return false;
  }

  void *obj =
      mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
  close(fd);
  if (obj == MAP_FAILED) {
    return false;
  }

  madvise(obj, file_size, MADV_SEQUENTIAL);
  madvise(obj, file_size, MADV_WILLNEED);

  std::size_t num_batches = 0;
  {
    const char *data = static_cast<const char *>(obj);
    std::size_t off = 0;
    while (off < file_size) {
      (void)build_range(data, file_size, off, _impl->mConfig.batch_size);
      ++num_batches;
    }
  }
  _impl->mBatchArtifacts.resize(num_batches);
  _impl->mBatches.resize(num_batches);

  if (!_impl->importObj(obj, file_size)) {
    munmap(obj, file_size);
    return false;
  }
  return (munmap(obj, file_size) == 0);
}

bool Mesh::exportObj(const char *path) const {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    return false;
  }
  return _impl->exportObj(fd);
}
