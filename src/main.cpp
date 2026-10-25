#include "./utils/rtweekend.h"
#include "./utils/camera.h"
#include "./utils/hittable.h"
#include "./utils/hittable_list.h"
#include "./utils/material.h"
#include "./utils/sphere.h"
#include "./utils/bvh.h"
#include <string>

int main(int argc, char* argv[]) {
    // pass in image_width and num_threads as arguments
    if (argc != 4) { // expect 3 arguments: program_name, image_width, num_threads
        std::cerr << "Usage: " << argv[0] << " <image_width> <num_threads>\n";
        return 1;
    }

    int image_width = std::stoi(argv[1]);
    int num_threads = std::stoi(argv[2]);
    int use_bvh = std::stoi(argv[3]);

    // world setup
    hittable_list world;

    auto material_ground = make_shared<lambertian>(color(0.8, 0.8, 0.0));
    auto material_center = make_shared<lambertian>(color(0.1, 0.2, 0.5));
    auto material_left   = make_shared<dielectric>(1.50);
    auto material_bubble = make_shared<dielectric>(1.00 / 1.50);
    auto material_right  = make_shared<metal>(color(0.8, 0.6, 0.2), 1.0);

    world.add(make_shared<sphere>(point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(make_shared<sphere>(point3( 0.0,    0.0, -1.2),   0.5, material_center));
    world.add(make_shared<sphere>(point3(-1.0,    0.0, -1.0),   0.5, material_left));
    world.add(make_shared<sphere>(point3(-1.0,    0.0, -1.0),   0.4, material_bubble));
    world.add(make_shared<sphere>(point3( 1.0,    0.0, -1.0),   0.5, material_right));

    // cam setup
    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = image_width;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;


    cam.vfov     = 60; // change to zoom in
    cam.lookfrom = point3(-2,2,1);
    cam.lookat   = point3(0,0,-1);
    cam.vup      = vec3(0,1,0);

    cam.defocus_angle = 10.0;
    cam.focus_dist    = 3.4;

    // int num_threads = cam.image_width; // thread per row
    // int num_threads = 1;

    // cam.render(world);
    if (use_bvh) {
        bvh world_bvh(world);
        cam.render(world_bvh, num_threads);
    } else {
        cam.render(world, num_threads);
    }
}