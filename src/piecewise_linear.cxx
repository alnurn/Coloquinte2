
#include "coloquinte/piecewise_linear.hxx"

#include <cassert>

namespace coloquinte{

namespace{
struct pl_edge{
    p_v f, s;

    static void push_intersections(pl_edge a, pl_edge b, piecewise_linear_function & lf){
        // Strict, because it makes everything easier
        assert(a.f.first < b.s.first and a.s.first > b.f.first);
        assert(a.f.first < a.s.first and b.f.first < b.s.first);
        
        int_t denom = (a.s.first-a.f.first) * (b.s.second-b.f.second)
                    - (a.s.second-a.f.second) * (b.s.first-b.f.first);

        int_t a_num   = (a.f.second-b.f.second) * (b.s.first-b.f.first)
                    - (a.f.first-b.f.first) * (b.s.second-b.f.second);
        int_t b_num = (a.f.second-b.f.second) * (a.s.first-a.f.first)
                    - (a.f.first-b.f.first) * (a.s.second-a.f.second);

        // There is an intersection if o_num / denom and num / denom are in [0,1]

        // Strict since intersections on a slope change point are already handled
        bool intersect = denom > 0 ? 
            (a_num < denom and b_num < denom and a_num > 0 and b_num > 0)
          : (a_num > denom and b_num > denom and a_num < 0 and b_num < 0);

        if(intersect){
            // Find where they intersect
            int_t dist = a_num*(a.s.first-a.f.first);
            if( (dist % denom == 0) ){ // Exact integer intersection
                int_t pos = dist/denom;
                if(pos > std::max(a.f.first, b.f.first) ) // Necessarily smaller than s.first due to the previous condition
                    lf.point_values.push_back(p_v(pos, a.value_at(pos)));
            }
            else{ // Non exact intersection: create two integers since I don't want to mess with floating point
                int_t pos1 = a.f.first + dist / denom;
                int_t pos2 = pos1 + 1;
                // Value_at is only an approximation, but it shouldn't be too bad
                if(pos1 > std::max(a.f.first, b.f.first) )
                    lf.point_values.push_back(p_v(pos1, std::min(a.value_at(pos1), b.value_at(pos1))));
                if(pos2 < std::min(a.s.first, b.s.first) )
                    lf.point_values.push_back(p_v(pos2, std::min(a.value_at(pos2), b.value_at(pos2))));
            }
        }
    }

    // Lower-rounded value
    int_t value_at(int_t pos) const{
        assert(pos >= f.first and pos <= s.first);
        return (f.second * (s.first - pos) + s.second * (pos - f.first)) / (s.first - f.first); 
    }
    // Lower-rounded value
    int_t pos_at(int_t val) const{
        assert(val <= std::max(f.second, s.second) and val >= std::min(f.second, s.second));
        assert(f.second != s.second);
        return (f.first * (s.second - val) + s.first * (val - f.second)) / (s.second - f.second); 
    }

    bool above(p_v const o) const{
        int_t pos = o.first;
        assert(pos > f.first and pos < s.first);
        return (f.second * (s.first - pos) + s.second * (pos - f.first)) > o.second * (s.first - f.first); 
    }

