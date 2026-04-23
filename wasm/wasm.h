#ifndef WASM_H
#define WASM_H

#include "../main.h"

FileContent wasmOctreeDeserialize(char* name);
Texture wasmLoadImage(String path);

void wasmPolygon(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color);
void wasmPolygon3d(DrawSurface* surface,Vec3* coordinats,Vec3 color);

#endif
