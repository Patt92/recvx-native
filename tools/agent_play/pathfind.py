#!/usr/bin/env python3
"""Pathfinding over a dump's collision tables (walls), filtered the way collision.cpp move_walls checks them.

Grid of 1.0 units, obstacles inflated by the player radius 3.5 (+0.5 margin), 8-neighbour Dijkstra with
diagonal costs, then line-of-sight smoothing. Returns waypoints for the runtime action `route`.
Filter as move_walls: flags&1, type<=5, attr&1 -> own floor only, attr&4 skipped for the player
(stflg 0x40000000); height: the wall must overlap the body [y, y+16.5] (e.y 0 = room height).
"""
import heapq, math

RADIUS = 4.0
HEIGHT = 16.5


def active_walls(d):
    px, py, pz = d['actor']; floor = d.get('player_floor', 0)
    out = []
    for w in d.get('walls', []):
        if not (w['fl'] & 1) or w['ty'] > 5: continue
        if (w['attr'] & 1) and w['floor'] != floor: continue
        if w['attr'] & 4: continue
        h = w['e'][1] if w['e'][1] != 0 else 1e9
        if py + HEIGHT < w['p'][1] or py > w['p'][1] + h: continue
        out.append(w)
    return out


def _seg_dist(px, pz, ax, az, bx, bz):
    vx, vz = bx - ax, bz - az; L = vx * vx + vz * vz
    t = 0.0 if L == 0 else max(0.0, min(1.0, ((px - ax) * vx + (pz - az) * vz) / L))
    return math.hypot(px - (ax + t * vx), pz - (az + t * vz))


def _blocked_fn(w, r):
    p, e, ty = w['p'], w['e'], w['ty']
    if ty <= 1:
        x0, x1 = sorted((p[0], p[0] + e[0])); z0, z1 = sorted((p[2], p[2] + e[2]))
        return (x0 - r, z0 - r, x1 + r, z1 + r), lambda x, z: math.hypot(max(x0 - x, 0, x - x1), max(z0 - z, 0, z - z1)) < r
    if ty <= 3:
        R = abs(e[0]) + r; cx, cz = p[0], p[2]
        return (cx - R, cz - R, cx + R, cz + R), lambda x, z: math.hypot(x - cx, z - cz) < R
    a, b, c = (p[0], p[2]), (p[0] + e[0], p[2]), (p[0], p[2] + e[2])
    def inside(x, z):
        def s(p1, p2): return (x - p2[0]) * (p1[1] - p2[1]) - (p1[0] - p2[0]) * (z - p2[1])
        d1, d2, d3 = s(a, b), s(b, c), s(c, a)
        if not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0)): return True
        return min(_seg_dist(x, z, *a, *b), _seg_dist(x, z, *b, *c), _seg_dist(x, z, *c, *a)) < r
    xs = [a[0], b[0], c[0]]; zs = [a[1], b[1], c[1]]
    return (min(xs) - r, min(zs) - r, max(xs) + r, max(zs) + r), inside


class Grid:
    def __init__(self, d, cell=1.0, radius=RADIUS):
        walls = active_walls(d); self.cell = cell
        shapes = [_blocked_fn(w, radius) for w in walls]
        xs = [s[0][0] for s in shapes] + [s[0][2] for s in shapes] + [d['actor'][0]]
        zs = [s[0][1] for s in shapes] + [s[0][3] for s in shapes] + [d['actor'][2]]
        self.x0, self.z0 = math.floor(min(xs)), math.floor(min(zs))
        self.W = int(math.ceil((max(xs) - self.x0) / cell)) + 1; self.H = int(math.ceil((max(zs) - self.z0) / cell)) + 1
        self.block = bytearray(self.W * self.H)
        for (bx0, bz0, bx1, bz1), fn in shapes:
            i0, j0 = self.idx(bx0, bz0); i1, j1 = self.idx(bx1, bz1)
            for j in range(max(j0, 0), min(j1, self.H - 1) + 1):
                z = self.z0 + (j + .5) * cell
                for i in range(max(i0, 0), min(i1, self.W - 1) + 1):
                    if not self.block[j * self.W + i] and fn(self.x0 + (i + .5) * cell, z): self.block[j * self.W + i] = 1

    def idx(self, x, z): return int((x - self.x0) / self.cell), int((z - self.z0) / self.cell)
    def pos(self, i, j): return self.x0 + (i + .5) * self.cell, self.z0 + (j + .5) * self.cell
    def free(self, i, j): return 0 <= i < self.W and 0 <= j < self.H and not self.block[j * self.W + i]

    def nearest_free(self, i, j, limit=12):
        if self.free(i, j): return i, j
        for r in range(1, limit):
            best = None
            for dj in range(-r, r + 1):
                for di in range(-r, r + 1):
                    if max(abs(di), abs(dj)) == r and self.free(i + di, j + dj):
                        dd = di * di + dj * dj
                        if best is None or dd < best[0]: best = (dd, i + di, j + dj)
            if best: return best[1], best[2]
        return None

    def los(self, a, b):
        (i0, j0), (i1, j1) = a, b; n = int(max(abs(i1 - i0), abs(j1 - j0)) * 2) + 1
        for k in range(n + 1):
            t = k / n
            if not self.free(int(round(i0 + (i1 - i0) * t)), int(round(j0 + (j1 - j0) * t))): return False
        return True

    def path(self, sx, sz, tx, tz):
        s = self.nearest_free(*self.idx(sx, sz)); g = self.nearest_free(*self.idx(tx, tz))
        if not s or not g: return None, 'start or goal blocked'
        dist = {s: 0.0}; prev = {}; pq = [(0.0, s)]
        while pq:
            c, u = heapq.heappop(pq)
            if u == g: break
            if c > dist[u]: continue
            for di, dj, w in ((1, 0, 1), (-1, 0, 1), (0, 1, 1), (0, -1, 1), (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)):
                v = (u[0] + di, u[1] + dj)
                if not self.free(*v): continue
                if di and dj and not (self.free(u[0] + di, u[1]) and self.free(u[0], u[1] + dj)): continue
                nc = c + w
                if nc < dist.get(v, 1e18): dist[v] = nc; prev[v] = u; heapq.heappush(pq, (nc + math.hypot(v[0] - g[0], v[1] - g[1]) * 0, v))
        if g not in dist: return None, 'no path'
        cells = [g]
        while cells[-1] != s: cells.append(prev[cells[-1]])
        cells.reverse()
        pts = [cells[0]]; k = 0
        while k < len(cells) - 1:
            m = len(cells) - 1
            while m > k + 1 and not self.los(cells[k], cells[m]): m -= 1
            pts.append(cells[m]); k = m
        return [self.pos(*p) for p in pts[1:]], dist[g] * self.cell
