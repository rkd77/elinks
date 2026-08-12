/*

# Sixel codec. I'm lazy, so no decoder yet.
#
# "Regular" mode just encodes the image as a sixel image, with
# Cha-Image-Sixel-Palette colors. If that isn't given, it's set
# according to Cha-Image-Quality.
#
# The encoder also has a "half-dump" mode, where a binary lookup table
# is appended to the file end to allow vertical cropping in ~constant
# time.
#
# This table is an array of 32-bit big-endian integers indicating the
# start index of every sixel, and finally a 32-bit big-endian integer
# indicating the number of sixels in the image.
#
# Warning: we intentionally leak the final octree. Be careful if you
# want to integrate this module into a larger program. Deallocation
# would (currently) look like:
#
# * Free the leaves first, since they might have been inserted more
#   than once (iterate over "nodes" seq)
# * Recurse to free the parent nodes (start from root, dealloc each
#   node where idx == -1)

*/

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <vector>

#include <iostream>
#include <memory_resource>

#include "elinks.h"

#include "terminal/sixel2.h"
#include "util/memory.h"
#include "util/string.h"

static std::pmr::monotonic_buffer_resource resource;

const char *DCS = "\eP";
const char *ST = "\e\\";

uint32_t
rgb(uint8_t r, uint8_t g, uint8_t b)
{
	return (r << 16) | (g << 8) | b;
}

struct RGBColor {
	uint8_t r;
	uint8_t g;
	uint8_t b;

	RGBColor(uint32_t c = 0) { r = (c >> 16) & 255; g = (c >> 8) & 255; b = c & 255; }
	operator uint32_t() const { return (r << 16) | (g << 8) | b | (100 << 24); }
	friend std::ostream & operator<< (std::ostream &out, const RGBColor &c);
};

struct ARGBColor {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;

	ARGBColor(uint32_t c = 0) { r = (c >> 16) & 255; g = (c >> 8) & 255; b = (c & 255); a = (c >> 24) & 255;}
	operator uint32_t() const { return (r << 16) | (g << 8) | b | (a << 24); }
	uint32_t rgb() { return (r << 16) | (g << 8) | b; }
	friend std::ostream & operator<< (std::ostream &out, const ARGBColor &c);
};

struct RGBAColorBE {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;

	friend std::ostream & operator<< (std::ostream &out, const RGBAColorBE &c);

};

std::ostream & operator<< (std::ostream &out, const RGBAColorBE &c)
{
	return out << "(r: " << (int)c.r << ", g: " << (int)c.g << ", b: " << (int)c.b << ", a: " << (int)c.a << ")";
}

std::ostream & operator<< (std::ostream &out, const ARGBColor &c)
{
	out.precision(std::numeric_limits<double>::digits10 + 2);
	return out << "rgba(" << (int)c.r << ", " << (int)c.g << ", " << (int)c.b << ", " << double(c.a) / 255.0 << ")";
}

std::ostream & operator<< (std::ostream &out, const RGBColor &c)
{
	out.precision(std::numeric_limits<double>::digits10 + 2);
	return out << "rgba(" << (int)c.r << ", " << (int)c.g << ", " << (int)c.b << ", " << 1 << ")";
}

struct NodeLeaf {
	RGBColor c;
	uint32_t n;
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

struct NodeObj;

typedef NodeObj* Node;
typedef Node NodeChildren[8];

union NodeUnion {
	struct NodeLeaf leaf;
	NodeChildren children;

	NodeUnion(): children() {};
};

struct NodeObj {
	int idx; // -1 parent, anything else leaf
	union NodeUnion u;

	NodeObj(): idx(0), u() {};
};

std::pmr::polymorphic_allocator<NodeObj> alloc(&resource);

#if 0
type
  Node = ptr NodeObj

  NodeObj = object
    idx: int # -1: parent, anything else: leaf
    u: NodeUnion

  NodeUnion {.union.} = object
    leaf: NodeLeaf
    children: NodeChildren

  NodeChildren = array[8, Node]