    pl_edge(p_v a, p_v b) : f(a), s(b) {}
};
} // End anonymous namespace

void piecewise_linear_function::add_monotone(int_t slope, int_t offset){
    for(auto & V : point_values){
        // Offset taken into account here, multiplied with the slope
        V.second += slope * (V.first - point_values.front().first + offset);
    }
}

void piecewise_linear_function::add_bislope(int_t s_l, int_t s_r, int_t pos){
    //assert(pos <= point_values.back().first);
    //assert(pos >= point_values.front().first);

    auto it = point_values.begin();
    while(it->first < pos){
        it->second += s_l * (it->first - pos);
        ++it;
        assert(it != point_values.end());
    }
    if(it->first != pos){
        point_values.insert(it, p_v(pos, pl_edge(*std::prev(it), *it).value_at(pos)));
    }
    for(auto & V : point_values){
        if(V.first > pos)
            V.second += s_r * (V.first - pos);
    }
}

piecewise_linear_function::piecewise_linear_function(int_t min_def, int_t max_def){
    point_values.push_back(p_v(min_def, 0));
    point_values.push_back(p_v(max_def, 0));
}

piecewise_linear_function piecewise_linear_function::previous_min() const{
    piecewise_linear_function ret;
    auto it = point_values.begin();
    ret.point_values.push_back(*it);
    ++it;
    // Use the previous minimum to detect when we find something smaller
    for(; it != point_values.end(); ++it){
        int_t cur_min = ret.point_values.back().second;
        assert(it->first > ret.point_values.back().first);
        if(it->second < cur_min){
            if(std::prev(it)->first != ret.point_values.back().first){ // May be equal, in which case we don't need to push anything new
                int_t pos = pl_edge(*std::prev(it), *it).pos_at(cur_min);
                if(pos != ret.point_values.back().first and pos != it->first){
                    ret.point_values.push_back(p_v(pos, cur_min));
                }
            }
            ret.point_values.push_back(*it);
        }
    }
    return ret;
}

piecewise_linear_function piecewise_linear_function::previous_min_of_sum(piecewise_linear_function const & a, int_t shift) const{
    piecewise_linear_function ret;

    // Go to the correct definition
    auto b_begin = point_values.begin(), a_begin = a.point_values.begin();
    auto b_it = b_begin, a_it = a_begin;
    auto b_end = point_values.end(), a_end = a.point_values.end();

    while(a_it != a_end){
        if(b_it == b_end or a_it->first < b_it->first-shift){ // Ok, create an edge and calculate the value
            if(b_it != b_begin){
                int_t value;
                if(b_it != b_end){
                    pl_edge b_edge(*std::prev(b_it), *b_it);
                    value = b_edge.value_at(a_it->first+shift);
                }
                else{
                    value = point_values.back().second;
                }
                ret.point_values.push_back(p_v(a_it->first, a_it->second + value));
            }
            ++a_it;
        }
        else if(a_it->first > b_it->first-shift){
            if(a_it != a_begin){
                pl_edge a_edge(*std::prev(a_it), *a_it);
                int_t value = a_edge.value_at(b_it->first-shift);
                ret.point_values.push_back(p_v(b_it->first-shift, b_it->second + value));
            }
            ++b_it;
        }
        else{ // if(a_it->first == b_it->first-shift){
            ret.point_values.push_back(p_v(a_it->first, a_it->second + b_it->second));
            ++a_it;
            ++b_it;
        }
    }

    return ret.previous_min();
}


int_t piecewise_linear_function::last_before(int_t pos) const{
    auto it = point_values.rbegin();
    while(it != point_values.rend()){
        if(it->first <= pos){
            return it->first;
        }
    }
    assert(false); // We should have found it if the bound was correct
}

int_t piecewise_linear_function::value_at(int_t pos) const{
    // First position bigger or equal than pos
    auto it = std::lower_bound(point_values.begin(), point_values.end(), pos, [](p_v o, int_t v){ return o.first < v; });
    if(pos != it->first){
        assert(it != point_values.begin());
        return pl_edge(*std::prev(it), *it).value_at(pos);
    }
    else{
        return it->second;
    }
}

piecewise_linear_function piecewise_linear_function::piecewise_linear_function::minimum(piecewise_linear_function const & a, piecewise_linear_function const & b){
    assert(a.point_values.front().first == b.point_values.front().first);
    assert(a.point_values.back().first == b.point_values.back().first);

    piecewise_linear_function ret;
    auto a_it = a.point_values.begin(), b_it = b.point_values.begin();
    auto a_end = a.point_values.end(), b_end = b.point_values.end();

    ret.point_values.push_back(p_v(a_it->first, std::min(a_it->second, b_it->second)));

    assert(std::next(a_it) != a_end and std::next(b_it) != b_end);
    while(std::next(a_it) != a_end and std::next(b_it) != b_end){
        pl_edge a_edge(*a_it, *std::next(a_it)), b_edge(*b_it, *std::next(b_it));
        // Three cases: one of them always below, or both intersect
        // Both intersect: we push the values when intersecting
        pl_edge::push_intersections(a_edge, b_edge, ret);

        // In any case, we push the value of the one below if it finishes, and increment the iterator
        if(a_edge.s.first < b_edge.s.first){
            ++a_it;
            if(b_edge.above(a_edge.s)){ // We push a_edge.s
                ret.point_values.push_back(a_edge.s);
            }
        }
        else if(a_edge.s.first > b_edge.s.first){
            ++b_it;
            if(a_edge.above(b_edge.s)){ // We push a_edge.s
                ret.point_values.push_back(b_edge.s);
            }
        }
        else{
            ret.point_values.push_back(p_v(a_edge.s.first, std::min(a_edge.s.second, b_edge.s.second)));
            ++a_it;
            ++b_it;
        }
    }
    return ret;
}

} // End namespace coloquinte
