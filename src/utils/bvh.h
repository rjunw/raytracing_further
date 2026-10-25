#ifndef BVH_H
#define BVH_H

# include "rtweekend.h"
# include "hittable.h"
# include "hittable_list.h"
# include <algorithm>
# include <vector>

// Authored October 2026 by Ryan Wang <ryanjun.wang@outlook.com>
//
// based on https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/

// our data structure
class bvh_node {
    public: 
        /* conceptually, we want to work with the following, but will use a more memory-efficient representation */
        // // bounding box for current node
        // aabb box; 

        // // left and right children box references
        // bvh_node* left;
        // bvh_node* right;

        // // reference to hittables
        // std::vector<hittable*> hittables;
        
        // for leaves, num_prims > 0
        // for internal nodes, left_index and right_index != 0, don't store primitives
        vec3 aabb_min, aabb_max;
        int left_index;//, right_index; right_index = left_index+1 since we split in half in continguous memory
        int first_prim_index, num_prims; // as long as prims are consecutive in memory
    
        bool is_leaf_node() const { return num_prims > 0; }
};

// store nodes and perform bvh algorithms
//  - inherit hittable since camera render can call hit()
//  - traversal should exit once a ray misses AABB of a node
class bvh : public hittable {
    public:
        // constructor
        bvh(const hittable_list& world) {build(world);}


        bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
            // check if ray intersects 
            if (nodes.empty()) return false; // no nodes to intersect
            return intersect_bvh(r, ray_t, rec, root_index);
        };

        point3 get_upper_bounds() const override {
            if (nodes.empty()) return point3(+infinity, +infinity, +infinity);
            return nodes[root_index].aabb_max;
        }
        point3 get_lower_bounds() const override {
            if (nodes.empty()) return point3(-infinity, -infinity, -infinity);
            return nodes[root_index].aabb_min;
        }

    private:
        std::vector<bvh_node> nodes; // max binary tree size where num_prims will be the # leaf nodes
        int root_index=0, nodes_used=1;
        std::vector<hittable*> primitives; // to store primitives in a contigious memory
        
        // build bvh via recursive subdivision
        void build(const hittable_list& world){
            // get hittables from our world
            for (auto& obj : world.objects){
                primitives.push_back(obj.get());
            }
            int num_prims = primitives.size();
            if (num_prims == 0) return;

            // initialize bvh nodes
            int bvh_size = num_prims*2-1;
            nodes.resize(bvh_size);

            // initialize root
            bvh_node& root = nodes[root_index];
            root.left_index = 0; // zero children initially
            root.first_prim_index = 0, root.num_prims = num_prims; // add primitives
            update_node_bounds(root_index);
            
            // recursive subdivision
            subdivide(root_index);
        }

        // define the bounding box of a node given the primitives it contains
        void update_node_bounds(int node_index){
            bvh_node& node = nodes[node_index];
            // initialize bounds to be infinity
            node.aabb_max = vec3(-infinity, -infinity, -infinity);
            node.aabb_min = vec3( infinity,  infinity,  infinity);

            // include a node's primatives
            for (int prim_index = node.first_prim_index; prim_index < node.first_prim_index+node.num_prims; prim_index++){
                // no modification of object, just use to calculate bounds
                const hittable& leaf_hittable = *primitives[prim_index];

                // upper corner of 3d bounding box
                node.aabb_max = vec3(
                    std::fmax(node.aabb_max.x(), leaf_hittable.get_upper_bounds().x()),
                    std::fmax(node.aabb_max.y(), leaf_hittable.get_upper_bounds().y()),
                    std::fmax(node.aabb_max.z(), leaf_hittable.get_upper_bounds().z())
                ); 

                // lower corner of 3d bounding box
                node.aabb_min = vec3(
                    std::fmin(node.aabb_min.x(), leaf_hittable.get_lower_bounds().x()),
                    std::fmin(node.aabb_min.y(), leaf_hittable.get_lower_bounds().y()),
                    std::fmin(node.aabb_min.z(), leaf_hittable.get_lower_bounds().z())
                ); 
            }

        }

        // subdivide node into 2 children
        //  1. find split plane axis/position
        //  2. split primatives into two halves via split plane
        //  3. create childs for halves
        //  4. recurse into children
        void subdivide(int node_index){
            bvh_node& node = nodes[node_index];
            if (node.num_prims <= 1) return; // end recursion

            // 1. find split plane axis/position (longest axis)
            vec3 extent = node.aabb_max - node.aabb_min;
            int axis = 0;
            if (extent.y() > extent.x()) axis = 1;
            if (extent.z() > extent[axis]) axis = 2;
            // midpoint
            double split_pos = 0.5*(node.aabb_min[axis] + node.aabb_max[axis]);

            // 2. organize primitives in halves based on object centers
            int i = node.first_prim_index; // start index
            int j = node.first_prim_index + node.num_prims - 1; // end index
            while (i <= j) {
                // move primitives with center < split_pos to front
                if (primitives[i]->get_center()[axis] < split_pos) {
                    i++;
                } else {
                    hittable* temp = primitives[i];
                    primitives[i] = primitives[j];
                    primitives[j] = temp;
                    j--;
                }
            }

            // 3. children
            int left_count = i - node.first_prim_index;
            if (left_count == 0 || left_count == node.num_prims) {
                return; // no valid split and we are leaf
            }
            int left_index = nodes_used++;  
            int right_index = nodes_used++; // left_index + 1 (post-increment)
            node.left_index = left_index;

            // left will still start from same spot -> i
            nodes[left_index].first_prim_index = node.first_prim_index;
            nodes[left_index].num_prims = i - node.first_prim_index;

            // right will start from i (at boundary) -> end
            nodes[right_index].first_prim_index = i;
            nodes[right_index].num_prims = node.num_prims - left_count;
            node.num_prims = 0; // mark as internal node

            // update nodes
            update_node_bounds(left_index);
            update_node_bounds(right_index);

            // 4. recurse
            subdivide(left_index);
            subdivide(right_index);

        }

        // basically, we only ray trace specific primitives in leaves
        bool intersect_bvh(const ray& r, interval ray_t, hit_record& rec, int node_index) const {
            const bvh_node& node = nodes[node_index]; 
            if (!intersect_aabb(r, ray_t, node.aabb_min, node.aabb_max)){
                return false;
            }

            // want closest intersection => update ray_t.max (like hittable_list)
            if (node.is_leaf_node()){
                bool hit_anything = false;
                // intersect ray with primatives
                for (int i=0; i < node.num_prims; i++){
                    hittable& prim = *primitives[node.first_prim_index + i];
                    if (prim.hit(r, ray_t, rec)){
                        hit_anything = true;
                        ray_t.max = rec.t; // update max to closest intersection
                    }
                }
                return hit_anything;
            }
            else {
                // test children
                bool hit_left = intersect_bvh(r, ray_t, rec, node.left_index);
                bool hit_right = intersect_bvh(r, ray_t, rec, node.left_index+1);
                return hit_left || hit_right;
            }
        }
        // slab method https://en.wikipedia.org/wiki/Slab_method
        //  - idea is to check if ray (p(t) = o + td) intersects with each axis-aligned slab (plane) of the aabb
        //  - t = (p - o)/d where p is point on aabb, o is ray origin, d is ray direction
        //  - ray ENTERS box only after it enters EACH SLAB
        bool intersect_aabb(const ray& r, interval ray_t, point3 b_min, point3 b_max) const {

            // solve for the 2 intersections with each slab of the aabb (because a box has 6 slabs - b_min.x, b_max.x, b_min.y, b_max.y, b_min.z, b_max.z)
            //      - box is region where all 3 overlap
            vec3 d = r.direction();
            d = d + vec3(1e-7, 1e-7, 1e-7); // stability
            vec3 d_inv = vec3(1.0/d.x(), 1.0/d.y(), 1.0/d.z());
            
            // each coord would represent t that we intersect with each axis-aligned slab (low is entry, high is exit)
            vec3 t_low = (b_min - r.origin()) * d_inv; 
            vec3 t_high = (b_max - r.origin()) * d_inv; 

            // get the segment extrema (closest and farthest intersection), clipped at ray_t bounds
            vec3 t_close = vec3(
                std::fmax(ray_t.min, std::fmin(t_low.x(), t_high.x())),
                std::fmax(ray_t.min, std::fmin(t_low.y(), t_high.y())),
                std::fmax(ray_t.min, std::fmin(t_low.z(), t_high.z()))
            );
            vec3 t_far = vec3(
                std::fmin(ray_t.max, std::fmax(t_low.x(), t_high.x())),
                std::fmin(ray_t.max, std::fmax(t_low.y(), t_high.y())),
                std::fmin(ray_t.max, std::fmax(t_low.z(), t_high.z()))
            );

            // intersection exists only if t_close is before t_far (<=)
            //  - t_close sign determines which side of the slab the ray is intersecting
            return std::max({t_close.x(), t_close.y(), t_close.z()}) <= std::min({t_far.x(), t_far.y(), t_far.z()});

        }
};


#endif