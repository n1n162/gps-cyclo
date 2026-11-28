#include "gvt.h"
#include <math.h>
#include <string.h>

static uint16_t u16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t u32le(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t u64le(const uint8_t *p) { return (uint64_t)u32le(p) | ((uint64_t)u32le(p + 4) << 32); }

int gvt_open(gvt_t *g, gvt_read_fn read, void *ctx)
{
    uint8_t h[GVT_HEADER_SIZE];
    if (!g || !read) return -1;
    memset(g, 0, sizeof(*g));
    if (read(ctx, 0, h, sizeof h) != 0) return -1;
    if (memcmp(h, "GVT1", 4) != 0 || (u16le(h + 4) != 1 && u16le(h + 4) != GVT_VERSION) || u16le(h + 6) != GVT_EXTENT) return -1;
    g->read = read;
    g->ctx = ctx;
    g->version = u16le(h + 4);
    g->extent = u16le(h + 6);
    g->zmin = h[8];
    g->zmax = h[9];
    g->n_tiles = u32le(h + 12);
    g->index_offset = u32le(h + 16);
    g->west_e7 = (int32_t)u32le(h + 20);
    g->south_e7 = (int32_t)u32le(h + 24);
    g->east_e7 = (int32_t)u32le(h + 28);
    g->north_e7 = (int32_t)u32le(h + 32);
    if (g->zmin > g->zmax || g->zmax > 16 || g->index_offset < GVT_HEADER_SIZE) return -1;
    return 0;
}

int gvt_load_index(gvt_t *g, gvt_entry_t *buf, uint32_t capacity)
{
    uint8_t chunk[GVT_ENTRY_SIZE * 32];
    uint32_t done = 0;
    if (!g || !g->read || !buf || capacity < g->n_tiles) return -1;
    while (done < g->n_tiles) {
        uint32_t n = g->n_tiles - done;
        if (n > 32) n = 32;
        if (g->read(g->ctx, g->index_offset + done * GVT_ENTRY_SIZE, chunk, n * GVT_ENTRY_SIZE) != 0) return -1;
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t *p = chunk + i * GVT_ENTRY_SIZE;
            gvt_entry_t *e = &buf[done + i];
            e->key = u64le(p);
            e->offset = u32le(p + 8);
            e->size = u32le(p + 12);
            if (done + i > 0 && e->key <= buf[done + i - 1].key) return -1;   /* index trie strictement */
        }
        done += n;
    }
    g->index = buf;
    return 0;
}

const gvt_entry_t *gvt_find(const gvt_t *g, int z, uint32_t x, uint32_t y)
{
    uint64_t key = GVT_KEY(z, x, y);
    uint32_t lo = 0, hi;
    if (!g || !g->index) return NULL;
    hi = g->n_tiles;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (g->index[mid].key == key) return &g->index[mid];
        if (g->index[mid].key < key) lo = mid + 1; else hi = mid;
    }
    return NULL;
}

/* Charge un bloc contigu de 32 entrees (512 octets) ou le retrouve dans le cache LRU. */
static gvt_index_cache_block_t *get_index_block(gvt_t *g, uint32_t i)
{
    uint32_t first = (i / GVT_INDEX_CACHE_ENTRIES) * GVT_INDEX_CACHE_ENTRIES;
    uint32_t count = g->n_tiles - first;
    int victim = 0;
    uint8_t raw[GVT_ENTRY_SIZE * GVT_INDEX_CACHE_ENTRIES];
    uint64_t off;

    if (count > GVT_INDEX_CACHE_ENTRIES) count = GVT_INDEX_CACHE_ENTRIES;
    for (uint32_t b = 0; b < GVT_INDEX_CACHE_BLOCKS; b++) {
        gvt_index_cache_block_t *block = &g->index_cache[b];
        if (block->valid && block->first == first) {
            block->stamp = ++g->index_cache_clock;
            g->index_cache_hits++;
            return block;
        }
        if (!g->index_cache[victim].valid ||
            (block->valid && block->stamp < g->index_cache[victim].stamp))
            victim = (int)b;
    }

    off = (uint64_t)g->index_offset + (uint64_t)first * GVT_ENTRY_SIZE;
    if (off > UINT32_MAX ||
        g->read(g->ctx, (uint32_t)off, raw, count * GVT_ENTRY_SIZE) != 0)
        return NULL;

    gvt_index_cache_block_t *block = &g->index_cache[victim];
    for (uint32_t j = 0; j < count; j++) {
        const uint8_t *p = raw + j * GVT_ENTRY_SIZE;
        block->entries[j].key = u64le(p);
        block->entries[j].offset = u32le(p + 8);
        block->entries[j].size = u32le(p + 12);
        if (j && block->entries[j].key <= block->entries[j - 1].key) return NULL;
    }
    block->first = first;
    block->stamp = ++g->index_cache_clock;
    block->valid = 1;
    g->index_cache_loads++;
    return block;
}