  NodeLeaf = object
    c: RGBColor
    n: uint32
    r: uint32
    g: uint32
    b: uint32
#endif

static uint8_t
getIdx(RGBColor c1, int level)
{
	//std::cerr << "getIdx c=" << c1 << " level=" << level << "\n";
	uint32_t c = (uint32_t)c1 & (0x80808080 >> (uint32_t)level);
	uint8_t res = (uint8_t)((c >> (21 - level)) | (c >> (14 - level)) | (c >> (7 - level)));
	//std::cerr << "res=" << (int)res << "\n";
	return res;
}

#if 0
proc getIdx(c: RGBColor; level: int): uint8 {.inline.} =
  let c = uint32(c) and (0x80808080u32 shr uint32(level))
  return uint8((c shr (21 - level)) or
    (c shr (14 - level)) or
    (c shr (7 - level)))
#endif

static Node
new_node_object(void)
{
	Node node = alloc.allocate(1);
	::new (node) NodeObj();

	return node;
}

static unsigned int
insert(Node *root, RGBColor c, std::pmr::vector<Node> *trimMap)
{
	// # max level is 7, because we only have ~6.5 bits (0..100, inclusive)
	// # (it *is* 0-indexed, but one extra level is needed for the final leaves)
	unsigned int level = 0;
	Node *parent = (Node *)root;
	bool split = false;

	while (1) {
		assert(level < 8);
		//std::cerr << "insert c=" << c << "\n";
		int idx = getIdx(c, level);
		Node old = parent[idx];

		if (old == nullptr) {
			Node node = new_node_object();

			//Node node = new NodeObj();
			//if (!node) {
				////return 2000;
			//}
			node->idx = 0;
			node->u.leaf = {
				.c = c,
				.n = 1,
				.r = (uint32_t)(c.r),
				.g = (uint32_t)(c.g),
				.b = (uint32_t)(c.b)
			};
			parent[idx] = node;
			return 1;
		} else if (old->idx != -1) {
			// split just once with identical colors
			if (level == 7 || split && old->u.leaf.c == c) {
				old->u.leaf.n++;
				old->u.leaf.r += (uint32_t)(c.r);
				old->u.leaf.g += (uint32_t)(c.g);
				old->u.leaf.b += (uint32_t)(c.b);
				break;
			}
			RGBColor oc = old->u.leaf.c;
			Node child = new_node_object();
			///Node child = new NodeObj();

			//if (!child) {
				//return 3000;
			//}
			child->idx = 0;
			copy_struct(&(child->u.leaf), &(old->u.leaf));
			old->idx = -1;
			memset(&(old->u.children), 0, sizeof(NodeChildren));
			old->u.children[getIdx(oc, level + 1)] = child;
			trimMap[level].push_back(old);
			split = true;
		}
		level++;
		parent = (Node *)&(old->u.children);
	}
	return 0;
}

#if 0
proc insert(root: var NodeChildren; c: RGBColor; trimMap: var TrimMap): uint =
  # max level is 7, because we only have ~6.5 bits (0..100, inclusive)
  # (it *is* 0-indexed, but one extra level is needed for the final leaves)
  var level = 0
  var parent = addr root
  var split = false
  while true:
    assert level < 8
    let idx = c.getIdx(level)
    let old = parent[idx]
    if old == nil:
      let node = cast[Node](alloc(sizeof(NodeObj)))
      node.idx = 0
      node.u.leaf = NodeLeaf(
        c: c,
        n: 1,
        r: uint32(c.r),
        g: uint32(c.g),
        b: uint32(c.b)
      )
      parent[idx] = node
      return 1
    elif old.idx != -1:
      # split just once with identical colors
      if level == 7 or split and old.u.leaf.c == c:
        inc old.u.leaf.n
        old.u.leaf.r += uint32(c.r)
        old.u.leaf.g += uint32(c.g)
        old.u.leaf.b += uint32(c.b)
        break
      let oc = old.u.leaf.c
      let child = cast[Node](alloc(sizeof(NodeObj)))
      child.idx = 0
      child.u.leaf = old.u.leaf
      old.idx = -1
      zeroMem(addr old.u.children, sizeof(old.u.children))
      old.u.children[oc.getIdx(level + 1)] = child
      trimMap[level].add(old)
      split = true
    inc level
    parent = addr old.u.children
  0
#endif

static void
trim(std::pmr::vector<Node> *trimMap, unsigned int &K)
{
	Node node = nullptr;
	int i;

	for (i = 6; i >= 0; i--) {
		//std::cerr << "trim i=" << i << "\n";
		//std::cerr << "trimMap.size=" << trimMap[i].size() << "\n";
		if (trimMap[i].size() > 0) {
			node = (Node)trimMap[i].back();
			trimMap[i].pop_back();

			break;
		}
	}
	unsigned int r = 0;
	unsigned int g = 0;
	unsigned int b = 0;
	unsigned int n = 0;
	unsigned int k = K + 1;

	for (i = 0; i < 8; i++) {
		auto child = node->u.children[i];
		if (child) {
			r += child->u.leaf.r;
			g += child->u.leaf.g;
			b += child->u.leaf.b;
			n += child->u.leaf.n;
			//delete(child);
			node->u.children[i] = nullptr;
			k--;
		}
	}
	node->idx = 0;
	node->u.leaf = {
		.c = rgb(uint8_t(r / n), uint8_t(g / n), uint8_t(b / n)),
		.n = n,
		.r = r,
		.g = g,
		.b = b
	};
	K = k;
	//std::cerr << "trim K=" << K << "\n";
}

#if 0
proc trim(trimMap: var TrimMap; K: var uint) =
  var node: Node = nil
  for i in countdown(trimMap.high, 0):
    if trimMap[i].len > 0:
      node = trimMap[i].pop()
      break
  var r = 0u32
  var g = 0u32
  var b = 0u32
  var n = 0u32
  var k = K + 1
  for child in node.u.children:
    if child != nil:
      r += child.u.leaf.r
      g += child.u.leaf.g
      b += child.u.leaf.b
      n += child.u.leaf.n
      dealloc(child)
      dec k
  node.idx = 0
  node.u.leaf = NodeLeaf(
    c: rgb(uint8(r div n), uint8(g div n), uint8(b div n)),
    r: r,
    g: g,
    b: b,
    n: n
  )
  K = k
#endif

struct OpenArray {
	RGBAColorBE *data;
	int len;
};


static RGBColor
rgb(ARGBColor c)
{
	return RGBColor(uint32_t(c) & 0xFFFFFF);
}

static ARGBColor
rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return ARGBColor((uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b));
}

static ARGBColor
argb(RGBColor c, uint8_t a)
{
	return ARGBColor((uint32_t(c) & 0x00FFFFFF) | (uint32_t(a) << 24));
}

static ARGBColor
argb(RGBColor c)
{
	return ARGBColor((uint32_t)(c) | 0xFF000000);
}

static ARGBColor
rgba(int r, int g, int b, int a)
{
	return rgba(uint8_t(r), uint8_t(g), uint8_t(b), uint8_t(a));
}

static ARGBColor
argb(RGBAColorBE c)
{
	return rgba(c.r, c.g, c.b, c.a);
}

