#ifndef CAMERA_H
#define CAMERA_H
//
// Updated October 2026 by Ryan Wang <ryanjun.wang@outlook.com>
// 
// Added multi-threading functionality to camera rendering.
// ==============================================================================================
// Originally written in 2016 by Peter Shirley <ptrshrl@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public Domain Dedication
// along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include "hittable.h"
#include "material.h"
#include "vec3.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

// responsible for 2 things:
// 1. generate and send rays into the world
// 2. render the scene using ray results
class camera {
  public:
    /* public camera parameters*/

    double aspect_ratio = 1.0; // image width/height ratio
    int image_width = 100; // rendered image width in pixel count
    int samples_per_pixel = 10; // random samples per any particular pixel
    int max_depth = 10; // max recursion depth for ray tracing (i.e. max ray bounces)

    double vfov = 90; // vertical fov angle
    point3 lookfrom = point3(0,0,0); // point the camera looks from
    point3 lookat = point3(0,0,-1); // point the camera looks at

    // this is "world up", and keeps our camera horizontally level
    vec3 vup = vec3(0,1,0); // camera-relative "up" direction

    double defocus_angle = 0; // variation angle of rays through each pixel
    double focus_dist = 10; // distance from camera lookfrom point to plane of perfect focus
    
    void render(const hittable& world) {
        auto start_time = std::chrono::steady_clock::now();

        initialize(); // we will call initialize on render (could also call it ouside if we wanted)

        std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++) {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
            for (int i = 0; i < image_width; i++) {
                // auto pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
                // auto ray_direction = pixel_center - center;
                // ray r(center, ray_direction);

                // color pixel_color = ray_color(r, world);
                // write_color(std::cout, pixel_color);

                // want to calculate average color per pixel via samples
                color pixel_color(0,0,0);
                for (int sample = 0; sample < samples_per_pixel; sample++) {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, max_depth, world);
                }
                write_color(std::cout, pixel_samples_scale*pixel_color); // basically average out color
            }

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::clog << "\nRendering time: " << duration.count() << " ms\n";
        }
    }

    // compare pixel-level threads, to row-level threads
    color render_pixel(int i, int j, const hittable& world, color& pixel_color) {
        // render a single pixel at (i, j)
        // modify pixel_color
        for (int sample = 0; sample < samples_per_pixel; sample++) {
            ray r = get_ray(i, j);
            pixel_color += ray_color(r, max_depth, world);
        }
        return pixel_samples_scale*pixel_color;
    }
    void render_pixel_chunk(const hittable& world, color* chunk_pixels, int start_pixel, int end_pixel, std::atomic<int>& pixels_processed) {
        // render a single chunk of pixels 
        // modify chunk_pixels[i] for each pixel i in chunk
        for (int pixel = start_pixel; pixel < end_pixel; pixel++) {
            int i = pixel % image_width;
            int j = pixel / image_width;
            // we can back-calculate i,j from thread# and relative pixel
            chunk_pixels[pixel] = render_pixel(i, j, world, chunk_pixels[pixel]);

            // update progress counter
            pixels_processed += 1;
        }
    }
    
    // render the scene using num_threads threads
    void render(const hittable& world, int num_threads) {
        auto start_time = std::chrono::steady_clock::now();

        // setup camera
        initialize(); 

        // ppm header
        std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        //  multithreading
        int pixel_per_thread = (image_height*image_width) / num_threads;
        int row_per_thread = pixel_per_thread / image_width;
        // leftover pixels after distribute by thread
        int remaining_pixels = image_height*image_width - pixel_per_thread*num_threads;
        std::vector<std::thread> threads(num_threads);
        std::vector<color> chunk_pixels(image_height*image_width);

        // atomic = thread-safe counter x{0}, and historically uses 'direct' initialization, backwards compatible with older C++ (counter x = 0 would be 'copy-initialization')
        std::atomic<int> pixels_processed{0};

        for (int t=0; t < num_threads; t++) {

            // define size of chunk we want to evaluate and set endpoint to include missing pixels if last thread (if pixel_per_thread is not divisible by num_threads)
            int start_pixel = t * pixel_per_thread;
            int end_pixel = (t == num_threads - 1) ? image_height*image_width : start_pixel + pixel_per_thread; 

            // create thread
            std::thread render_thread(&camera::render_pixel_chunk, this, std::ref(world), chunk_pixels.data(), start_pixel, end_pixel, std::ref(pixels_processed));
            threads[t] = std::move(render_thread);
        }

        // log completion
        while (pixels_processed < image_height*image_width) {
            int processed_pixels = pixels_processed.load(); // load reads atomic value
            std::clog << "\rProcessed " << processed_pixels << " pixels out of " << image_height*image_width << std::flush;
        }

        // join threads
        for (int t=0; t < num_threads; t++) {
            threads[t].join();
        }

        // collect outpus and turn into ppm
        for (int pixel = 0; pixel < image_height*image_width; pixel++) {
            write_color(std::cout, chunk_pixels[pixel]);
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::clog << "\nRendering time: " << duration.count() << " ms\n";
        
    }

  private:
    /* private camera parameters*/

    int image_height; // rendered image height in pixel count
    double pixel_samples_scale; // color scale factor for sum of pixel samples
    point3 center; // camera center
    point3 pixel00_loc; // location of pixel 0, 0
    vec3 pixel_delta_u; // offset to pixel to the right
    vec3 pixel_delta_v; // offset to pixel below
    vec3 u,v,w; // camera frame basis vectors

    vec3 defocus_disk_u; // defocus disk horizontal radius
    vec3 defocus_disk_v; // defocus disk vertical radius

    void initialize() {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        // each pixel sample is scaled by pixel_samples_scale
        pixel_samples_scale = 1.0 / samples_per_pixel;

        center = lookfrom;

        // auto focal_length = (lookfrom - lookat).length();
        
        // vertical fov angle is converted to radians
        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta/2);
        // auto viewport_height = 2.0 * h * focal_length;
        auto viewport_height = 2.0 * h * focus_dist; // for now we will set our focus point to be the same as focal length
        auto viewport_width = viewport_height * (double(image_width)/image_height);

        // u,v,w unit basis camera coord frame
        w = unit_vector(lookfrom - lookat); // camera "forward" direction (actually points in opposite direction of lookat)
        u = unit_vector(cross(vup, w)); // orthogonal to up and w (points right on camera plane)
        v = cross(w, u); // orthogonal to w and u (points up on camera plane)

        // horizontal/vertical viewport vectors, now camera-relative
        auto viewport_u = viewport_width * u; // left-right
        auto viewport_v = viewport_height * -v; // up-down

        // pixel delta vectors
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        // upper left pixel
        auto viewport_upper_left = center - (0.5 * viewport_u) - (0.5 * viewport_v) - focus_dist*w;
        pixel00_loc = viewport_upper_left + (0.5 * pixel_delta_u) + (0.5 * pixel_delta_v);

        // defocus disk basis vectors
        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    color ray_color(const ray& r, int depth, const hittable& world) const {
        if (depth <= 0) {
            return color(0,0,0); // if max depth reached, return black - no more light gathered
        }        
        
        hit_record rec; 

        // we ignore any hit that is too close to the ray origin (0.001)
        //  => this is to avoid "shadow acne", caused by floating point precision errors during intersection point solving
        //  => the floating point error can cause the interesction point to be slightly off, and if the ray occurs slightly below the surface, it can interact with the surface again
        //  => this means we will cast a shadow on ourself in small areas that should be lit
        if (world.hit(r, interval(0.001, infinity), rec)){
            // randomly reflect ray off surface (simple diffuse reflection)
            // vec3 direction = random_on_hemisphere(rec.normal);

            // lambertian reflection
            //  => the direction of the reflected ray is random about the normal on the tangent sphere outside the hit point rec.p
            //  => because of the less uniform scattering, we get better bounce light from sky, and more pronounced shadows
            //  => since for diffuse objects, less light bounces towards the camera (darker), and for shadows, more light bounces straight up (normal on surface) so underneath sphere is darker
            // vec3 direction = rec.normal + random_unit_vector();

            // recursively trace reflected ray
            // - if 100% color is reflected, white
            // - if 0% color is reflected, black
            // - here we set 50% reflected, grey
            // random direction about the hit point rec.p
            // modifying the multiplier from 0 -> 1 gives us the "gamut" of our renderer
            //  => but 0.5 is too dark compared to a 'half-way' point between black and white
            //  => so we need to use gamma correction since most image viewers assume gamma space (non-transformed = linear space, transformed 0-1 values = gamma space)
            // return 0.3 * ray_color(ray(rec.p, direction), depth -1, world);
            // return 0.5 * (rec.normal + color(1,1,1));

            ray scattered;
            color attenuation;
            if (rec.mat->scatter(r, rec, attenuation, scattered)) {
                return attenuation * ray_color(scattered, depth - 1, world);
            }
            return color(0,0,0); // no scatter than absorb = black
        }

        vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5*(unit_direction.y() + 1.0);
        return (1.0-a)*color(1.0, 1.0, 1.0) + a*color(0.5, 0.7, 1.0);
    }

    ray get_ray(int i, int j) const {
        // construct camera ray from defocus disk to randomly sampled point about pixel i,j

        auto offset = sample_square(); // relative to pixel center by [-0.5, 0.5]
        auto pixel_sample = pixel00_loc 
                                + ((i + offset.x()) * pixel_delta_u)
                                + ((j + offset.y()) * pixel_delta_v);
        // auto ray_origin = center;
        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        return ray(ray_origin, ray_direction);
    }

    vec3 sample_square() const {
        // sample square [-0.5, 0.5] x [-0.5, 0.5]
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    vec3 defocus_disk_sample() const {
        // returns a random point in the camera defocus disk
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }
};


#endif
