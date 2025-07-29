#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

#include <unistd.h>

#include <fmt/core.h>
#include <fmt/format.h>
#include "../lib/fast_float/fast_float.h"

#include "../include/obj-parser.hpp"
#include "../include/mesh.hpp"

// Newline-aligned chunks for parallelized parsing
struct Chunk
{
    const char *start;
    const char *end;
};

// ---------------------------------------------------------------------------
//   Forward Declarations and Inline Functions
// ---------------------------------------------------------------------------

static void parseChunk(const char *start, const char *end, Mesh &out);
static std::vector<Chunk> splitFileIntoChunks(const char *file, size_t file_size, size_t num_threads);
static void mergeMeshes(Mesh &main_mesh, const Mesh &partial_mesh, std::mutex &mesh_mutex);
static void parseLine(Mesh &mesh, std::string_view line, bool parallel);
static void parseVertex(Mesh &mesh, std::string_view line);
static void parseTexture(Mesh &mesh, std::string_view line);
static void parseNormal(Mesh &mesh, std::string_view line);
static void parseFace(Mesh &mesh, std::string_view line, bool parallel);
static int parseIndex(const char *start, const char *end, Mesh &mesh, int state, bool parallel);
static void updateFace(Face &f, size_t state, int v_idx, int vt_idx, int vn_idx);
static void resolveFace(Face &f, Mesh &mesh);
inline bool is_space(char c)
{
    return c == ' ' || c == '\t';
}
inline const char *findChar(const char *start, const char *end, char c)
{
    for (const char *ptr = start; ptr < end; ++ptr)
    {
        if (*ptr == c)
            return ptr;
    }
    return nullptr;
}
inline std::string_view formatIndex(int idx, char* buf) {
    if (idx == INT_MIN)
        return std::string_view{};
    auto end = fmt::format_to(buf, "{}", idx + 1);
    return std::string_view(buf, end - buf);
}

// ---------------------------------------------------------------------------
//   Core API
// ---------------------------------------------------------------------------

void importMeshFromObj(Mesh &mesh, const char *obj_file, off_t file_size)
{
    size_t pos = 0;
    size_t line_end = 0;
    std::string_view line;
    while (pos < static_cast<size_t>(file_size))
    {
        const char *start = obj_file + pos;
        const char *end = obj_file + file_size;
        const char *newline = static_cast<const char *>(findChar(start, end, '\n'));

        // Get a view of the current line
        line_end = newline ? (newline - obj_file) : file_size;
        line = std::string_view(obj_file + pos, line_end - pos);

        // Setup the next line before parsing to avoid infinite loops on `continue`
        pos = newline ? line_end + 1 : file_size;

        // Pass leading whitespace
        size_t i = 0;
        for (; i < line.size() && is_space(line[i]); ++i)
            ;

        // Skip comments and empty lines
        if (i >= line.size() || line[i] == '#')
            continue;

        parseLine(mesh, line.substr(i), false);
    }
}

void importMeshFromObjParallel(Mesh &mesh, const char *obj_file, off_t file_size)
{
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
        num_threads = 4;

    auto chunks = splitFileIntoChunks(obj_file, file_size, num_threads);

    std::vector<std::thread> workers;
    std::vector<Mesh> partial_meshes(num_threads);
    std::mutex mesh_mutex;

    // Parse a chunk in each thread
    for (size_t i = 0; i < num_threads; ++i)
    {
        workers.emplace_back([&, i]()
                             { parseChunk(chunks[i].start, chunks[i].end, partial_meshes[i]); });
    }

    // Wait for all threads
    for (auto &t : workers)
        t.join();

    // Merge partial results into the main mesh
    for (const auto &pm : partial_meshes)
    {
        mergeMeshes(mesh, pm, mesh_mutex);
    }

    // Resolve to positive 0-indexed
    for (size_t i = 0; i < mesh.faces.size(); ++i)
    {
        resolveFace(mesh.faces[i], mesh);
    }
}

