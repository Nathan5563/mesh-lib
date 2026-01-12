#include "../include/mesh.hpp"

#include <charconv>
#include <cstddef>
#include <vector>
#include <thread>
#include <string_view>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <immintrin.h>

#include "../include/spmc_queue.hpp"
#include "../thirdparty/fast_float/fast_float.h"

struct vec2f
{
    float u, v;
};

struct vec3f
{
    float x, y, z;
};

struct vec3i
{
    std::size_t i, j, k;
};

struct consumer_store
{
    std::vector<vec3f> vertices;
    std::vector<vec2f> textures;
    std::vector<vec3f> normals;
    std::vector<vec3i> face_tape; // (v, t, n) is one vertex in a face
    std::vector<std::size_t> face_bounds; // number of vertices in the ith face
};

struct range
{
    std::size_t begin, end;
};

struct mesh_config
{
    std::size_t batch_size;
    std::size_t num_consumers;
    std::size_t queue_capacity;
};

enum class LineType
{
    Vertex,
    Texture,
    Normal,
    Face,
    Unknown
}; 

struct object
{
    LineType type;
    std::string_view a, b, c;
    std::size_t v_seen, t_seen, n_seen;
};

struct batch
{
    std::vector<object> objects;
    std::size_t batch_id;
};

struct batch_artifact
{
    std::size_t batch_id;
    std::size_t consumer_id;
    range v, t, n, ft, fb;
};

struct ring_buffer
{
    explicit ring_buffer(std::size_t capacity)
        : buf(capacity), cap(capacity) {}

    std::vector<batch*> buf;
    const std::size_t cap;
    std::size_t head = 0, tail = 0, count = 0;

    bool empty() const
    {
        return count == 0;
    }
    
    bool full() const
    {
        return count == cap;
    }
    
    std::size_t size() const
    {
        return count;
    }

    void push(batch* r)
    {
        buf[tail] = r;
        tail = (tail + 1) % cap;
        ++count;
    }

    batch* front() const
    {
        return buf[head];
    }

    void pop()
    {
        head = (head + 1) % cap;
        --count;
    }

    void drain_to(SPMCQueue<batch*>& queue)
    {
        while (!empty())
        {
            if (!queue.try_push(front()))
            {
                break;
            }
            pop();
        }
    }
};

inline range build_range(
    const char* data,
    std::size_t file_size,
    std::size_t& offset,
    std::size_t batch_size
){
    const std::size_t begin = offset;
    std::size_t end = std::min(begin + batch_size, file_size);

    if (end < file_size)
    {
        const char* nl = static_cast<const char*>(
            std::memchr(data + end, '\n', file_size - end)
        );
        end = nl ? static_cast<std::size_t>((nl - data) + 1) : file_size;
    }

    offset = end;
    return range{begin, end};
}

static constexpr std::size_t sentinel = static_cast<std::size_t>(-1);
static const batch* batch_sentinel = nullptr;

inline std::string_view stripComment(std::string_view line)
{
    std::size_t p = line.find('#');
    if (p == std::string_view::npos)
    {
        return line;
    }
    return line.substr(0, p);
}

inline LineType classifyLine(std::string_view line)
{
    std::size_t p = line.find_first_not_of(" \t");
    if (p == std::string_view::npos)
    {
        return LineType::Unknown;
    }
    line.remove_prefix(p);

    if (line.size() >= 2 && line[0] == 'v' && (line[1] == ' ' || line[1] == '\t'))
    {
        return LineType::Vertex;
    }
    if (line.size() >= 3 && line[0] == 'v' && line[1] == 't' && (line[2] == ' ' || line[2] == '\t'))
    {
        return LineType::Texture;
    }
    if (line.size() >= 3 && line[0] == 'v' && line[1] == 'n' && (line[2] == ' ' || line[2] == '\t'))
    {
        return LineType::Normal;
    }
    if (line.size() >= 2 && line[0] == 'f' && (line[1] == ' ' || line[1] == '\t'))
    {
        return LineType::Face;
    }
    return LineType::Unknown;
}

inline std::string_view nextToken(std::string_view s, std::size_t& pos)
{
    const std::size_t n = s.size();
    while (pos < n && (s[pos] == ' ' || s[pos] == '\t'))
    {
        ++pos;
    }
    const std::size_t begin = pos;
    while (pos < n && s[pos] != ' ' && s[pos] != '\t')
    {
        ++pos;
    }
    return s.substr(begin, pos - begin);
}