static ARGBColor
fastmul(ARGBColor c, uint32_t n)
{
	uint64_t c1 = (uint64_t(c) << 24) | uint64_t(c);
	c1 &= 0x00FF00FF00FF00FF;
	c1 *= n;
	c1 += 0x80008000800080;
	c1 += (c1 >> 8) & 0x00FF00FF00FF00FF;
	c1 &= 0xFF00FF00FF00FF00;
	c1 = (c1 >> 32) | (c1 >> 8);
	return ARGBColor(c1);
}

static Node *
quantize(RGBAColorBE *img, int length, unsigned int &outk, bool &outTransparent)
{
	//Node *root = (Node *)new NodeChildren;
	Node *root = static_cast<Node *>(resource.allocate(8 * sizeof(Node), alignof(Node)));

	if (!root) {
		return root;
	}
	memset(root, 0, sizeof(NodeChildren));

	if (outk <= 2) {
		root[0] = (Node)(new_node_object());
		root[0]->u.leaf.c = rgb(0, 0, 0);
		root[7] = (Node)(new_node_object());
		root[7]->u.leaf.c = rgb(100, 100, 100);
		outk = 2;
		// # the point is to skip the first scan, so fall back to the option
		// # that always works
		outTransparent = true;
		return root;
	}
	// number of leaves
	unsigned int palette = outk;
	unsigned int K = 0;
	//# map of non-leaves for each level.
	//# (note: somewhat confusingly, this actually starts at level 1.)
	std::pmr::vector<Node> trimMap[7] = {}; // = (std::pmr::vector<Node> *)(new std::pmr::vector<Node>[7]);
	bool transparent = false;

	for (int i = 0; i < length; i++) {
		ARGBColor c0 = argb(img[i]);
		c0 = fastmul(c0,  100);
		transparent = transparent || c0.a != 100;
		RGBColor c(c0);
		K += insert(root, c, trimMap);

		while (K > palette) {
			trim(trimMap, K);
		}
	}
	outk = K;
	outTransparent = transparent;
	return root;
}

#if 0
proc quantize(img: openArray[RGBAColorBE]; outk: var uint;
    outTransparent: var bool): NodeChildren =
  var root = NodeChildren.default
  if outk <= 2: # monochrome; not much we can do with an octree...
    root[0] = cast[Node](alloc0(sizeof(NodeObj)))
    root[0].u.leaf.c = rgb(0, 0, 0)
    root[7] = cast[Node](alloc0(sizeof(NodeObj)))
    root[7].u.leaf.c = rgb(100, 100, 100)
    outk = 2
    # the point is to skip the first scan, so fall back to the option
    # that always works
    outTransparent = true
    return root
  # number of leaves
  let palette = outk
  var K = 0u
  # map of non-leaves for each level.
  # (note: somewhat confusingly, this actually starts at level 1.)
  var trimMap: array[7, seq[Node]]
  var transparent = false
  for c0 in img:
    let c0 = c0.argb().fastmul(100)
    transparent = transparent or c0.a != 100
    let c = RGBColor(c0)
    K += root.insert(c, trimMap)
    while K > palette:
      trimMap.trim(K)
  outk = K
  outTransparent = transparent
  return root

#endif

static void
flatten(Node *children, std::pmr::vector<Node> &cols)
{
	for (int i = 0; i < 8; i++) {
		Node node = children[i];
		if (node) {
			if (node->idx != -1) {
				cols.push_back(node);
			} else {
				flatten(node->u.children, cols);
			}
		}
	}
}

#if 0
proc flatten(children: NodeChildren; cols: var seq[Node]) =
  for node in children:
    if node != nil:
      if node.idx != -1:
        cols.add(node)
      else:
        node.u.children.flatten(cols)
#endif

static bool
cmp(Node a, Node b)
{
	return a->u.leaf.n > b->u.leaf.n;
}

static std::pmr::vector<Node>
flatten(Node *root, struct string *outs, unsigned int palette)
{
	std::pmr::vector<Node> cols{&resource};
	cols.reserve(palette);
	flatten(root, cols);

	sort(cols.begin(), cols.end(), cmp);
	//  # try to set the most common colors as the smallest numbers (so we write less)
	//cols.sort(proc(a, b: Node): int = cmp(a.u.leaf.n, b.u.leaf.n),
	//order = Descending)

	for (int n = 0; n < cols.size(); n++) {
		auto it = cols.at(n);
		auto c = it->u.leaf.c;
		// 2 is RGB
		add_format_to_string(outs, "#%d;2;%d;%d;%d", n, c.r, c.g, c.b);
		it->idx = n;
	}
	return cols;
}

#if 0
proc flatten(root: NodeChildren; outs: var string; palette: uint): seq[Node] =
  var cols = newSeqOfCap[Node](palette)
  root.flatten(cols)
  # try to set the most common colors as the smallest numbers (so we write less)
  cols.sort(proc(a, b: Node): int = cmp(a.u.leaf.n, b.u.leaf.n),
    order = Descending)
  for n, it in cols:
    let c = it.u.leaf.c
    # 2 is RGB
    outs &= '#' & $n & ";2;" & $c.r & ';' & $c.g & ';' & $c.b
    it.idx = n
  return cols
#endif

struct DitherDiff {
	int32_t a;
	int32_t r;
	int32_t g;
	int32_t b;

	friend std::ostream & operator<< (std::ostream &out, const DitherDiff &d);
};

std::ostream & operator<< (std::ostream &out, const DitherDiff &d)
{
	return out << "(a: " << d.a << ", r: " << d.r << ", g: " << d.g << ", b: " << d.b << ")";
}

