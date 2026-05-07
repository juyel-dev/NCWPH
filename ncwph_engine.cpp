#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>

using namespace emscripten;

const float PI = 3.141592653589793f;

std::vector<float> gaussian_kernel(int size, float sigma) {
    std::vector<float> kernel(size * size);
    int half = size / 2;
    float sum = 0.0f;
    for (int y = -half; y <= half; ++y) {
        for (int x = -half; x <= half; ++x) {
            float val = std::exp(-(x*x + y*y) / (2*sigma*sigma));
            kernel[(y+half)*size + (x+half)] = val;
            sum += val;
        }
    }
    for (float &v : kernel) v /= sum;
    return kernel;
}

std::vector<float> convolve2d(const std::vector<float>& img, int w, int h,
                              const std::vector<float>& kernel, int kw) {
    int kh = kw;
    int out_w = w - kw + 1;
    int out_h = h - kh + 1;
    std::vector<float> out(out_w * out_h);
    for (int y = 0; y < out_h; ++y) {
        for (int x = 0; x < out_w; ++x) {
            float sum = 0.0f;
            for (int ky = 0; ky < kh; ++ky) {
                for (int kx = 0; kx < kw; ++kx) {
                    sum += img[(y+ky)*w + (x+kx)] * kernel[ky*kw + kx];
                }
            }
            out[y*out_w + x] = sum;
        }
    }
    return out;
}

std::vector<float> resize(const std::vector<float>& src, int w, int h, int nw, int nh) {
    std::vector<float> dst(nw * nh);
    float scaleX = (float)w / nw;
    float scaleY = (float)h / nh;
    for (int y = 0; y < nh; ++y) {
        float sy = (y + 0.5f) * scaleY - 0.5f;
        int iy0 = (int)std::floor(sy);
        int iy1 = std::min(iy0 + 1, h - 1);
        float fy = sy - iy0;
        iy0 = std::max(0, iy0);
        for (int x = 0; x < nw; ++x) {
            float sx = (x + 0.5f) * scaleX - 0.5f;
            int ix0 = (int)std::floor(sx);
            int ix1 = std::min(ix0 + 1, w - 1);
            float fx = sx - ix0;
            ix0 = std::max(0, ix0);
            float v00 = src[iy0*w + ix0];
            float v10 = src[iy0*w + ix1];
            float v01 = src[iy1*w + ix0];
            float v11 = src[iy1*w + ix1];
            float v0 = v00 + fx * (v10 - v00);
            float v1 = v01 + fx * (v11 - v01);
            dst[y*nw + x] = v0 + fy * (v1 - v0);
        }
    }
    return dst;
}

std::vector<float> rgb_to_gray(const uint8_t* rgba, int w, int h) {
    std::vector<float> gray(w * h);
    for (int i = 0; i < w * h; ++i) {
        uint8_t r = rgba[i*4 + 0];
        uint8_t g = rgba[i*4 + 1];
        uint8_t b = rgba[i*4 + 2];
        gray[i] = 0.2989f * r + 0.5870f * g + 0.1140f * b;
    }
    return gray;
}

std::vector<float> apply_gaussian(const std::vector<float>& img, int w, int h, float sigma) {
    int ksize = (int)std::ceil(3.0f * sigma) * 2 + 1;
    auto kernel = gaussian_kernel(ksize, sigma);
    return convolve2d(img, w, h, kernel, ksize);
}

std::vector<float> log_polar(const std::vector<float>& img, int w, int h,
                              int out_radius, int out_angles) {
    std::vector<float> lp(out_radius * out_angles, 0.0f);
    float center_x = w / 2.0f;
    float center_y = h / 2.0f;
    float max_radius = std::min(center_x, center_y);
    for (int r_idx = 0; r_idx < out_radius; ++r_idx) {
        float radius = max_radius * std::pow(1.0f / max_radius, (float)r_idx / (out_radius - 1));
        for (int a_idx = 0; a_idx < out_angles; ++a_idx) {
            float angle = 2.0f * PI * a_idx / out_angles;
            float x = center_x + radius * std::cos(angle);
            float y = center_y + radius * std::sin(angle);
            int x0 = (int)std::floor(x);
            int x1 = x0 + 1;
            int y0 = (int)std::floor(y);
            int y1 = y0 + 1;
            float fx = x - x0;
            float fy = y - y0;
            x0 = std::clamp(x0, 0, w-1);
            x1 = std::clamp(x1, 0, w-1);
            y0 = std::clamp(y0, 0, h-1);
            y1 = std::clamp(y1, 0, h-1);
            float v00 = img[y0*w + x0];
            float v10 = img[y0*w + x1];
            float v01 = img[y1*w + x0];
            float v11 = img[y1*w + x1];
            float v0 = v00 + fx * (v10 - v00);
            float v1 = v01 + fx * (v11 - v01);
            lp[r_idx*out_angles + a_idx] = v0 + fy * (v1 - v0);
        }
    }
    return lp;
}

