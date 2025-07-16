#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

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

void exportMeshToObj(const Mesh &mesh)
{
    for (auto &v : mesh.vertices)
    {
        std::cout << "v " << v.x << ' ' << v.y << ' ' << v.z << '\n';
    }
    for (auto &t : mesh.textures)
    {
        std::cout << "vt " << t.u << ' ' << t.v << '\n';
    }
    for (auto &n : mesh.normals)
    {
        std::cout << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    }

    auto printIndex = [](int idx)
    {
        if (idx == INT_MIN)
            std::cout << "";
        else
            std::cout << idx + 1;
    };

    for (auto &f : mesh.faces)
    {
        std::cout << "f ";
        // vertex 1
        std::cout << f.v1 + 1 << '/';
        printIndex(f.vt1);
        std::cout << '/';
        printIndex(f.vn1);
        std::cout << ' ';
        // vertex 2
        std::cout << f.v2 + 1 << '/';
        printIndex(f.vt2);
        std::cout << '/';
        printIndex(f.vn2);
        std::cout << ' ';
        // vertex 3
        std::cout << f.v3 + 1 << '/';
        printIndex(f.vt3);
        std::cout << '/';
        printIndex(f.vn3);
        std::cout << '\n';
    }
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