/* Acces a une entree de l'index : RAM complete si disponible, sinon cache de blocs SD. */
static int get_index_entry(gvt_t *g, uint32_t i, gvt_entry_t *out)
{
    if (!g || !out || i >= g->n_tiles) return -1;
    if (g->index) { *out = g->index[i]; return 0; }
    if (!g->read) return -1;
    gvt_index_cache_block_t *block = get_index_block(g, i);
    if (!block) return -1;
    *out = block->entries[i - block->first];
    return 0;
}

int gvt_find_entry(gvt_t *g, int z, uint32_t x, uint32_t y, gvt_entry_t *out)
{
    uint64_t key;
    uint32_t lo = 0, hi;
    if (!g || !out || z < g->zmin || z > g->zmax || x > 65535u || y > 65535u) return 0;
    key = GVT_KEY(z, x, y);
    hi = g->n_tiles;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        gvt_entry_t e;
        if (get_index_entry(g, mid, &e) != 0) return -1;
        if (e.key == key) { *out = e; return 1; }
        if (e.key < key) lo = mid + 1; else hi = mid;
    }
    return 0;
}

int gvt_read_tile(const gvt_t *g, const gvt_entry_t *e, uint8_t *buf, uint32_t capacity)
{
    if (!g || !g->read || !e || !buf || e->size > capacity) return -1;
    if (g->read(g->ctx, e->offset, buf, e->size) != 0) return -1;
    return (int)e->size;
}

void gvt_tile_of(int z, double lat, double lon, uint32_t *x, uint32_t *y, float *fx, float *fy)
{
    double n = (double)(1u << z);
    double lr, wx, wy;
    if (lat > 85.0511287798) lat = 85.0511287798;
    if (lat < -85.0511287798) lat = -85.0511287798;
    lr = lat * M_PI / 180.0;
    wx = (lon + 180.0) / 360.0 * n;
    wy = (1.0 - log(tan(lr) + 1.0 / cos(lr)) / M_PI) / 2.0 * n;
    if (wx < 0.0) wx = 0.0;
    if (wy < 0.0) wy = 0.0;
    if (wx > n - 1e-9) wx = n - 1e-9;
    if (wy > n - 1e-9) wy = n - 1e-9;
    if (x) *x = (uint32_t)wx;
    if (y) *y = (uint32_t)wy;
    if (fx) *fx = (float)((wx - floor(wx)) * GVT_EXTENT);
    if (fy) *fy = (float)((wy - floor(wy)) * GVT_EXTENT);
}

static int get_varint(gvt_tile_it_t *it, uint32_t *v)
{
    uint32_t r = 0;
    for (int shift = 0; shift <= 28; shift += 7) {
        uint8_t b;
        if (it->p >= it->end) return -1;
        b = *it->p++;
        r |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) { *v = r; return 0; }
    }
    return -1;
}

static int get_zigzag(gvt_tile_it_t *it, int32_t *v)
{
    uint32_t u;
    if (get_varint(it, &u) != 0) return -1;
    *v = (int32_t)(u >> 1) ^ -(int32_t)(u & 1);
    return 0;
}

int gvt_tile_begin(gvt_tile_it_t *it, const uint8_t *blob, uint32_t size)
{
    if (!it || !blob) return -1;
    it->p = blob;
    it->end = blob + size;
    it->remaining = 0;
    return get_varint(it, &it->remaining) == 0 ? 0 : -1;
}