inline void packageLine(
    std::string_view line,
    LineType type,
    std::vector<object>& out,
    std::size_t& v_seen,
    std::size_t& t_seen,
    std::size_t& n_seen
){
    std::size_t p = line.find_first_not_of(" \t");
    if (p == std::string_view::npos)
    {
        return;
    }
    line.remove_prefix(p);
    line = stripComment(line);

    std::size_t q = line.find_last_not_of(" \t");
    if (q == std::string_view::npos)
    {
        return;
    }
    line = line.substr(0, q + 1);

    if (type == LineType::Vertex)
    {
        line.remove_prefix(2);
        std::size_t pos = 0;
        std::string_view x = nextToken(line, pos);
        std::string_view y = nextToken(line, pos);
        std::string_view z = nextToken(line, pos);
        if (!x.empty() && !y.empty() && !z.empty())
        {
            out.push_back(object{type, x, y, z, v_seen, t_seen, n_seen});
            ++v_seen;
        }
        return;
    }
    if (type == LineType::Texture)
    {
        line.remove_prefix(3);
        std::size_t pos = 0;
        std::string_view u = nextToken(line, pos);
        std::string_view v = nextToken(line, pos);
        std::string_view w = nextToken(line, pos);
        if (!u.empty())
        {
            out.push_back(object{type, u, v, w, v_seen, t_seen, n_seen});
            ++t_seen;
        }
        return;
    }
    if (type == LineType::Normal)
    {
        line.remove_prefix(3);
        std::size_t pos = 0;
        std::string_view x = nextToken(line, pos);
        std::string_view y = nextToken(line, pos);
        std::string_view z = nextToken(line, pos);
        if (!x.empty() && !y.empty() && !z.empty())
        {
            out.push_back(object{type, x, y, z, v_seen, t_seen, n_seen});
            ++n_seen;
        }
        return;
    }
    if (type == LineType::Face)
    {
        line.remove_prefix(2);
        std::size_t r = line.find_first_not_of(" \t");
        if (r == std::string_view::npos)
        {
            return;
        }
        line.remove_prefix(r);
        line = stripComment(line);

        std::size_t s = line.find_last_not_of(" \t");
        if (s == std::string_view::npos)
        {
            return;
        }
        line = line.substr(0, s + 1);

        out.push_back(object{type, line, {}, {}, v_seen, t_seen, n_seen});
        return;
    }
}

void producerWork(
    SPMCQueue<batch*>& queue,
    const void* obj,
    std::size_t file_size,
    const mesh_config& config
){
    ring_buffer backlog(std::max<std::size_t>(config.queue_capacity * 4, 64));

    const char* data = static_cast<const char*>(obj);
    std::size_t offset = 0;
    std::size_t batch_id = 0;
    std::size_t v_seen = 0;
    std::size_t t_seen = 0;
    std::size_t n_seen = 0;
    while (offset < file_size || !backlog.empty())
    {
        if (!backlog.empty())
        {
            backlog.drain_to(queue);
        }

        while (offset < file_size && !backlog.full())
        {
            range r = build_range(data, file_size, offset, config.batch_size);
            std::vector<object> objects;

            std::size_t i = r.begin;
            while (i < r.end)
            {
                const char* line_end = static_cast<const char*>(
                    std::memchr(data + i, '\n', r.end - i)
                );

                std::size_t len = line_end
                    ? static_cast<std::size_t>(line_end - (data + i))
                    : (r.end - i);

                std::string_view line(data + i, len);
                LineType type = classifyLine(line);
                if (type != LineType::Unknown)
                {
                    packageLine(line, type, objects, v_seen, t_seen, n_seen);
                }

                i += len + (line_end ? 1 : 0);
            }

            batch* b = new batch{std::move(objects), batch_id++};
            backlog.push(b);

            if (backlog.size() >= 8)
            {
                backlog.drain_to(queue);
            }
        }

        if (backlog.full())
        {
            _mm_pause();
        }
    }

    for (std::size_t i = 0; i < config.num_consumers; ++i)
    {
        while (!queue.try_push(const_cast<batch*>(batch_sentinel)))
        {
            _mm_pause();
        }
    }
}

