# Occlusion and Translucency Baker

## About

Otb is a tool built to easily generate occlusion maps for a given scene, without resorting to more complicate or fully fledged 3D modelling tools. The final goal is to be able to load a scene in any common format, tweak the occlusion map parameters and save to disk the final result in any common format in a few minutes for fast iteration times.

Moreover it allows to invert the normals to approximate the translucency and bake what is generally referred to as Local Thickness Map, as seen in [Approximating Translucency for a Fast, Cheap and Convincing Subsurface Scattering Look](http://www.frostbite.com/wp-content/uploads/2013/05/Colin_BarreBrisebois_Programming_ApproximatingTranslucency.pdf) by DICE.

Under the hood it uses [Intel Embree](https://embree.github.io/) to generate the occlusion data.

**NOTE:** The tool is currently under development and is in a broken state as of now. It is undergoing a complete rewrite and more info will be given on my site/twitter!

### Final Features

* Easily generate ambient occlusion for a single mesh or the entire scene
* Every parameter of the rendering algorithm is exposed on the GUI to tweak the quality
* Preview the result directly inside the tool
* Export the occlusion data in every common image format
* Use the command line interface, the GUI application or directly link the library in your tools

### Compatibility

Otb uses OpenGL 3.3 for the visualization and it is written in C++11. It is written with maximum compatibility in mind, working on Window, Mac OS X and Linux and compiling with MSVC 2015, GCC and Clang.

## Binaries

A precompiled binary release will be available as soon as the rewrite is done and the tool reach a stable and working set of features.

## License

It is licensed under the very permissive [MIT License](https://opensource.org/licenses/MIT).
For the complete license see the provided [LICENSE](https://github.com/Corralx/helios/blob/master/LICENSE.md) file in the root directory.

## Thanks

Several open-source third-party libraries are currently used in this project:
* [Embree](https://embree.github.io/) to compute the occlusion data
* [SDL2](https://www.libsdl.org/index.php) for window and input management
* [gl3w](https://github.com/skaslev/gl3w) as OpenGL extensions loader
* [glm](http://glm.g-truc.net/0.9.7/index.html) as math library
* [stb_image](https://github.com/nothings/stb) to save the occlusion data
* [imgui](https://github.com/ocornut/imgui) as GUI library
* [rapidjson](http://rapidjson.org/) as JSON parser
* [tinyobjloader](https://syoyo.github.io/tinyobjloader/) to laod mesh in **.obj** file format
* [Remotery](https://github.com/Celtoys/Remotery) for the profiling