struct GaborFilter {
    std::vector<float> even_kernel;
    std::vector<float> odd_kernel;
    int size;
};

std::vector<GaborFilter> make_gabor_filters(int scales, int orientations) {
    std::vector<GaborFilter> filters;
    float base_freq = 0.05f;
    float freq_factor = 1.5f;
    for (int s = 0; s < scales; ++s) {
        float freq = base_freq * std::pow(freq_factor, s);
        float wavelength = 1.0f / freq;
        float sigma = wavelength * 0.56f;
        int ksize = (int)std::ceil(3.0f * sigma) * 2 + 1;
        int half = ksize / 2;
        for (int o = 0; o < orientations; ++o) {
            float theta = PI * o / orientations;
            GaborFilter gf;
            gf.size = ksize;
            gf.even_kernel.resize(ksize*ksize);
            gf.odd_kernel.resize(ksize*ksize);
            for (int y = -half; y <= half; ++y) {
                for (int x = -half; x <= half; ++x) {
                    float x_theta = x * std::cos(theta) + y * std::sin(theta);
                    float y_theta = -x * std::sin(theta) + y * std::cos(theta);
                    float gauss = std::exp(-0.5f * (x_theta*x_theta + y_theta*y_theta) / (sigma*sigma));
                    float c = std::cos(2 * PI * freq * x_theta);
                    float si = std::sin(2 * PI * freq * x_theta);
                    int idx = (y+half)*ksize + (x+half);
                    gf.even_kernel[idx] = gauss * c;
                    gf.odd_kernel[idx] = gauss * si;
                }
            }
            filters.push_back(gf);
        }
    }
    return filters;
}

std::vector<float> phase_congruency(const std::vector<float>& img, int w, int h) {
    int scales = 4, orientations = 6;
    auto filters = make_gabor_filters(scales, orientations);
    int pc_w = w - filters[0].size + 1;
    int pc_h = h - filters[0].size + 1;
    std::vector<float> sumA(pc_w * pc_h, 0.0f);
    std::vector<float> sumE(pc_w * pc_h, 0.0f);
    for (size_t i = 0; i < filters.size(); ++i) {
        auto& gf = filters[i];
        auto even = convolve2d(img, w, h, gf.even_kernel, gf.size);
        auto odd = convolve2d(img, w, h, gf.odd_kernel, gf.size);
        for (int j = 0; j < pc_w*pc_h; ++j) {
            float amp = std::sqrt(even[j]*even[j] + odd[j]*odd[j]);
            sumA[j] += amp;
            sumE[j] += amp;
        }
    }
    std::vector<float> pc(pc_w * pc_h);
    float eps = 0.01f;
    for (int j = 0; j < pc_w*pc_h; ++j) {
        pc[j] = sumE[j] / (sumA[j] + eps);
    }
    return pc;
}

std::vector<float> spatial_pyramid_pooling(const std::vector<float>& feat_map,
                                           int w, int h) {
    std::vector<float> features;
    int levels[] = {1, 2, 4};
    for (int L : levels) {
        int cell_h = h / L;
        int cell_w = w / L;
        for (int y = 0; y < L; ++y) {
            for (int x = 0; x < L; ++x) {
                float sum = 0.0f, sq_sum = 0.0f;
                int count = 0;
                for (int cy = y*cell_h; cy < (y+1)*cell_h; ++cy) {
                    for (int cx = x*cell_w; cx < (x+1)*cell_w; ++cx) {
                        float val = feat_map[cy*w + cx];
                        sum += val;
                        sq_sum += val * val;
                        count++;
                    }
                }
                float mean = sum / count;
                float var = (sq_sum / count) - (mean * mean);
                features.push_back(mean);
                features.push_back(std::sqrt(std::max(var, 0.0f)));
            }
        }
    }
    return features;
}