struct Dither {
	std::pmr::vector<DitherDiff> d1{&resource};
	std::pmr::vector<DitherDiff> d2{&resource};

	friend std::ostream & operator<< (std::ostream &out, const Dither &d);
};
std::ostream & operator<< (std::ostream &out, const Dither &d)
{
	out << "(d1: @[";
	auto br = "";
	for (auto it = d.d1.begin(); it != d.d1.end(); it++) {
		out << br << *it;
		br = ", ";
	}
	br = "";
	out << "], d2: @[";
	for (auto it = d.d2.begin(); it != d.d2.end(); it++) {
		out << br << *it;
		br = ", ";
	}

	return	out << "])";
}

#if 0
type
  DitherDiff = tuple[a, r, g, b: int32]

  Dither = object
    d1: seq[DitherDiff]
    d2: seq[DitherDiff]
#endif

//typedef uint32_t ARGBColor;

static Node
getColor(std::pmr::vector<Node> &nodes, ARGBColor c, DitherDiff *diff)
{
	Node child = nullptr;
	uint32_t minDist = UINT32_MAX;
	DitherDiff mdiff = {0};

	//std::cerr << "mdiff=" << mdiff << "\n";
	//std::cerr << "minDist=" << minDist << "\n";

	//std::cerr << "getColor c=" << c << "\n";

	for (auto node: nodes) {
		RGBColor ic = node->u.leaf.c;

		//std::cerr << "ic=" << ic << "\n";

		auto ad = int32_t(c.a) - 100;
		auto rd = int32_t(c.r) - int32_t(ic.r);
		auto gd = int32_t(c.g) - int32_t(ic.g);
		auto bd = int32_t(c.b) - int32_t(ic.b);
		auto d = uint32_t(abs(rd)) + uint32_t(abs(gd)) + uint32_t(abs(bd));

		//std::cerr << "d=" << d << "\n";
		//std::cerr << "minDist=" << minDist << "\n";

		if (d < minDist) {
			minDist = d;

			//std::cerr << "minDist1=" << minDist << "\n";

			child = node;
			mdiff = {ad, rd, gd, bd};

			//std::cerr << "ic=" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << (uint32_t)ic << std::dec << "\n";
			//std::cerr << "c.rgb()=" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << (uint32_t)c.rgb() << std::dec << "\n";

			//std::cerr << "cond=" << std::boolalpha << (uint32_t(ic) == uint32_t(c.rgb())) << "\n";

			if (uint32_t(ic) == uint32_t(c.rgb())) {
				//std::cerr << "break\n";
				break;
			}
		}
	}
	*diff = mdiff;
	//std::cerr << "getColor diff=" << *diff << "\n";
	return child;
}

#if 0
proc getColor(nodes: seq[Node]; c: ARGBColor; diff: var DitherDiff): Node =
  var child: Node = nil
  var minDist = uint32.high
  var mdiff = DitherDiff.default
  for node in nodes:
    let ic = node.u.leaf.c
    let ad = int32(c.a) - 100
    let rd = int32(c.r) - int32(ic.r)
    let gd = int32(c.g) - int32(ic.g)
    let bd = int32(c.b) - int32(ic.b)
    let d = uint32(abs(rd)) + uint32(abs(gd)) + uint32(abs(bd))
    if d < minDist:
      minDist = d
      child = node
      mdiff = (ad, rd, gd, bd)
      if ic == c.rgb():
        break
  diff = mdiff
  return child
#endif

static int
getColor(Node *root, ARGBColor c, std::pmr::vector<Node> &nodes, DitherDiff *diff)
{
	//std::cerr << "nodes.size=" << nodes.size() << "\n";
	if (nodes.size() < 64) {
//    # Octree-based nearest neighbor search creates really ugly artifacts
//    # with a low amount of colors, which is exactly the case where
//    # linear search is still acceptable.
//    #
//    # 64 is the first power of 2 that gives OK results on my test images
//    # with the octree.
//    #
//    # (In practice, I assume no sane terminal would pick a palette (> 2)
//    # that isn't a multiple of 4, so really only 16 is relevant here.
//    # Even that is quite rare, unless you misconfigure XTerm - or
//    # have a hardware terminal, but those didn't have private color
//    # registers in the first place. I do like the aesthetics, though;
//    # would be a shame if it didn't work :P)
		return getColor(
			nodes,
			c,
			diff)
			->idx;
	}
//  # Find a matching color in the octree.
//  # Not as accurate as a linear search, but good enough (and much
//  # faster) for palettes that reach this path.
	unsigned int level = 0;
	Node *children = (Node *)root;

	while (1) {
		auto idx = getIdx(RGBColor(c), level);
		Node child = children[idx];

		if (child == nullptr) {
			child = getColor(nodes, c, diff);
			// # No child found in this corner of the octree. This is caused by
			// # dithering.
			// # Allocate at least 4 ancestors, so that other colors with the
			// # same initial bits don't end up using something wildly different
			// # than the dither intended.
			while (level < 4) {
				idx = getIdx(RGBColor(c), level);
				Node node = new_node_object();

				if (!node) {
					return -1;
				}
				node->idx = -1;
				children[idx] = node;
				children = (Node *)&node->u.children;
				level++;
			}
			idx = getIdx(RGBColor(c), level);
			children[idx] = child;
			return child->idx;
		}

		if (child->idx != -1) {
			RGBColor ic = child->u.leaf.c;
			auto a = int32_t(c.a) - 100;
			auto r = int32_t(c.r) - int32_t(ic.r);
			auto g = int32_t(c.g) - int32_t(ic.g);
			auto b = int32_t(c.b) - int32_t(ic.b);
			*diff = {a, r, g, b};

			return child->idx;
		}
		level++;
		children = (Node *)child->u.children;
	}
	return -1; //unreachable
}

