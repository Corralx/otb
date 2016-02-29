Corralx:
* Try render the triangles with the GPU using uv as vertices and outputting to RGBA8 format the triangle index
* Test the supersampling with different scales
* Use std::round insted of casting with +.5f
* Implement Gaussian blur to compensate for lower number of sample
* Multithread the raycasting
* Try to transform every double loop into a single loop (could make performance better)
* Move stb inside an external folder