void exportMeshToObj(const Mesh &mesh, int fd)
{
    uint64_t reserve_size =
        mesh.vertices.size() * 50 +
        mesh.normals.size()  * 50 +
        mesh.textures.size() * 35 +
        mesh.faces.size()    * 120;
    
    fmt::memory_buffer buf;
    buf.reserve(reserve_size);
    auto out_it = std::back_inserter(buf);

    // auto start_v = std::chrono::high_resolution_clock::now();    
    for (auto &v : mesh.vertices)
    {
        fmt::format_to(out_it, "v {} {} {}\n", v.x, v.y, v.z);
    }
    // auto end_v = std::chrono::high_resolution_clock::now();
    // auto start_t = std::chrono::high_resolution_clock::now();    
    for (auto &t : mesh.textures)
    {
        fmt::format_to(out_it, "vt {} {}\n", t.u, t.v);
    }
    // auto end_t = std::chrono::high_resolution_clock::now();
    // auto start_n = std::chrono::high_resolution_clock::now();    
    for (auto &n : mesh.normals)
    {
        fmt::format_to(out_it, "vn {} {} {}\n", n.x, n.y, n.z);
    }
    // auto end_n = std::chrono::high_resolution_clock::now();
    // auto start_f = std::chrono::high_resolution_clock::now();    
    char buf_vt1[16], buf_vt2[16], buf_vt3[16];
    char buf_vn1[16], buf_vn2[16], buf_vn3[16];
    for (auto &f : mesh.faces) 
    {
        fmt::format_to(out_it,
            "f {}/{}/{} {}/{}/{} {}/{}/{}\n",
            f.v1 + 1,
            formatIndex(f.vt1, buf_vt1),
            formatIndex(f.vn1, buf_vn1),
            f.v2 + 1,
            formatIndex(f.vt2, buf_vt2),
            formatIndex(f.vn2, buf_vn2),
            f.v3 + 1,
            formatIndex(f.vt3, buf_vt3),
            formatIndex(f.vn3, buf_vn3));
    }
    // auto end_f = std::chrono::high_resolution_clock::now();
    // auto start_w = std::chrono::high_resolution_clock::now();
    const char *data = buf.data();
    int64_t size = buf.size();
    int64_t written = 0;
    while (written < size)
    {
        ssize_t nbytes = write(fd, data + written, size - written);
        if (written == -1)
        {
            perror("write");
            break;
        }
        written += nbytes;
    }
    // auto end_w = std::chrono::high_resolution_clock::now();

    // auto v_time = std::chrono::duration_cast<std::chrono::microseconds>(end_v - start_v).count();
    // auto t_time = std::chrono::duration_cast<std::chrono::microseconds>(end_t - start_t).count();
    // auto n_time = std::chrono::duration_cast<std::chrono::microseconds>(end_n - start_n).count();
    // auto f_time = std::chrono::duration_cast<std::chrono::microseconds>(end_f - start_f).count();
    // auto w_time = std::chrono::duration_cast<std::chrono::microseconds>(end_w - start_w).count();

    // std::cerr << "Vertex time: " << v_time << std::endl;
    // std::cerr << "Texture time: " << t_time << std::endl;
    // std::cerr << "Normal time: " << n_time << std::endl;
    // std::cerr << "Face time: " << f_time << std::endl;
    // std::cerr << "Write time: " << w_time << std::endl;
}

// ---------------------------------------------------------------------------
//   Templates
// ---------------------------------------------------------------------------

template <typename T>
static void customPushBack(std::vector<T> &vec, const T &value, float growth_factor = 4.0f)
{
    if (vec.size() == vec.capacity())
    {
        size_t new_capacity = static_cast<size_t>(vec.capacity() * growth_factor);
        if (new_capacity <= vec.capacity())
        {
            // Handle small capacities like 0 or 1:
            new_capacity = vec.capacity() + 1;
        }
        vec.reserve(new_capacity);
    }
    vec.push_back(value);
}

template <typename Callback>
static void forEachComponent(std::string_view line, size_t max_components, Callback cb)
{
    const char *ptr = line.data();
    const char *end = ptr + line.size();
    const char *start;
    size_t num_components = 0;
    while (ptr < end && num_components < max_components)
    {
        // Pass whitespace
        while (ptr < end && is_space(*ptr))
            ++ptr;

        // Get a pointer and a size for the current component
        start = ptr;
        while (ptr < end && !is_space(*ptr))
            ++ptr;

        // If there was a component found, call the callback
        if (ptr > start)
            cb(start, ptr, num_components);

        // Increment the number of components
        ++num_components;
    }
}

template <typename T>
static inline bool toNumber(const char *start, const char *end, T &out)
{
    auto result = fast_float::from_chars(start, end, out);
    return result.ec == std::errc();
}

// ---------------------------------------------------------------------------
//   Parsers
// ---------------------------------------------------------------------------