#if 0
/*
proc getColor(root: var NodeChildren; c: ARGBColor; nodes: seq[Node];
    diff: var DitherDiff): int =
  if nodes.len < 64:
    # Octree-based nearest neighbor search creates really ugly artifacts
    # with a low amount of colors, which is exactly the case where
    # linear search is still acceptable.
    #
    # 64 is the first power of 2 that gives OK results on my test images
    # with the octree.
    #
    # (In practice, I assume no sane terminal would pick a palette (> 2)
    # that isn't a multiple of 4, so really only 16 is relevant here.
    # Even that is quite rare, unless you misconfigure XTerm - or
    # have a hardware terminal, but those didn't have private color
    # registers in the first place. I do like the aesthetics, though;
    # would be a shame if it didn't work :P)
    return nodes.getColor(c, diff).idx
  # Find a matching color in the octree.
  # Not as accurate as a linear search, but good enough (and much
  # faster) for palettes that reach this path.
  var level = 0
  var children = addr root
  while true:
    let idx = RGBColor(c).getIdx(level)
    let child = children[idx]
    if child == nil:
      let child = nodes.getColor(c, diff)
      # No child found in this corner of the octree. This is caused by
      # dithering.
      # Allocate at least 4 ancestors, so that other colors with the
      # same initial bits don't end up using something wildly different
      # than the dither intended.
      while level < 4:
        let idx = RGBColor(c).getIdx(level)
        let node = cast[Node](alloc0(sizeof(NodeObj)))
        node.idx = -1
        children[idx] = node
        children = addr node.u.children
        inc level
      let idx = RGBColor(c).getIdx(level)
      children[idx] = child
      return child.idx
    if child.idx != -1:
      let ic = child.u.leaf.c
      let a = int32(c.a) - 100
      let r = int32(c.r) - int32(ic.r)
      let g = int32(c.g) - int32(ic.g)
      let b = int32(c.b) - int32(ic.b)
      diff = (a, r, g, b)
      return child.idx
    inc level
    children = addr child.u.children
  -1 # unreachable
*/
#endif

static int32_t
clamp(int32_t n, int32_t lo, int32_t hi)
{
	return std::min(std::max(n, lo), hi);
}

static ARGBColor
correctDither(ARGBColor c, int x, Dither *dither)
{
	DitherDiff tmp = dither->d1.at(x + 1);

	auto pa = (uint32_t(c) >> 20) & 0xFF0;
	auto pr = (uint32_t(c) >> 12) & 0xFF0;
	auto pg = (uint32_t(c) >> 4) & 0xFF0;
	auto pb = (uint32_t(c) << 4) & 0xFF0;

	auto a = (uint8_t)(uint32_t)(clamp(int32_t(pa) + tmp.a, 0, 1600) >> 4);
	auto r = (uint8_t)(uint32_t)(clamp(int32_t(pr) + tmp.r, 0, 1600) >> 4);
	auto g = (uint8_t)(uint32_t)(clamp(int32_t(pg) + tmp.g, 0, 1600) >> 4);
	auto b = (uint8_t)(uint32_t)(clamp(int32_t(pb) + tmp.b, 0, 1600) >> 4);

	//std::cerr << "correctDither:" << c << " " << x << " " << (int)r << "," << (int)g << "," << (int)b << "," << (int)a << "\n";

	return rgba(r, g, b, a);
}

#if 0
proc correctDither(c: ARGBColor; x: int; dither: Dither): ARGBColor =
  let (ad, rd, gd, bd) = dither.d1[x + 1]
  let pa = (uint32(c) shr 20) and 0xFF0
  let pr = (uint32(c) shr 12) and 0xFF0
  let pg = (uint32(c) shr 4) and 0xFF0
  let pb = (uint32(c) shl 4) and 0xFF0
  {.push overflowChecks: off.}
  let a = uint8(uint32(clamp(int32(pa) + ad, 0, 1600)) shr 4)
  let r = uint8(uint32(clamp(int32(pr) + rd, 0, 1600)) shr 4)
  let g = uint8(uint32(clamp(int32(pg) + gd, 0, 1600)) shr 4)
  let b = uint8(uint32(clamp(int32(pb) + bd, 0, 1600)) shr 4)
  {.pop.}
  return rgba(r, g, b, a)
#endif


#define at2(p, mul) \
	tmp = p; \
	p.a = tmp.a + d.a * mul; \
	p.r = tmp.r + d.r * mul; \
	p.g = tmp.g + d.g * mul; \
	p.b = tmp.b + d.b * mul;

static void
fs(Dither *dither, int x, DitherDiff d)
{
	DitherDiff tmp;
	x++;
	at2(dither->d1.at(x + 1), 7)
	at2(dither->d2.at(x - 1), 3)
	at2(dither->d2.at(x), 5)
	at2(dither->d2.at(x + 1), 1)

	//std::cerr << "fs:" << *dither << "\n";

}
#if 0
proc fs(dither: var Dither; x: int; d: DitherDiff) =
  let x = x + 1 # skip first bounds check
  template at(p, mul: untyped) =
    var (ad, rd, gd, bd) = p
    p = (ad + d.a * mul, rd + d.r * mul, gd + d.g * mul, bd + d.b * mul)
  {.push overflowChecks: off.}
  at(dither.d1[x + 1], 7)
  at(dither.d2[x - 1], 3)
  at(dither.d2[x], 5)
  at(dither.d2[x + 1], 1)
  {.pop.}
#endif

