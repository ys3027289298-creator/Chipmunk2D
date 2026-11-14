/* Tiny dependency-free software rasterizer for headless screen recording.
 * Renders polygon/circle bodies and a chapter title banner into an RGB frame
 * buffer that is streamed to ffmpeg as raw PPM frames. */
#ifndef CP_SOLVER_RENDER_H
#define CP_SOLVER_RENDER_H

#include "chipmunk/chipmunk_private.h"
#include <stdio.h>
#include <math.h>

#define RENDER_W 640
#define RENDER_H 480

typedef struct {
	unsigned char px[RENDER_H][RENDER_W][3];
} FrameBuffer;

/* Minimal 5x7 uppercase/digit font. */
static const unsigned char font5x7[][7] = {
	['A'] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
	['B'] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
	['C'] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
	['D'] = {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},
	['E'] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
	['F'] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
	['G'] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
	['H'] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
	['I'] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},
	['J'] = {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
	['K'] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
	['L'] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
	['M'] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
	['N'] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
	['O'] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
	['P'] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
	['Q'] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
	['R'] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
	['S'] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
	['T'] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
	['U'] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
	['V'] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
	['W'] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
	['X'] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
	['Y'] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
	['Z'] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
	['0'] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
	['1'] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
	['2'] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
	['3'] = {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
	['4'] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
	['5'] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
	['6'] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
	['7'] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
	['8'] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
	['9'] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
	[' '] = {0,0,0,0,0,0,0},
	[':'] = {0,0,0x04,0,0x04,0,0},
	['.'] = {0,0,0,0,0,0x04},
	['-'] = {0,0,0,0x1F,0,0,0},
	['/'] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
	['('] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
	[')'] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
	['%'] = {0x19,0x1A,0x02,0x04,0x08,0x0B,0x13},
	['='] = {0,0,0x1F,0,0x1F,0,0},
};

static inline void
fb_clear(FrameBuffer *fb)
{
	memset(fb->px, 245, sizeof(fb->px));
}

static inline void
fb_pixel(FrameBuffer *fb, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	if(x >= 0 && x < RENDER_W && y >= 0 && y < RENDER_H){
		fb->px[y][x][0] = r;
		fb->px[y][x][1] = g;
		fb->px[y][x][2] = b;
	}
}

static inline void
fb_rect(FrameBuffer *fb, int x0, int y0, int x1, int y1,
        unsigned char r, unsigned char g, unsigned char b)
{
	for(int y = y0; y <= y1; y++)
		for(int x = x0; x <= x1; x++)
			fb_pixel(fb, x, y, r, g, b);
}

static inline void
fb_text(FrameBuffer *fb, int x, int y, const char *text, int scale,
        unsigned char r, unsigned char g, unsigned char b)
{
	for(int cx = 0; text[cx]; cx++){
		char ch = text[cx];
		if(ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
		const unsigned char *glyph =
			((unsigned)ch < 128 && font5x7[(unsigned)ch][0] != 0)
				? font5x7[(unsigned)ch] : font5x7[' '];
		for(int gy = 0; gy < 7; gy++){
			for(int gx = 0; gx < 5; gx++){
				if(glyph[gy] & (1 << (4 - gx))){
					fb_rect(fb, x + cx*6*scale + gx*scale,
					            y + gy*scale,
					            x + cx*6*scale + gx*scale + scale - 1,
					            y + gy*scale + scale - 1, r, g, b);
				}
			}
		}
	}
}

/* Draw a filled axis-aligned-in-body box using its transform (rotation). */
static inline void
fb_body(FrameBuffer *fb, cpBody *body, cpFloat halfW, cpFloat halfH,
        unsigned char r, unsigned char g, unsigned char b,
        double scale, double ox, double oy)
{
	cpVect corners[4] = {
		cpv(-halfW, -halfH), cpv(halfW, -halfH),
		cpv(halfW, halfH), cpv(-halfW, halfH)
	};
	cpVect world[4];
	for(int i = 0; i < 4; i++){
		cpVect w = cpBodyLocalToWorld(body, corners[i]);
		world[i] = cpv(ox + w.x*scale, oy - w.y*scale);
	}
	/* Rasterize via scanline bounding box + point-in-quad test. */
	double minX = world[0].x, maxX = world[0].x;
	double minY = world[0].y, maxY = world[0].y;
	for(int i = 1; i < 4; i++){
		minX = fmin(minX, world[i].x); maxX = fmax(maxX, world[i].x);
		minY = fmin(minY, world[i].y); maxY = fmax(maxY, world[i].y);
	}
	/* Determine winding so the half-plane test points into the convex quad. */
	double signedArea = 0.0;
	for(int i = 0; i < 4; i++){
		cpVect a = world[i], c = world[(i+1)%4];
		signedArea += a.x*c.y - c.x*a.y;
	}
	double sign = (signedArea >= 0.0) ? 1.0 : -1.0;
	for(int py = (int)minY; py <= (int)maxY; py++){
		for(int px = (int)minX; px <= (int)maxX; px++){
			/* Simple convex containment via four edge half-plane tests. */
			int inside = 1;
			for(int i = 0; i < 4 && inside; i++){
				cpVect a = world[i], c = world[(i+1)%4];
				double cross = (c.x - a.x)*(py - a.y) - (c.y - a.y)*(px - a.x);
				if(sign*cross < 0) inside = 0;
			}
			if(inside) fb_pixel(fb, px, py, r, g, b);
		}
	}
}

static inline void
fb_circle(FrameBuffer *fb, cpVect center, double radius,
          unsigned char r, unsigned char g, unsigned char b,
          double scale, double ox, double oy)
{
	int cx = (int)(ox + center.x*scale);
	int cy = (int)(oy - center.y*scale);
	int rr = (int)(radius*scale);
	for(int y = -rr; y <= rr; y++)
		for(int x = -rr; x <= rr; x++)
			if(x*x + y*y <= rr*rr)
				fb_pixel(fb, cx+x, cy+y, r, g, b);
}

static inline void
fb_banner(FrameBuffer *fb, const char *chapter, const char *subtitle)
{
	fb_rect(fb, 0, 0, RENDER_W - 1, 46, 28, 32, 48);
	fb_text(fb, 12, 8, chapter, 2, 255, 255, 255);
	if(subtitle) fb_text(fb, 12, 32, subtitle, 1, 180, 210, 255);
}

static inline void
fb_write_ppm(FrameBuffer *fb, FILE *out)
{
	fprintf(out, "P6\n%d %d\n255\n", RENDER_W, RENDER_H);
	fwrite(fb->px, 1, sizeof(fb->px), out);
}

#endif
