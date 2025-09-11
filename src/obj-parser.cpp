#include <cstdint>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <unistd.h>

#include <fmt/core.h>
#include <fmt/format.h>
#include "../lib/fast_float/fast_float.h"

#include "../include/obj-parser.hpp"
#include "../include/mesh.hpp"

// ---------------------------------------------------------------------------
//   Structs
// ---------------------------------------------------------------------------

struct Chunk
{
    const char *start;
    const char *end;
};

// ---------------------------------------------------------------------------
//   Function Declarations and Inline Functions
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
static void parseMtllib(Mesh &mesh, std::string_view name);
static void parseUsemtl(Mesh &mesh, std::string_view material);
static void resolveFace(Face &f, Mesh &mesh);
inline bool is_space(char c)
{
    return c == ' ' || c == '\t';
}
inline const char *findChar(const char *start, const char *end, char c)
{
    std::string_view sv(start, end - start);
    auto pos = sv.find(c);
    if (pos != std::string_view::npos)
    {
        return start + pos;
    }
    else
    {
        return nullptr;
    }
}
inline std::string_view formatIndex(int idx, char* buf) {
    if (idx == INT_MIN)
        return std::string_view{};
    auto end = fmt::format_to(buf, "{}", idx + 1);
    return std::string_view(buf, end - buf);
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
//   Core API
// ---------------------------------------------------------------------------

void importMeshFromObj(Mesh &mesh, const char *obj_file, size_t file_size)
{
    size_t pos = 0;
    size_t line_end = 0;
    std::string_view line;
    while (pos < file_size)
    {
        const char *start = obj_file + pos;
        const char *end = obj_file + file_size;
        const char *newline = static_cast<const char *>(findChar(start, end, '\n'));

        // Get a view of the current line
        line_end = newline ? (newline - obj_file) : file_size;
        line = std::string_view(obj_file + pos, line_end - pos);

        // Setup the next line before parsing to avoid infinite loops on `continue`
        pos = newline ? line_end + 1 : file_size;

        // Skip leading whitespace
        size_t i = 0;
        for (; i < line.size() && is_space(line[i]); ++i);

        // Skip comments and empty lines
        if (i >= line.size() || line[i] == '#')
            continue;

        parseLine(mesh, line.substr(i), false);
    }
}

void importMeshFromObjParallel(Mesh &mesh, const char *obj_file, size_t file_size)
{
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0)
    {
        std::cout << "Insufficient thread count, switching to sequential parser...\n";
        importMeshFromObj(mesh, obj_file, file_size);
        return;
    }

    auto chunks = splitFileIntoChunks(obj_file, file_size, num_threads);

    std::vector<std::thread> workers;
    std::vector<Mesh> partial_meshes(num_threads);
    std::mutex mesh_mutex;

    // Parse a chunk in each thread
    for (size_t i = 0; i < num_threads; ++i)
    {
        workers.emplace_back([&, i](){ 
            parseChunk(chunks[i].start, chunks[i].end, partial_meshes[i]); 
        }
    );
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
    fmt::memory_buffer buf;
    uint64_t reserve_size =
            mesh.vertices.size() * 50 +
            mesh.normals.size()  * 50 +
            mesh.textures.size() * 35 +
            mesh.faces.size()    * 120;
    buf.reserve(reserve_size);
    auto out_it = std::back_inserter(buf);

    // Write vertices
    for (auto &v : mesh.vertices)
    {
        fmt::format_to(out_it, "v {} {} {}\n", v.x, v.y, v.z);
    }

    // Write textures
    for (auto &t : mesh.textures)
    {
        fmt::format_to(out_it, "vt {} {}\n", t.u, t.v);
    }

    // Write normals
    for (auto &n : mesh.normals)
    {
        fmt::format_to(out_it, "vn {} {} {}\n", n.x, n.y, n.z);
    }

    // Write faces
    for (auto &f : mesh.faces)
    {
        fmt::format_to(out_it, "f");

        for (size_t i = 0; i < f.v.len; ++i)
        {
            int64_t v_idx  = mesh.vertex_indices[f.v.start + i] + 1; // OBJ is 1-based
            int64_t vt_idx = (f.vt.len > 0) ? mesh.texture_indices[f.vt.start + i] + 1 : 0;
            int64_t vn_idx = (f.vn.len > 0) ? mesh.normal_indices[f.vn.start + i] + 1 : 0;

            fmt::format_to(out_it, " {}", v_idx);

            if (f.vt.len > 0 || f.vn.len > 0)
            {
                fmt::format_to(out_it, "/");
                if (f.vt.len > 0)
                    fmt::format_to(out_it, "{}", vt_idx);
                if (f.vn.len > 0)
                    fmt::format_to(out_it, "/{}", vn_idx);
            }
        }
        fmt::format_to(out_it, "\n");
    }

    // Write buffer to file descriptor
    const char *data = buf.data();
    int64_t size = buf.size();
    int64_t written = 0;
    while (written < size)
    {
        ssize_t nbytes = write(fd, data + written, size - written);
        if (nbytes == -1)
        {
            perror("write");
            break;
        }
        written += nbytes;
    }
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
        for (; i < line.size() && is_space(line[i]); ++i);

        // Skip comments and empty lines
        if (i >= line.size() || line[i] == '#')
            continue;

        parseLine(out, line.substr(i), true);
    }
}