struct SixelChunk {
	int x;
	int c;
	unsigned int nrow;
	unsigned int *data;
	int len;
	struct SixelChunk *next;

	SixelChunk(): x(0), c(0), nrow(0), data(nullptr), len(0), next(nullptr) {};
};

struct SixelBand {
	struct SixelChunk *head;
	struct SixelChunk *tail;
};

#if 0

type
  SixelBand = object
    head: ptr SixelChunk
    tail: ptr SixelChunk

  SixelChunk = object
    x: int
    c: int
    nrow: uint
    # data is binary 0..63; compressSixel creates the final ASCII form
    data: seq[uint8]
    # linked list for chaining together bands
    # (yes, this *is* faster than a seq.)
    next: ptr SixelChunk
#endif

static void
compressSixel(struct string *outs, SixelBand *band)
{
	int x = 0;
	SixelChunk *chunk = band->head;

	while (chunk != nullptr) {
		add_format_to_string(outs, "#%d", chunk->c);
		auto n = chunk->x - x;
		unsigned char c = '?';
		for (int i = 0; i < chunk->len; i++) {
			unsigned int u = chunk->data[i];
			char cc = (char)(u + 0x3F);
			if (c != cc) {
				if (n > 3) {
					add_format_to_string(outs, "!%d%c", n, c);
				} else {
					for (int i = 0; i < n; i++) {
						add_char_to_string(outs, c);
					}
				}
				c = cc;
				n = 0;
			}
			n++;
		}
		if (n > 3) {
			add_format_to_string(outs, "!%d%c", n, c);
		} else {
			for (int i = 0; i < n; i++) {
				add_char_to_string(outs, c);
			}
		}
		x = chunk->x + chunk->len;
		auto next = chunk->next;
		chunk->next = nullptr;
		chunk = next;
	}
}

#if 0
proc compressSixel(outs: var string; band: SixelBand) =
  var x = 0
  var chunk = band.head
  while chunk != nil:
    outs &= '#' & $chunk.c
    var n = chunk.x - x
    var c = '?'
    for u in chunk.data:
      let cc = char(u + 0x3F)
      if c != cc:
        if n > 3:
          outs &= '!' & $n & c
        else:
          for i in 0 ..< n:
            outs &= c
        c = cc
        n = 0
      inc n
    if n > 3:
      outs &= '!' & $n & c
    else:
      for i in 0 ..< n:
        outs &= c
    x = chunk.x + chunk.data.len
    let next = chunk.next
    chunk.next = nil
    chunk = next
#endif


static void
createBands(std::pmr::vector<SixelBand *> &bands, std::pmr::vector<SixelChunk *> &activeChunk)
{
	for (SixelChunk *chunk: activeChunk) {
		bool found = false;
		for (SixelBand *band: bands) {
			if (band->head->x > chunk->x + chunk->len) {
				chunk->next = band->head;
				band->head = chunk;
				found = true;
				break;
			} else if (band->tail->x + band->tail->len <= chunk->x) {
				band->tail->next = chunk;
				band->tail = chunk;
				found = true;
				break;
			}
		}
		if (!found) {
			SixelBand *no = static_cast<SixelBand *>(resource.allocate(1 * sizeof(SixelBand)));
			memset(no, 0, sizeof(SixelBand));
			no->head = chunk;
			no->tail = chunk;
			bands.push_back(no);
		}
	}
}

#if 0
proc createBands(bands: var seq[SixelBand]; activeChunks: seq[ptr SixelChunk]) =
  for chunk in activeChunks:
    var found = false
    for band in bands.mitems:
      if band.head.x > chunk.x + chunk.data.len:
        chunk.next = band.head
        band.head = chunk
        found = true
        break
      elif band.tail.x + band.tail.data.len <= chunk.x:
        band.tail.next = chunk
        band.tail = chunk
        found = true
        break
    if not found:
      bands.add(SixelBand(head: chunk, tail: chunk))
#endif