inline float parseFloat(const char*& p, const char*& end)
{
    float value = 0.0f;
    auto result = fast_float::from_chars(p, end, value);
    if (result.ec == std::errc())
    {
        return value;
    }
    return 0.0f; // default value in case of parsing failure
}

inline long parseLong(std::string_view s, bool& ok)
{
    long v = 0;
    auto r = fast_float::from_chars(s.data(), s.data() + s.size(), v);
    ok = (r.ec == std::errc());
    return v;
}

inline std::size_t normalizeIndex(long idx, std::size_t seen)
{
    if (idx > 0)
    {
        return static_cast<std::size_t>(idx - 1);
    }
    if (idx < 0)
    {
        long v = static_cast<long>(seen) + idx;
        if (v >= 0)
        {
            return static_cast<std::size_t>(v);
        }
    }
    return sentinel;
}

inline void parseObject(
    const object& o,
    consumer_store& store
){
    if (o.type == LineType::Vertex)
    {
        const char* p0 = o.a.data();
        const char* e0 = p0 + o.a.size();
        const char* p1 = o.b.data();
        const char* e1 = p1 + o.b.size();
        const char* p2 = o.c.data();
        const char* e2 = p2 + o.c.size();
        float x = parseFloat(p0, e0);
        float y = parseFloat(p1, e1);
        float z = parseFloat(p2, e2);
        store.vertices.push_back(vec3f{x, y, z});
        return;
    }
    if (o.type == LineType::Texture)
    {
        const char* p0 = o.a.data();
        const char* e0 = p0 + o.a.size();
        float u = parseFloat(p0, e0);

        float v = 0.0f;
        if (!o.b.empty())
        {
            const char* p1 = o.b.data();
            const char* e1 = p1 + o.b.size();
            v = parseFloat(p1, e1);
        }

        store.textures.push_back(vec2f{u, v});
        return;
    }
    if (o.type == LineType::Normal)
    {
        const char* p0 = o.a.data();
        const char* e0 = p0 + o.a.size();
        const char* p1 = o.b.data();
        const char* e1 = p1 + o.b.size();
        const char* p2 = o.c.data();
        const char* e2 = p2 + o.c.size();
        float x = parseFloat(p0, e0);
        float y = parseFloat(p1, e1);
        float z = parseFloat(p2, e2);
        store.normals.push_back(vec3f{x, y, z});
        return;
    }
    if (o.type == LineType::Face)
    {
        std::string_view s = o.a;
        std::size_t pos = 0;
        std::size_t count = 0;
        for (;;)
        {
            std::string_view tok = nextToken(s, pos);
            if (tok.empty())
            {
                break;
            }

            std::size_t i1 = tok.find('/');
            std::size_t i2 = i1 == std::string_view::npos ? std::string_view::npos : tok.find('/', i1 + 1);

            std::string_view sv = i1 == std::string_view::npos ? tok : tok.substr(0, i1);
            std::string_view st = i1 == std::string_view::npos ? std::string_view{} :
                (i2 == std::string_view::npos ? tok.substr(i1 + 1) : tok.substr(i1 + 1, i2 - (i1 + 1)));
            std::string_view sn = i2 == std::string_view::npos ? std::string_view{} : tok.substr(i2 + 1);

            bool okv = false;
            bool okt = false;
            bool okn = false;
            long iv = sv.empty() ? 0 : parseLong(sv, okv);
            long it = st.empty() ? 0 : parseLong(st, okt);
            long in = sn.empty() ? 0 : parseLong(sn, okn);

            std::size_t v = (sv.empty() || !okv) ? sentinel : normalizeIndex(iv, o.v_seen);
            std::size_t t = (st.empty() || !okt) ? sentinel : normalizeIndex(it, o.t_seen);
            std::size_t n = (sn.empty() || !okn) ? sentinel : normalizeIndex(in, o.n_seen);

            store.face_tape.push_back(vec3i{v, t, n});
            ++count;
        }

        if (count > 0)
        {
            store.face_bounds.push_back(count);
        }
        return;
    }
}