static void parseChunk(const char *start, const char *end, Mesh &out)
{
    size_t pos = 0;
    size_t size = static_cast<size_t>(end - start);

    while (pos < size)
    {
        const char *line_start = start + pos;
        const char *newline = static_cast<const char *>(findChar(line_start, end, '\n'));
        size_t line_len = newline ? (newline - line_start) : (end - line_start);

        // Get a view of the current line
        std::string_view line(line_start, line_len);

        // Setup the next line before parsing to avoid infinite loops on `continue`
        pos += line_len + (newline ? 1 : 0);

        // Skip leading whitespace
        size_t i = 0;
        for (; i < line.size() && is_space(line[i]); ++i)
            ;

        // Skip comments and empty lines
        if (i >= line.size() || line[i] == '#')
            continue;

        parseLine(out, line.substr(i), true);
    }
}

static void parseLine(Mesh &mesh, std::string_view line, bool parallel)
{
    if (line.size() < 5)
    {
        return;
    }

    if (line[0] == 'v')
    {
        if (is_space(line[1]))
        {
            parseVertex(mesh, line.substr(2));
        }
        else if (line[1] == 't' && is_space(line[2]))
        {
            parseTexture(mesh, line.substr(3));
        }
        else if (line[1] == 'n' && is_space(line[2]))
        {
            parseNormal(mesh, line.substr(3));
        }
    }
    else if (line[0] == 'f' && is_space(line[1]))
    {
        parseFace(mesh, line.substr(2), parallel);
    }
}

static void parseVertex(Mesh &mesh, std::string_view line)
{
    Vertex v = {NAN, NAN, NAN};

    forEachComponent(
        line,
        3,
        [&](const char *start, const char *end, size_t state)
        {
            float val;
            if (toNumber(start, end, val))
            {
                if (state == 0)
                    v.x = val;
                else if (state == 1)
                    v.y = val;
                else if (state == 2)
                    v.z = val;
            }
            else
            {
                // ERROR
            }
        });

    customPushBack(mesh.vertices, v);
}

static void parseTexture(Mesh &mesh, std::string_view line)
{
    Texture t = {NAN, NAN};

    forEachComponent(
        line,
        2,
        [&](const char *start, const char *end, size_t state)
        {
            float val;
            if (toNumber(start, end, val))
            {
                if (state == 0)
                    t.u = val;
                else if (state == 1)
                    t.v = val;
            }
            else
            {
                // ERROR
            }
        });

    customPushBack(mesh.textures, t);
}

static void parseNormal(Mesh &mesh, std::string_view line)
{
    Normal n = {NAN, NAN, NAN};

    forEachComponent(
        line,
        3,
        [&](const char *start, const char *end, size_t state)
        {
            float val;
            if (toNumber(start, end, val))
            {
                if (state == 0)
                    n.x = val;
                else if (state == 1)
                    n.y = val;
                else if (state == 2)
                    n.z = val;
            }
            else
            {
                // ERROR
            }
        });

    customPushBack(mesh.normals, n);
}

static void parseFace(Mesh &mesh, std::string_view line, bool parallel)
{
    Face f = {INT_MIN, INT_MIN, INT_MIN, INT_MIN, INT_MIN, INT_MIN, INT_MIN, INT_MIN, INT_MIN};

    const char *ptr = line.data();
    const char *end = ptr + line.size();

    int state = 0;

    while (ptr < end && state < 3)
    {
        // Skip whitespace
        while (ptr < end && is_space(*ptr))
            ++ptr;

        // Find end of token
        const char *token_start = ptr;
        while (ptr < end && !is_space(*ptr))
            ++ptr;
        const char *token_end = ptr;

        // Split token into v/vt/vn
        const char *slash1 = findChar(token_start, token_end, '/');
        const char *slash2 = slash1 ? findChar(slash1 + 1, token_end, '/') : nullptr;

        int v_idx = parseIndex(token_start, slash1 ? slash1 : token_end, mesh, 0, parallel);
        int vt_idx = INT_MIN;
        int vn_idx = INT_MIN;

        if (slash1 && slash1 + 1 < token_end)
        {
            if (slash2)
            {
                vt_idx = parseIndex(slash1 + 1, slash2, mesh, 1, parallel);
                vn_idx = parseIndex(slash2 + 1, token_end, mesh, 2, parallel);
            }
            else
            {
                vt_idx = parseIndex(slash1 + 1, token_end, mesh, 1, parallel);
            }
        }

        updateFace(f, state, v_idx, vt_idx, vn_idx);
        ++state;
    }

    customPushBack(mesh.faces, f);
}