void
encode(struct string *outs, unsigned char *img, int width, int height, int offx, int offy, int realw, unsigned int palette)
{
	//std::cerr << "width=" << width << " height=" << height << " offx=" << offx << " offy=" << offy << " realw=" << realw << "\n";

	bool transparent = false;
	Node *root = (Node *)quantize((RGBAColorBE *)img, width * height, palette, transparent);
//
//	add_format_to_string(&outs, "Cha-Image-Sixel-Transparent: %d\n", (int)(transparent));
//	add_to_string(&outs, "Cha-Image-Sixel-Prelude-Len: ");
//	char *PreludePad = "666 666 666";
//	int preludeLenPos = outs.length;
//	add_to_string(&outs, PreludePad);
//	add_to_string(&outs, "\n\n");
//	int dcsPos = outs.length;

	RGBAColorBE *img2 = (RGBAColorBE *)img;

	add_to_string(outs, DCS);
	if (transparent) {
		add_to_string(outs, "0;1");
	}
	add_char_to_string(outs, 'q');
	add_format_to_string(outs, "\"1;1;%d;%d", realw, height - offy);
	std::pmr::vector<Node> nodes = flatten(root, outs, palette);

	auto L = width * height;
//	auto realw = cropw - offx;
	auto n = offy * width;


	uint32_t totalLen = 0;
	Dither dither;
	dither.d1 = std::pmr::vector<DitherDiff>(realw + 2);
	dither.d2 = std::pmr::vector<DitherDiff>(realw + 2);

	SixelChunk chunkMap[palette];
	std::pmr::vector<SixelChunk *> activeChunks;
	unsigned int nrow = 1;

//	const MaxBuffer = 65536;

	while (1) {
//		if (halfDump) {
//
//			putU32BE(ymap, totalLen)
//		}
		for (int i = 0; i < 6; i++) {
			//std::cerr << "i=" << i << "\n";

			if (n >= L) {
				break;
			}

			uint8_t mask = 1 << i;
			SixelChunk *chunk = nullptr;

			//std::cerr << "mask=" << (int)mask << " ";

			for (int j = 0; j < realw; j++) {

				//std::cerr << "j=" <<  j << " ";

				auto m = n + offx + j;

				//std::cerr << "m=" <<  m << " ";

				RGBAColorBE c0r = img2[m];

				//std::cerr << "c0im=" << c0r << "\n";

				ARGBColor c0 = argb(img2[m]);

				//std::cerr << "c0argb=" << c0 << "\n";

				c0 = fastmul(c0, 100);

				//std::cerr << "c0fast=" << c0 << "\n";

				correctDither(c0, j, &dither);

				//std::cerr << "c0=" << c0 << "\n";

				if (c0.a < 50) { //transparent
					DitherDiff diff = { int32_t(c0.a), 0, 0, 0 };
					fs(&dither, j, diff);
					chunk = nullptr;
					continue;
				}
				DitherDiff diff = {0};
				int c = getColor(root, c0, nodes, &diff);
				fs(&dither, j, diff);

				//std::cerr << "c=" << c << "\n";

				//if (chunk) {
					//std::cerr << "chunk->c=" << chunk->c << "\n";
				//}

				if (chunk == nullptr || chunk->c != c) {
					chunk = &chunkMap[c];
					if (chunk->nrow < nrow) {
						//std::cerr << "chunk->nrow < nrow\n";
						chunk->c = c;
						chunk->nrow = nrow;
						chunk->x = j;
						chunk->len = 0;
						mem_free_set(&chunk->data, NULL);
						activeChunks.push_back(chunk);
					} else if (chunk->x > j) {
						//std::cerr << "chunk->x > j\n";
						auto diff = chunk->x - j;
						chunk->x = j;
						int olen = chunk->len;
						chunk->data = (unsigned int *)mem_realloc(chunk->data, (olen + diff) * sizeof(*chunk->data));

						if (!chunk->data) {
							return;
						}

						memmove(&(chunk->data[diff]), &(chunk->data[0]), olen * sizeof(*chunk->data));
						memset(&(chunk->data[0]), 0, diff * sizeof(*chunk->data));
						chunk->len = (olen + diff);
					} else if (chunk->len < j - chunk->x) {
						//std::cerr << "chunk->len < j - chunk->x\n";
						int olen = chunk->len;
						chunk->data = (unsigned int *)mem_realloc(chunk->data, (j - chunk->x) * sizeof(*chunk->data));

						if (!chunk->data) {
							return;
						}
						memset(&(chunk->data[olen]), 0, (j - chunk->x - olen) * sizeof(*chunk->data));
						chunk->len = (j - chunk->x);
					}
				}
				int k = j - chunk->x;

				if (k < chunk->len) {
					chunk->data[k] |= mask;
				} else {
					chunk->data = (unsigned int *)mem_realloc(chunk->data, (chunk->len + 1) * sizeof(*chunk->data));

					if (!chunk->data) {
						return;
					}

					chunk->data[chunk->len] = mask;
					chunk->len++;
				}
				//std::cerr << "chunk->data:";
				//for (int iii = 0; iii < chunk->len; iii++) {
				//	std::cerr << " " << chunk->data[iii];
				//}
				//std::cerr << "\n";
			}
			n += width;
			auto tmp = dither.d1;
			dither.d1 = dither.d2;
			dither.d2 = tmp;
			memset(&(dither.d2.data()[0]), 0, dither.d2.size() * sizeof(unsigned int));
		}
		std::pmr::vector<SixelBand *> bands{&resource};
		createBands(bands, activeChunks);
		//int olen = outs->length;

		for (int i = 0; i < bands.size(); i++) {
			if (i > 0) {
				add_char_to_string(outs, '$');
			}
			compressSixel(outs, bands.at(i));
		}
		activeChunks.clear();

		if (n >= L) {
			add_to_string(outs, ST);
			//totalLen += (uint32_t)(outs->length - olen);
			break;
		} else {
			add_char_to_string(outs, '-');
			//totalLen += (uint32_t)(outs->length - olen);
		}
		//if outs.len >= MaxBuffer:
		//os.puts(outs)
		//outs.setLen(0)
		nrow++;
	}
	dither.d1.clear();
	dither.d2.clear();
	for (int iii = 0; iii < palette; iii++) {
		SixelChunk *chunk = &chunkMap[iii];
		mem_free_if(chunk->data);
	}
	resource.release();
}

