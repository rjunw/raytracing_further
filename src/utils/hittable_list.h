#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H
//==============================================================================================
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

#include <vector>


class hittable_list : public hittable {
  public:
    std::vector<shared_ptr<hittable>> objects;

    hittable_list() {}
    hittable_list(shared_ptr<hittable> object) { add(object); }

    void clear() { objects.clear(); }

    void add(shared_ptr<hittable> object) {
        objects.push_back(object);
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        hit_record temp_rec;
        bool hit_anything = false;
        auto closest_so_far = ray_t.max;

        for (const auto& object : objects) {
            if (object->hit(r, interval(ray_t.min, closest_so_far), temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }

    point3 get_upper_bounds() const override {
        point3 upper_bound = point3(-infinity, -infinity, -infinity);
        for (const auto& object : objects) {
            point3 obj_upper = object->get_upper_bounds();
            upper_bound = point3(
                std::fmax(upper_bound.x(), obj_upper.x()),
                std::fmax(upper_bound.y(), obj_upper.y()),
                std::fmax(upper_bound.z(), obj_upper.z())
            );
        }
        return upper_bound;
    }
    point3 get_lower_bounds() const override {
        point3 lower_bound = point3(+infinity, +infinity, +infinity);
        for (const auto& object : objects) {
            point3 obj_lower = object->get_lower_bounds();
            lower_bound = point3(
                std::fmin(lower_bound.x(), obj_lower.x()),
                std::fmin(lower_bound.y(), obj_lower.y()),
                std::fmin(lower_bound.z(), obj_lower.z())
            );
        }
        return lower_bound;
    }
};


#endif
