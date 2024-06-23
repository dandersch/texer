#pragma once
/*
   IDEA: a texture generation api in C that mimicks the following javascript api:

   texture(0x468f).create(); // Sky
   texture(0x340f).grunge().create(); // Grass
   texture(0x340f).grunge().noise(0x108a, 1.5).create(); // Grass dark
   texture(0x236f).noise(0x1005, 4.5).noise(Ox100a, 2.5).create(); // Water

   inspiration:
   https://x.com/phoboslab/status/1432363394418491394/photo/1
*/

/* USAGE
   texture_t tex = TEXTURE_CREATE(texture(color), noise(2), ....);
*/

/*
  Limitations:
  - color (first argument) cannot be specified as an initializer containing commas, e.g. TEXTURE_CREATE(texture((color_t){0.2,0.3,0.5}), ...)
  - requires GNU C-extension of statement expressions
  - ...
*/

/* Possible extensions:
 * tex_copy(tex): deep copy a texture
 * stencil/blit(t,x,y): draws tex t to the texture at (x,y)
 *
 * circle: draw a filled circle at position with radius and color
 * line:   draw a line segment between two points with a color
 * blend:  blend two textures together with a specified blending mode (e.g., alpha blending, additive blending)
 * resize: resize the texture to a new width and height using interpolation techniques (e.g., bilinear interpolation).
 * rotate: rotate the texture by a specified angle around a pivot point.
 * flip:   flip texture horizontally or vertically
 *
 * - generate normal maps along texture for operations that add depth (e.g. insets)
 */

#include <stddef.h>

// NOTE RGBA vs BGRA layout could be set with a macro (?)
typedef struct color_t { float r; float g; float b; float a; } color_t;
typedef struct texture_t { size_t width; size_t height; color_t* rgb; } texture_t;

/* api */
texture_t texture(int w, int h, color_t rgb);
#define noise(...) _noise(&temp, __VA_ARGS__)
void _noise(texture_t* tex, float intensity);
#define grunge(...) _grunge(&temp, __VA_ARGS__)
void _grunge(texture_t* tex, float intensity);
#define smear(...) _smear(&temp, ##__VA_ARGS__)
void _smear(texture_t* tex, color_t rgb);

#define rect(...) _rect(&temp, __VA_ARGS__)
void _rect(texture_t* tex, unsigned int x, unsigned int y, unsigned int width, unsigned int height, color_t color);

#define circle(...) _circle(&temp, __VA_ARGS__)
void _circle(texture_t* tex, unsigned int x, unsigned int y, unsigned int radius, color_t color);

#define flip(...) _flip(&temp, ##__VA_ARGS__)
void _flip(texture_t* tex);

#define mirror(...) _mirror(&temp, ##__VA_ARGS__)
void _mirror(texture_t* tex);

#define tex_build(tex, ...) ({texture_t temp = tex; __VA_ARGS__; _create(temp); })

/* internal */
#ifdef TEX_BUILDER_IMPLEMENTATION
#include <stdlib.h> // for malloc & rand()
#include <math.h>   // for RAND_MAX, ...
void _noise(texture_t* tex, float intensity)  {
    srand(1);

    for (size_t i = 0; i < tex->width * tex->height; i++) {
        // Add noise to each color component based on intensity
        tex->rgb[i].r += intensity * ((float)rand() / (float) RAND_MAX - 0.5f);
        tex->rgb[i].g += intensity * ((float)rand() / (float) RAND_MAX - 0.5f);
        tex->rgb[i].b += intensity * ((float)rand() / (float) RAND_MAX - 0.5f);

        // Ensure color components are within [0, 1] range
        tex->rgb[i].r = fminf(fmaxf(tex->rgb[i].r, 0.0f), 1.0f);
        tex->rgb[i].g = fminf(fmaxf(tex->rgb[i].g, 0.0f), 1.0f);
        tex->rgb[i].b = fminf(fmaxf(tex->rgb[i].b, 0.0f), 1.0f);
    }
}

void _rect(texture_t* tex, unsigned int x, unsigned int y, unsigned int width, unsigned int height, color_t color)  {
    /* check if rectangle fits into the texture */
    if (x + width > tex->width || y + height > tex->height) { return; }

    /* fill rectangle region */
    for (unsigned int i = y; i < y + height; i++) {
        for (unsigned int j = x; j < x + width; j++) {
            size_t index = i * tex->width + j;
            tex->rgb[index] = color;
        }
    }
}

void _circle(texture_t* tex, unsigned int x, unsigned int y, unsigned int radius, color_t color) {
    // filled circle using midpoint circle algorithm
    int cx = radius - 1;
    int cy = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);

    while (cx >= cy) {
        for (int i = x - cx; i <= x + cx; i++) {
            if (i >= 0 && i < tex->width && y + cy >= 0 && y + cy < tex->height) {
                size_t index = (y + cy) * tex->width + i;
                tex->rgb[index] = color;
            }
            if (i >= 0 && i < tex->width && y - cy >= 0 && y - cy < tex->height) {
                size_t index = (y - cy) * tex->width + i;
                tex->rgb[index] = color;
            }
        }

        for (int i = x - cy; i <= x + cy; i++) {
            if (i >= 0 && i < tex->width && y + cx >= 0 && y + cx < tex->height) {
                size_t index = (y + cx) * tex->width + i;
                tex->rgb[index] = color;
            }
            if (i >= 0 && i < tex->width && y - cx >= 0 && y - cx < tex->height) {
                size_t index = (y - cx) * tex->width + i;
                tex->rgb[index] = color;
            }
        }

        if (err <= 0) {
            cy++;
            err += dy;
            dy += 2;
        }

        if (err > 0) {
            cx--;
            dx += 2;
            err += dx - (radius << 1);
        }
    }
}

void _mirror(texture_t* tex) {
    color_t* mirrored = (color_t*) malloc(tex->width * tex->height * sizeof(color_t));

    /* flip each row horizontally */
    for (size_t y = 0; y < tex->height; y++) {
        for (size_t x = 0; x < tex->width; x++) {
            mirrored[y * tex->width + x] = tex->rgb[y * tex->width + (tex->width - 1 - x)];
        }
    }

    for (size_t i = 0; i < tex->width * tex->height; i++) {
        tex->rgb[i] = mirrored[i];
    }

    free(mirrored);
}

void _flip(texture_t* tex) {
    color_t* flipped = (color_t*)malloc(tex->width * tex->height * sizeof(color_t));

    /* flip each column vertically */
    for (size_t y = 0; y < tex->height; y++) {
        for (size_t x = 0; x < tex->width; x++) {
            flipped[y * tex->width + x] = tex->rgb[(tex->height - 1 - y) * tex->width + x];
        }
    }

    for (size_t i = 0; i < tex->width * tex->height; i++) {
        tex->rgb[i] = flipped[i];
    }

    free(flipped);
}

void _grunge(texture_t* tex, float intensity) {
    tex->rgb[0].b += intensity;
}

void _smear(texture_t* tex, color_t rgb) {
    tex->rgb[0] = rgb;
}

texture_t texture(int w, int h, color_t rgba)
{
    texture_t tex;
    tex.width       = w;
    tex.height      = h;
    tex.rgb         = malloc(w * h * sizeof(color_t));
    for (int i = 0; i < (w * h); i++)
    {
        tex.rgb[i] = rgba;
    }
    return tex;
}

texture_t _create(texture_t tex) { return tex; }
#endif
