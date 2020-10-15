#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <execution>
#include <random>
#include <variant>
#include <mutex>

#include "profile.h"


class ComputeRequest {
 public:
     int left; int right;

     ComputeRequest(int l = 0, int r = 0) : left(l), right(r) {}
};

class UpdateRequest {
 public:
     int index; int delta;

     UpdateRequest(int ind = 0, int delt = 0) : index(ind), delta(delt) {}
};



class segment_tree {
 public:
    std::vector<int> Data;
    int real_size;

    segment_tree(const std::vector<int>& other) {
        int curr_s = 1;
        while ((1 << curr_s) < other.size()) {
            ++curr_s;
        }
        real_size = (1 << curr_s);
        Data.resize((1 << (curr_s + 1)) - 1);
        for (int i = 0; i < other.size(); ++i) {
            Data[i + real_size - 1] = other[i];
        }
        for (int i = real_size - 2; i >= 0; --i) {
            Data[i] = Data[get_left(i)] + Data[get_right(i)];
        }
    }

    int get_left(int pos) const {
        return pos * 2 + 1;
    }
    int get_right(int pos) const {
        return pos * 2 + 2;
    }

    int sum_in(int left, int right) const {
        return get_sum(left, right, 0, 0, real_size - 1);
    }

    std::vector<int>& get_data() {
        return Data;    
    }
    int get_sum(int left, int right, int curr_pos, int curr_left, int curr_right) const {
        if (left > right) {
            return 0;
        }
        if (left == curr_left && right == curr_right) {
            return Data[curr_pos];
        }
        int mid = curr_left + (curr_right - curr_left) / 2;
        return get_sum(left, std::min(right, mid), get_left(curr_pos), curr_left, mid) +
               get_sum(std::max(mid + 1, left), right, get_right(curr_pos), mid + 1, curr_right);
    }

    void init_change_par(int pos, int diff) {
        int curr_node = pos - 1 + real_size;
        std::vector<int> visited({curr_node});
        while (curr_node != 0) {
            curr_node = (curr_node - 1) / 2;
            visited.push_back(curr_node);
        }
        std::for_each(std::execution::par, visited.begin(), visited.end(), [&](int node) {
            Data[node] += diff;
        });
    }

    void init_change(int pos, int diff) {
        int curr_node = pos - 1 + real_size;
        Data[curr_node] += diff;
        while (curr_node != 0) {
            curr_node = (curr_node - 1) / 2;
            Data[curr_node] += diff;
        }
    }
};

using Request = std::variant<ComputeRequest, UpdateRequest>;

std::vector<int> ProcessRequests(const std::vector<int>& numbers, const std::vector<Request>& requests) {
    segment_tree solve(numbers);
    std::vector<int> ans;
    ans.reserve(requests.size());
    auto it = requests.begin();
    while (it != requests.end()) {
        auto curr_it = it;
        int start = ans.size();
        while (curr_it != requests.end() && std::holds_alternative<ComputeRequest>(*curr_it)) {
            ++curr_it;
        }
        ans.resize(ans.size() + curr_it - it);
        auto answer_insert_it = ans.begin() + start;
        std::transform(std::execution::par, it, curr_it, answer_insert_it, [&solve](const Request& req) {
            return solve.sum_in(std::get<0>(req).left, std::get<0>(req).right - 1);
        });
        it = curr_it;
        while (curr_it != requests.end() && std::holds_alternative<UpdateRequest>(*curr_it)) {
            ++curr_it;
        }
        std::for_each(std::execution::seq, it, curr_it, [&solve](const Request& req) {
            solve.init_change(std::get<1>(req).index, std::get<1>(req).delta);
        });
        it = curr_it;
    }
    return ans;
}

std::vector<int> ProcessRequests_seq_count(const std::vector<int>& numbers, const std::vector<Request>& requests) {
    segment_tree solve(numbers);
    std::vector<int> ans;
    for (int i = 0; i < requests.size(); ++i) {
        if (std::holds_alternative<ComputeRequest>(requests[i])) {
            ans.push_back(solve.sum_in(std::get<0>(requests[i]).left, std::get<0>(requests[i]).right));
        } else {
            solve.init_change(std::get<1>(requests[i]).index, std::get<1>(requests[i]).delta);
        }
    }
    return ans;
}



std::vector<int> gen_numbers(std::mt19937& generator, int count, int max_val) {
    std::vector<int> ans(count);
    std::uniform_int_distribution<int> gen(0, max_val);
    for (int i = 0; i < count; ++i) {
        ans[i] = gen(generator);
    }
    return ans;
}
std::vector<Request> gen_requests(std::mt19937& generator, int count, int max_right_pos, int max_delta) {
    std::vector<Request> ans(count);
    std::uniform_int_distribution<int> gen(0, max_right_pos);
    int i = 0;
    int curr = 0;
    while (i < count) {
        int next = std::uniform_int_distribution<int>(i + 1, count)(generator);
        for (; i < next; ++i) {
            if (curr == 0) {
                int l = gen(generator), r = gen(generator);
                if (l > r) {
                    std::swap(l, r);
                }
                if (l == r) {
                    --i;
                } else {
                    ans[i] = ComputeRequest(l, r);
                }
            } else {
                ans[i] = UpdateRequest(std::uniform_int_distribution<int>(0, max_right_pos - 1)(generator), std::uniform_int_distribution<int>(-max_delta, max_delta)(generator));
            }
        }
        curr = (curr + 1) % 2;
    }
    return ans;
}

int main() {
    std::mt19937 generator;
    int count_errors = 0;
    for (int j = 0; j < 1; ++j) {
        std::vector<int> v(gen_numbers(generator, 10'000'000, 2));
        std::vector<Request> Requests(gen_requests(generator, 1'000'000'0, 10'000'000 - 1, 2));
        std::vector<int> first_ans, second_ans;


        {
            LOG_DURATION("simple");
            first_ans = ProcessRequests_seq_count(v, Requests);
        }
        {
            LOG_DURATION("parallel");
            second_ans = ProcessRequests(v, Requests);
        }
    }
    if (count_errors == 0) {
        std::cout << "OK\n";
    } else {
        std::cout << count_errors << " is total errors\n";
    }
}