void consumerWork(
    SPMCQueue<batch*>& queue,
    consumer_store& store,
    std::vector<batch_artifact>& artifacts,
    std::size_t consumer_id
){
    batch* b{};
    for (;;)
    {
        if (!queue.try_pop(b))
        {
            _mm_pause();
            continue;
        }

        if (b == batch_sentinel)
        {
            break;
        }

        const std::size_t v0 = store.vertices.size();
        const std::size_t t0 = store.textures.size();
        const std::size_t n0 = store.normals.size();
        const std::size_t ft0 = store.face_tape.size();
        const std::size_t fb0 = store.face_bounds.size();

        for (const object& o : b->objects)
        {
            parseObject(o, store);
        }

        const std::size_t v1 = store.vertices.size();
        const std::size_t t1 = store.textures.size();
        const std::size_t n1 = store.normals.size();
        const std::size_t ft1 = store.face_tape.size();
        const std::size_t fb1 = store.face_bounds.size();

        batch_artifact a{};
        a.batch_id = b->batch_id;
        a.consumer_id = consumer_id;
        a.v = range{v0,  v1};
        a.t = range{t0,  t1};
        a.n = range{n0,  n1};
        a.ft = range{ft0, ft1};
        a.fb = range{fb0, fb1};
        artifacts[b->batch_id] = a;

        delete b;
    }
}

class _MeshImpl
{
public:
    mesh_config mConfig;
    std::vector<consumer_store> mConsumerStores;
    std::vector<batch_artifact> mBatchArtifacts;
    SPMCQueue<batch*> mQueue;

    _MeshImpl(mesh_config config) : 
        mConfig(config), 
        mQueue(config.queue_capacity) 
    {
        mConsumerStores.resize(config.num_consumers);
    }

    bool importObj(void* obj, std::size_t file_size)
    {
        const std::size_t num_consumers = mConfig.num_consumers;
        std::vector<std::thread> consumers;
        consumers.reserve(num_consumers);
        for (std::size_t i = 0; i < num_consumers; ++i)
        {
            consumers.emplace_back(
                [this, obj, i]()
                {
                    consumerWork(
                        mQueue,
                        mConsumerStores[i],
                        mBatchArtifacts,
                        i
                    );
                }
            );
        }

        std::thread producer;
        producer = std::thread(
            [this, obj, file_size]()
            {
                producerWork(
                    mQueue,
                    obj,
                    file_size,
                    mConfig
                );
            }
        );

        producer.join();
        for (auto& t : consumers)
        {
            t.join();
        }

        return true;
    }

    bool exportObj(void* obj)
    {
        return true;
    }
};

Mesh::Mesh()
{
    mesh_config config;
    config.batch_size = 256 * 1024;
    config.num_consumers = std::thread::hardware_concurrency() > 4 ? 
        std::thread::hardware_concurrency() - 4 : 2;
    config.queue_capacity = 2 * config.num_consumers;

    _impl = std::make_unique<_MeshImpl>(config);
}
Mesh::~Mesh() = default;

bool Mesh::importObj(const char* path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        close(fd);
        return false;
    }
    std::size_t file_size = static_cast<std::size_t>(st.st_size);
    if (file_size == 0)
    {
        close(fd);
        return false;
    }

    std::size_t num_batches =
        (file_size + _impl->mConfig.batch_size - 1) / _impl->mConfig.batch_size;
    _impl->mBatchArtifacts.resize(num_batches);

    void* obj = mmap(
        nullptr, 
        file_size, 
        PROT_READ, 
        MAP_PRIVATE, 
        fd, 
        0
    );
    close(fd);
    if (obj == MAP_FAILED)
    {
        return false;
    }

    if (!_impl->importObj(obj, file_size))
    {
        munmap(obj, file_size);
        return false;
    }

    int r = munmap(obj, file_size);
    if (r != 0)
    {
        return false;
    }

    return true;
}