#if 0
/*
proc encode(os: PosixStream; img: openArray[RGBAColorBE];
    width, height, offx, offy, cropw, palette: int; halfdump: bool) =
  var palette = uint(palette)
  var transparent = false
  var root = img.quantize(palette, transparent)
  # prelude
  var outs = "Cha-Image-Sixel-Transparent: " & $int(transparent) & "\n"
  outs &= "Cha-Image-Sixel-Prelude-Len: "
  const PreludePad = "666 666 666"
  let preludeLenPos = outs.len
  outs &= PreludePad & "\n\n"
  let dcsPos = outs.len
  outs &= DCS
  if transparent:
    outs &= "0;1" # P2=1 -> image has transparency
  outs &= 'q'
  # set raster attributes
  outs &= "\"1;1;" & $width & ';' & $height
  let nodes = root.flatten(outs, palette)
  # prepend prelude size
  var ps = $(outs.len - dcsPos)
  while ps.len < PreludePad.len:
    ps &= ' '
  for i, c in ps:
    outs[preludeLenPos + i] = c
  let L = width * height
  let realw = cropw - offx
  var n = offy * width
  var ymap = ""
  var totalLen = 0u32
  # add +2 so we don't have to bounds check
  var dither = Dither(
    d1: newSeq[DitherDiff](realw + 2),
    d2: newSeq[DitherDiff](realw + 2)
  )
  var chunkMap = newSeq[SixelChunk](palette)
  var activeChunks: seq[ptr SixelChunk] = @[]
  var nrow = 1u
  # buffer to 64k, just because.
  const MaxBuffer = 65536
  while true:
    if halfdump:
      ymap.putU32BE(totalLen)
    for i in 0 ..< 6:
      if n >= L:
        break
      let mask = 1u8 shl i
      var chunk: ptr SixelChunk = nil
      for j in 0 ..< realw:
        let m = n + offx + j
        let c0 = img[m].argb().fastmul(100).correctDither(j, dither)
        if c0.a < 50: # transparent
          let diff = (int32(c0.a), 0i32, 0i32, 0i32)
          dither.fs(j, diff)
          chunk = nil
          continue
        var diff: DitherDiff
        let c = root.getColor(c0, nodes, diff)
        dither.fs(j, diff)
        if chunk == nil or chunk.c != c:
          chunk = addr chunkMap[c]
          if chunk.nrow < nrow:
            chunk.c = c
            chunk.nrow = nrow
            chunk.x = j
            chunk.data.setLen(0)
            activeChunks.add(chunk)
          elif chunk.x > j:
            let diff = chunk.x - j
            chunk.x = j
            let olen = chunk.data.len
            chunk.data.setLen(olen + diff)
            moveMem(addr chunk.data[diff], addr chunk.data[0], olen)
            zeroMem(addr chunk.data[0], diff)
          elif chunk.data.len < j - chunk.x:
            let olen = chunk.data.len
            chunk.data.setLen(j - chunk.x)
            when NimMajor < 2:
              zeroMem(addr chunk.data[olen], j - chunk.x - olen)
            else:
              discard olen
        let k = j - chunk.x
        if k < chunk.data.len:
          chunk.data[k] = chunk.data[k] or mask
        else:
          chunk.data.add(mask)
      n += width
      var tmp = move(dither.d1)
      dither.d1 = move(dither.d2)
      dither.d2 = move(tmp)
      zeroMem(addr dither.d2[0], dither.d2.len * sizeof(dither.d2[0]))
    var bands: seq[SixelBand] = @[]
    bands.createBands(activeChunks)
    let olen = outs.len
    for i in 0 ..< bands.len:
      if i > 0:
        outs &= '$'
      outs.compressSixel(bands[i])
    if n >= L:
      outs &= ST
      totalLen += uint32(outs.len - olen)
      break
    else:
      outs &= '-'
      totalLen += uint32(outs.len - olen)
      if outs.len >= MaxBuffer:
        os.puts(outs)
        outs.setLen(0)
    inc nrow
    activeChunks.setLen(0)
  if halfdump:
    ymap.putU32BE(totalLen)
    ymap.putU32BE(uint32(ymap.len))
    outs &= ymap
  os.puts(outs)
  # Note: we leave octree deallocation to the OS. See the header for details.
*/
#endif

#if 0
/*
proc main() =
  let os = newPosixStream(STDOUT_FILENO)
  if paramCount() != 1:
    cgiDie(ceInternalError, "usage: sixel [command]")
  if paramStr(1) == "encode":
    var width = 0
    var height = 0
    var offx = 0
    var offy = 0
    var halfdump = false
    var palette = -1
    var cropw = -1
    var quality = -1
    for hdr in getEnvEmpty("REQUEST_HEADERS").split('\n'):
      let s = hdr.after(':').strip()
      case hdr.until(':')
      of "Cha-Image-Dimensions":
        (width, height) = parseDimensions(s, allowZero = false)
      of "Cha-Image-Offset":
        (offx, offy) = parseDimensions(s, allowZero = true)
      of "Cha-Image-Crop-Width":
        let q = parseUInt32(s, allowSign = false)
        if q.isErr:
          cgiDie(ceInternalError, "wrong crop width")
        cropw = int(q.get)
      of "Cha-Image-Sixel-Halfdump":
        halfdump = true
      of "Cha-Image-Sixel-Palette":
        let q = parseUInt16(s, allowSign = false)
        if q.isErr:
          cgiDie(ceInternalError, "wrong palette")
        palette = int(q.get)
      of "Cha-Image-Quality":
        let q = parseUInt16(s, allowSign = false)
        if q.isErr:
          cgiDie(ceInternalError, "wrong quality")
        quality = int(q.get)
    if cropw == -1:
      cropw = width
    if palette == -1:
      if quality < 30:
        palette = 16
      elif quality < 70:
        palette = 256
      else:
        palette = 1024
    if width == 0 or height == 0:
      quit(0) # done...
    let n = width * height
    let L = n * 4
    let ps = newPosixStream(STDIN_FILENO)
    let src = ps.readLoopOrMmap(L)
    if src == nil:
      cgiDie(ceInternalError, "failed to read input")
    enterNetworkSandbox() # don't swallow stat
    let p = cast[ptr UncheckedArray[RGBAColorBE]](src.p)
    os.encode(p.toOpenArray(0, n - 1), width, height, offx, offy, cropw,
      palette, halfdump)
    deallocMem(src)
  else:
    cgiDie(ceInternalError, "not implemented")

main()

{.pop.} # raises: []
*/
#endif
