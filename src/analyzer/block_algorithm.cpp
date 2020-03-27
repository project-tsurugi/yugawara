#include <yugawara/analyzer/block_algorithm.h>

namespace yugawara::analyzer {

template<class G, class C>
void collect_heads0(G& g, C const& consumer) {
    for (auto&& b : g) {
        if (b.upstreams().empty()) {
            consumer(b);
        }
    }
}

void collect_heads(block_graph& g, block_consumer const& consumer) {
    collect_heads0(g, consumer);
}

void collect_heads(block_graph const& g, const_block_consumer const& consumer) {
    collect_heads0(g, consumer);
}

template<class G, class C>
void collect_tails0(G& g, C const& consumer) {
    for (auto&& b : g) {
        if (b.downstreams().empty()) {
            consumer(b);
        }
    }
}

void collect_tails(block_graph& g, block_consumer const& consumer) {
    collect_tails0(g, consumer);
}

void collect_tails(block_graph const& g, const_block_consumer const& consumer) {
    collect_tails0(g, consumer);
}

template<class B, class G>
::takatori::util::optional_ptr<B> find_unique_head0(G& g) noexcept {
    B* result {};
    for (auto&& b : g) {
        if (b.upstreams().empty()) {
            if (result == nullptr) {
                result = std::addressof(b);
            } else {
                return {}; // not unique
            }
        }
    }
    return ::takatori::util::optional_ptr<B> { result };
}

::takatori::util::optional_ptr<block> find_unique_head(block_graph& g) noexcept {
    return find_unique_head0<block>(g);
}

::takatori::util::optional_ptr<block const> find_unique_head(block_graph const& g) noexcept {
    return find_unique_head0<block const>(g);
}

template<class B>
::takatori::util::optional_ptr<B> find_unique_upstream0(B& b) noexcept {
    auto candidates = b.upstreams();
    if (candidates.size() == 1) {
        return candidates[0];
    }
    return {};
}

::takatori::util::optional_ptr<block> find_unique_upstream(block& b) noexcept {
    return find_unique_upstream0(b);
}

::takatori::util::optional_ptr<block const> find_unique_upstream(block const& b) noexcept {
    return find_unique_upstream0(b);
}

template<class B>
::takatori::util::optional_ptr<B> find_unique_downstream0(B& b) noexcept {
    auto candidates = b.downstreams();
    if (candidates.size() == 1) {
        return candidates[0];
    }
    return {};
}

::takatori::util::optional_ptr<block> find_unique_downstream(block& b) noexcept {
    return find_unique_downstream0(b);
}

::takatori::util::optional_ptr<block const> find_unique_downstream(block const& b) noexcept {
    return find_unique_downstream0(b);
}

} // namespace yugawara::analyzer