bool Mesh::exportObj(const char* path) const
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        return false;
    }

    auto flush = [&](std::string& out) -> bool
    {
        const char* p = out.data();
        std::size_t n = out.size();
        while (n)
        {
            ssize_t w = ::write(fd, p, n);
            if (w <= 0)
            {
                return false;
            }
            p += static_cast<std::size_t>(w);
            n -= static_cast<std::size_t>(w);
        }
        out.clear();
        return true;
    };

    auto appendU64 = [&](std::string& out, std::size_t v)
    {
        char buf[32];
        auto r = std::to_chars(buf, buf + sizeof(buf), v);
        out.append(buf, static_cast<std::size_t>(r.ptr - buf));
    };

    auto appendF32 = [&](std::string& out, float v)
    {
        char buf[64];
        auto r = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::general);
        if (r.ec != std::errc())
        {
            out += "0";
            return;
        }
        out.append(buf, static_cast<std::size_t>(r.ptr - buf));
    };

    std::string out;
    out.reserve(1 << 20);

    const std::size_t nb = _impl->mBatchArtifacts.size();

    for (std::size_t bid = 0; bid < nb; ++bid)
    {
        const batch_artifact& a = _impl->mBatchArtifacts[bid];
        const consumer_store& cs = _impl->mConsumerStores[a.consumer_id];

        for (std::size_t i = a.v.begin; i < a.v.end; ++i)
        {
            const vec3f& v = cs.vertices[i];
            out += "v ";
            appendF32(out, v.x);
            out += " ";
            appendF32(out, v.y);
            out += " ";
            appendF32(out, v.z);
            out += "\n";

            if (out.size() >= (1 << 20))
            {
                if (!flush(out))
                {
                    close(fd);
                    return false;
                }
            }
        }
    }

    for (std::size_t bid = 0; bid < nb; ++bid)
    {
        const batch_artifact& a = _impl->mBatchArtifacts[bid];
        const consumer_store& cs = _impl->mConsumerStores[a.consumer_id];

        for (std::size_t i = a.t.begin; i < a.t.end; ++i)
        {
            const vec2f& t = cs.textures[i];
            out += "vt ";
            appendF32(out, t.u);
            out += " ";
            appendF32(out, t.v);
            out += "\n";

            if (out.size() >= (1 << 20))
            {
                if (!flush(out))
                {
                    close(fd);
                    return false;
                }
            }
        }
    }

    for (std::size_t bid = 0; bid < nb; ++bid)
    {
        const batch_artifact& a = _impl->mBatchArtifacts[bid];
        const consumer_store& cs = _impl->mConsumerStores[a.consumer_id];

        for (std::size_t i = a.n.begin; i < a.n.end; ++i)
        {
            const vec3f& n = cs.normals[i];
            out += "vn ";
            appendF32(out, n.x);
            out += " ";
            appendF32(out, n.y);
            out += " ";
            appendF32(out, n.z);
            out += "\n";

            if (out.size() >= (1 << 20))
            {
                if (!flush(out))
                {
                    close(fd);
                    return false;
                }
            }
        }
    }

    for (std::size_t bid = 0; bid < nb; ++bid)
    {
        const batch_artifact& a = _impl->mBatchArtifacts[bid];
        const consumer_store& cs = _impl->mConsumerStores[a.consumer_id];

        std::size_t ft = a.ft.begin;
        for (std::size_t fb = a.fb.begin; fb < a.fb.end; ++fb)
        {
            const std::size_t cnt = cs.face_bounds[fb];
            out += "f";
            for (std::size_t k = 0; k < cnt; ++k)
            {
                const vec3i& tri = cs.face_tape[ft++];

                out += " ";
                if (tri.j == sentinel && tri.k == sentinel)
                {
                    appendU64(out, tri.i + 1);
                }
                else if (tri.j != sentinel && tri.k == sentinel)
                {
                    appendU64(out, tri.i + 1);
                    out += "/";
                    appendU64(out, tri.j + 1);
                }
                else if (tri.j == sentinel && tri.k != sentinel)
                {
                    appendU64(out, tri.i + 1);
                    out += "//";
                    appendU64(out, tri.k + 1);
                }
                else
                {
                    appendU64(out, tri.i + 1);
                    out += "/";
                    appendU64(out, tri.j + 1);
                    out += "/";
                    appendU64(out, tri.k + 1);
                }
            }
            out += "\n";

            if (out.size() >= (1 << 20))
            {
                if (!flush(out))
                {
                    close(fd);
                    return false;
                }
            }
        }
    }

    if (!out.empty())
    {
        if (!flush(out))
        {
            close(fd);
            return false;
        }
    }

    int r = close(fd);
    if (r != 0)
    {
        return false;
    }

    return true;
}