std::vector<float> projection_matrix(int feature_dim, int hash_bits) {
    static std::vector<float> mat;
    if (!mat.empty()) return mat;
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    mat.resize(feature_dim * hash_bits);
    for (size_t i = 0; i < mat.size(); ++i)
        mat[i] = dist(rng);
    return mat;
}

std::string vector_to_hex(const std::vector<uint8_t>& bits) {
    const char hex_chars[] = "0123456789abcdef";
    std::string hex;
    for (size_t i = 0; i < bits.size(); i += 4) {
        uint8_t nibble = 0;
        for (int j = 0; j < 4 && (i+j) < bits.size(); ++j)
            nibble |= (bits[i+j] << (3-j));
        hex.push_back(hex_chars[nibble]);
    }
    return hex;
}

std::string ncwph_compute_hash(const val& image_typed_array, int w, int h) {
    std::vector<uint8_t> img_data(w * h * 4);
    val memory_view{ typed_memory_view(img_data.size(), img_data.data()) };
    memory_view.call<void>("set", image_typed_array);

    std::vector<float> gray = rgb_to_gray(img_data.data(), w, h);
    gray = apply_gaussian(gray, w, h, 1.0f);

    const int target_size = 256;
    gray = resize(gray, w, h, target_size, target_size);
    w = h = target_size;

    std::vector<float> lp = log_polar(gray, w, h, 64, 128);
    std::vector<float> pc = phase_congruency(lp, 64, 128);
    
    int pc_w = 64 - (int)std::ceil(3.0f * (1.0f/0.05f) * 0.56f) * 2;
    if (pc_w <= 0) pc_w = 32;
    int pc_h = 128 - (int)std::ceil(3.0f * (1.0f/0.05f) * 0.56f) * 2;
    if (pc_h <= 0) pc_h = 64;
    
    std::vector<float> features = spatial_pyramid_pooling(pc, pc_w, pc_h);

    float sum = 0.0f, sq_sum = 0.0f;
    for (float v : features) { sum += v; sq_sum += v*v; }
    float mean = sum / features.size();
    float stddev = std::sqrt(sq_sum/features.size() - mean*mean + 1e-8f);
    for (float &v : features) v = (v - mean) / stddev;

    int feat_dim = 800;
    std::vector<float> final_features(feat_dim, 0.0f);
    int copy_len = std::min(feat_dim, (int)features.size());
    for (int i = 0; i < copy_len; ++i) final_features[i] = features[i];

    int hash_bits = 256;
    auto proj_mat = projection_matrix(feat_dim, hash_bits);
    std::vector<float> proj(hash_bits, 0.0f);
    for (int b = 0; b < hash_bits; ++b) {
        for (int f = 0; f < feat_dim; ++f) {
            proj[b] += final_features[f] * proj_mat[f * hash_bits + b];
        }
    }
    std::vector<float> sorted_proj = proj;
    std::sort(sorted_proj.begin(), sorted_proj.end());
    float median = sorted_proj[hash_bits / 2];
    std::vector<uint8_t> bits(hash_bits);
    for (int i = 0; i < hash_bits; ++i) bits[i] = (proj[i] > median) ? 1 : 0;

    return vector_to_hex(bits);
}

struct CompareResult {
    double similarity;
    bool match;
};

CompareResult ncwph_compare(const std::string& hash1, const std::string& hash2) {
    int length = std::min(hash1.size(), hash2.size()) * 4;
    int diff_bits = 0;
    for (size_t i = 0; i < hash1.size() && i < hash2.size(); ++i) {
        auto char_to_nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return 0;
        };
        int byte1 = char_to_nibble(hash1[i]);
        int byte2 = char_to_nibble(hash2[i]);
        uint8_t diff = byte1 ^ byte2;
        while (diff) { diff_bits += diff & 1; diff >>= 1; }
    }
    double similarity = 1.0 - (double)diff_bits / length;
    CompareResult res;
    res.similarity = similarity;
    res.match = similarity > 0.7;
    return res;
}

EMSCRIPTEN_BINDINGS(ncwph_module) {
    function("computeHash", &ncwph_compute_hash);
    function("compare", &ncwph_compare);
}