static int parseIndex(const char *start, const char *end, Mesh &mesh, int state, bool parallel)
{
    int idx = 0;
    if (toNumber(start, end, idx))
    {
        if (parallel)
        {
            // Resolving indices is handled later
            return idx;
        }
        else
        {
            if (idx > 0)
            {
                return idx - 1;
            }
            else if (idx < 0)
            {
                if (state == 0)
                {
                    return idx + mesh.vertices.size();
                }
                else if (state == 1)
                {
                    return idx + mesh.textures.size();
                }
                else
                {
                    return idx + mesh.normals.size();
                }
            }
            // 0 is not a valid index in obj files
        }
    }
    // Represent invalid or empty indices with INT_MIN
    return INT_MIN;
}

// ---------------------------------------------------------------------------
//   Helpers
// ---------------------------------------------------------------------------

static std::vector<Chunk> splitFileIntoChunks(const char *file, size_t file_size, size_t num_threads)
{
    std::vector<Chunk> chunks;
    chunks.reserve(num_threads);

    size_t chunk_size = file_size / num_threads;
    const char *file_end = file + file_size;
    const char *chunk_start = file;
    const char *chunk_end, *possible_end;
    for (size_t i = 0; i < num_threads; ++i)
    {
        if (i == num_threads - 1)
        {
            chunk_end = file_end;
        }
        else
        {
            // Get the start + the size, or the end of the file if this is the last chunk
            possible_end = chunk_start + chunk_size;
            if (possible_end >= file_end)
                possible_end = file_end;

            // Break chunks in reasonable places (i.e., don't separate one line across threads)
            const char *newline_ptr = possible_end;
            while (newline_ptr < file_end && *newline_ptr != '\n')
                ++newline_ptr;

            // Include the newline in the chunk
            if (newline_ptr < file_end)
                ++newline_ptr;

            chunk_end = newline_ptr;
        }

        chunks.push_back({chunk_start, chunk_end});
        chunk_start = chunk_end;
    }

    return chunks;
}

static void mergeMeshes(Mesh &main_mesh, const Mesh &partial_mesh, std::mutex &mesh_mutex)
{
    std::lock_guard<std::mutex> lock(mesh_mutex);

    // Append vertices, textures, normals
    main_mesh.vertices.insert(main_mesh.vertices.end(), partial_mesh.vertices.begin(), partial_mesh.vertices.end());
    main_mesh.textures.insert(main_mesh.textures.end(), partial_mesh.textures.begin(), partial_mesh.textures.end());
    main_mesh.normals.insert(main_mesh.normals.end(), partial_mesh.normals.begin(), partial_mesh.normals.end());

    // Append faces WITHOUT modifying their indices
    main_mesh.faces.insert(main_mesh.faces.end(), partial_mesh.faces.begin(), partial_mesh.faces.end());
}

static void updateFace(Face &f, size_t state, int v_idx, int vt_idx, int vn_idx)
{
    if (state == 0)
    {
        f.v1 = v_idx;
        f.vt1 = vt_idx;
        f.vn1 = vn_idx;
    }
    else if (state == 1)
    {
        f.v2 = v_idx;
        f.vt2 = vt_idx;
        f.vn2 = vn_idx;
    }
    else if (state == 2)
    {
        f.v3 = v_idx;
        f.vt3 = vt_idx;
        f.vn3 = vn_idx;
    }
}

static void resolveFace(Face &f, Mesh &mesh)
{
    if (f.v1 < 0)
        f.v1 += mesh.vertices.size();
    else
        f.v1 -= 1;
    if (f.v2 < 0)
        f.v2 += mesh.vertices.size();
    else
        f.v2 -= 1;
    if (f.v3 < 0)
        f.v3 += mesh.vertices.size();
    else
        f.v3 -= 1;
    if (f.vt1 < 0)
        f.vt1 += mesh.textures.size();
    else
        f.vt1 -= 1;
    if (f.vt2 < 0)
        f.vt2 += mesh.textures.size();
    else
        f.vt2 -= 1;
    if (f.vt3 < 0)
        f.vt3 += mesh.textures.size();
    else
        f.vt3 -= 1;
    if (f.vn1 < 0)
        f.vn1 += mesh.normals.size();
    else
        f.vn1 -= 1;
    if (f.vn2 < 0)
        f.vn2 += mesh.normals.size();
    else
        f.vn2 -= 1;
    if (f.vn3 < 0)
        f.vn3 += mesh.normals.size();
    else
        f.vn3 -= 1;
}