static void parseLine(Mesh &mesh, std::string_view line, bool parallel)
{
    if (line.size() < 2)
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
    else if (
        line[0] == 'm' &&
        line[1] == 't' &&
        line[2] == 'l' &&
        line[3] == 'l' &&
        line[4] == 'i' &&
        line[5] == 'b' &&
        is_space(line[6])
    ){
        forEachComponent(
            line.substr(7),
            std::numeric_limits<size_t>::max(),
            [&](const char *start, const char *end, size_t){
                parseMtllib(mesh, std::string_view(start, end - start));
            }
        );
    }
    else if (
        line[0] == 'u' &&
        line[1] == 's' &&
        line[2] == 'e' &&
        line[3] == 'm' &&
        line[4] == 't' &&
        line[5] == 'l' &&
        is_space(line[6])
    ){
        parseUsemtl(mesh, line.substr(7));
    }
}

static void parseVertex(Mesh &mesh, std::string_view line)
{
    Vec3 v = {NAN, NAN, NAN};

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
    Vec2 t = {NAN, NAN};

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
    Vec3 n = {NAN, NAN, NAN};

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
    Face f;

    // Record starting positions
    f.v.start = mesh.vertex_indices.size();
    f.vt.start = mesh.texture_indices.size();
    f.vn.start = mesh.normal_indices.size();

    f.v.len = 0;
    f.vt.len = 0;
    f.vn.len = 0;

    const char *ptr = line.data();
    const char *end = ptr + line.size();

    while (ptr < end)
    {
        // Skip whitespace
        while (ptr < end && is_space(*ptr)) ++ptr;
        if (ptr >= end) break;

        const char *token_start = ptr;
        while (ptr < end && !is_space(*ptr)) ++ptr;
        const char *token_end = ptr;
        if (token_start == token_end) continue; // skip empty tokens

        // Parse v/vt/vn
        const char *slash1 = findChar(token_start, token_end, '/');
        const char *slash2 = slash1 ? findChar(slash1 + 1, token_end, '/') : nullptr;

        int v_idx = parseIndex(token_start, slash1 ? slash1 : token_end, mesh, 0, parallel);
        int vt_idx = INT_MIN;
        int vn_idx = INT_MIN;

        if (slash1 && slash1 + 1 < token_end) {
            if (slash2) {
                vt_idx = parseIndex(slash1 + 1, slash2, mesh, 1, parallel);
                vn_idx = parseIndex(slash2 + 1, token_end, mesh, 2, parallel);
            } else {
                vt_idx = parseIndex(slash1 + 1, token_end, mesh, 1, parallel);
            }
        }

        if (v_idx != INT_MIN) { mesh.vertex_indices.push_back(v_idx); ++f.v.len; }
        if (vt_idx != INT_MIN) { mesh.texture_indices.push_back(vt_idx); ++f.vt.len; }
        if (vn_idx != INT_MIN) { mesh.normal_indices.push_back(vn_idx); ++f.vn.len; }
    }
    mesh.faces.push_back(f);
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

static void parseMtllib(Mesh &mesh, std::string_view name)
{
    // mtllib mtl_file
    // This function parses mtl_file and stores the data somewhere ->
    //   struct Material, Mesh has std::vector<Material>
    // The mtl parser should also be parallelized for large file sizes
}

static void parseUsemtl(Mesh &mesh, std::string_view material)
{
    // usemtl mtl_name
    // This function indicates that all following faces should use mtl_name
    // This requires modification to the face struct to store a material
    // This requires modification to the parallelized logic for face materials
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

            // Break chunks in reasonable places (e.g., don't separate one line across threads)
            const char *newline_ptr;
            const char *ptr = findChar(possible_end, file_end, '\n');
            if (ptr)
            {
                newline_ptr = ptr;
            }
            else
            {
                newline_ptr = file_end;
            }

            // Include the newline in the chunk
            if (newline_ptr < file_end)
                ++newline_ptr;

            chunk_end = newline_ptr;
        }

        customPushBack(chunks, {chunk_start, chunk_end});
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

    // Record old index vector sizes
    size_t v_offset  = main_mesh.vertex_indices.size();
    size_t vt_offset = main_mesh.texture_indices.size();
    size_t vn_offset = main_mesh.normal_indices.size();

    // Append indices
    main_mesh.vertex_indices.insert(main_mesh.vertex_indices.end(), partial_mesh.vertex_indices.begin(), partial_mesh.vertex_indices.end());
    main_mesh.texture_indices.insert(main_mesh.texture_indices.end(), partial_mesh.texture_indices.begin(), partial_mesh.texture_indices.end());
    main_mesh.normal_indices.insert(main_mesh.normal_indices.end(), partial_mesh.normal_indices.begin(), partial_mesh.normal_indices.end());

    // Append faces adjusting index start offset, NOT resolving 1-based & negative indices
    for (const Face &pf : partial_mesh.faces)
    {
        Face f = pf;
        if (f.v.len  > 0) f.v.start  += v_offset;
        if (f.vt.len > 0) f.vt.start += vt_offset;
        if (f.vn.len > 0) f.vn.start += vn_offset;
        customPushBack(main_mesh.faces, f);
    }
}

static void resolveFace(Face &f, Mesh &mesh)
{
    // Resolve vertex indices
    for (size_t i = 0; i < f.v.len; ++i)
    {
        int64_t &idx = mesh.vertex_indices[f.v.start + i];
        if (idx < 0)
            idx += mesh.vertices.size();
        else
            idx -= 1;
    }

    // Resolve texture indices
    for (size_t i = 0; i < f.vt.len; ++i)
    {
        int64_t &idx = mesh.texture_indices[f.vt.start + i];
        if (idx < 0)
            idx += mesh.textures.size();
        else
            idx -= 1;
    }

    // Resolve normal indices
    for (size_t i = 0; i < f.vn.len; ++i)
    {
        int64_t &idx = mesh.normal_indices[f.vn.start + i];
        if (idx < 0)
            idx += mesh.normals.size();
        else
            idx -= 1;
    }
}