/* Lit n points relatifs (premier absolu, puis differences). Si xy est NULL, les points sont seulement sautes. */
static int read_points(gvt_tile_it_t *it, uint32_t n, int16_t *xy)
{
    int32_t x = 0, y = 0;
    for (uint32_t i = 0; i < n; i++) {
        int32_t a, b;
        if (get_zigzag(it, &a) != 0 || get_zigzag(it, &b) != 0) return -1;
        x = i ? x + a : a;
        y = i ? y + b : b;
        if (x < -GVT_EXTENT || x > 2 * GVT_EXTENT || y < -GVT_EXTENT || y > 2 * GVT_EXTENT) return -1;
        if (xy) { xy[2 * i] = (int16_t)x; xy[2 * i + 1] = (int16_t)y; }
    }
    return 0;
}

/* Lit l'enregistrement suivant du type demande (want_area) en sautant les autres. */
static int next_record(gvt_tile_it_t *it, int want_area, uint8_t *cls, uint16_t *count, uint16_t *ring_len, int16_t *xy, uint16_t capacity)
{
    if (!it || !cls || !count || !xy) return -1;
    while (it->remaining > 0) {
        const uint8_t *start = it->p;
        uint8_t c, flags;
        uint32_t n;
        if (it->end - it->p < 2) return -1;
        c = it->p[0];
        flags = it->p[1];
        it->p += 2;
        if (c == 0 || c >= GVT_CLASS_COUNT || flags > 1) return -1;
        if (flags == 0) {                                        /* ligne */
            if (c >= GVT_AREA_URBAN) return -1;
            if (get_varint(it, &n) != 0 || n < 2 || n > GVT_MAX_POINTS) return -1;
            if (want_area) {
                if (read_points(it, n, NULL) != 0) return -1;
                it->remaining--;
                continue;
            }
            if (n > capacity) { it->p = start; return -2; }
            if (read_points(it, n, xy) != 0) return -1;
            *cls = c;
            *count = (uint16_t)n;
            it->remaining--;
            return 1;
        } else {                                                 /* surface (version 2) */
            uint32_t rings, total = 0, lens[GVT_MAX_RINGS];
            if (c < GVT_AREA_URBAN) return -1;
            if (get_varint(it, &rings) != 0 || rings < 1 || rings > GVT_MAX_RINGS) return -1;
            if (!want_area) {
                for (uint32_t r = 0; r < rings; r++) {
                    if (get_varint(it, &n) != 0 || n < 3 || (total += n) > GVT_MAX_AREA_POINTS) return -1;
                    if (read_points(it, n, NULL) != 0) return -1;
                }
                it->remaining--;
                continue;
            }
            {   /* on mesure d'abord (sans consommer) pour pouvoir refuser un tampon trop petit */
                const uint8_t *mark = it->p;
                for (uint32_t r = 0; r < rings; r++) {
                    if (get_varint(it, &n) != 0 || n < 3 || (total += n) > GVT_MAX_AREA_POINTS) return -1;
                    lens[r] = n;
                    if (read_points(it, n, NULL) != 0) return -1;
                }
                if (total > capacity) { it->p = start; return -2; }
                it->p = mark;
            }
            {
                int16_t *dst = xy;
                for (uint32_t r = 0; r < rings; r++) {
                    if (get_varint(it, &n) != 0 || n != lens[r]) return -1;
                    if (read_points(it, n, dst) != 0) return -1;
                    ring_len[r] = (uint16_t)n;
                    dst += 2 * n;
                }
            }
            *cls = c;
            *count = (uint16_t)rings;
            it->remaining--;
            return 1;
        }
    }
    return it->p == it->end ? 0 : -1;
}

int gvt_tile_next(gvt_tile_it_t *it, uint8_t *cls, uint16_t *n_points, int16_t *xy, uint16_t capacity)
{
    return next_record(it, 0, cls, n_points, NULL, xy, capacity);
}

int gvt_tile_next_area(gvt_tile_it_t *it, uint8_t *cls, uint16_t *n_rings, uint16_t *ring_len, int16_t *xy, uint16_t capacity)
{
    if (!ring_len) return -1;
    return next_record(it, 1, cls, n_rings, ring_len, xy, capacity);
}
