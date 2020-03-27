#include <yugawara/analyzer/block.h>

#include <algorithm>

#include <cassert>

#include <takatori/util/string_builder.h>

namespace yugawara::analyzer {

using ::takatori::util::string_builder;

block::block(block::value_type& front, block::value_type& back, ::takatori::util::object_creator creator) noexcept
    : front_(std::addressof(front))
    , back_(std::addressof(back))
    , upstreams_(decltype(upstreams_)::allocator_type(creator.allocator<decltype(upstreams_)::value_type>()))
    , downstreams_(decltype(downstreams_)::allocator_type(creator.allocator<decltype(downstreams_)::value_type>()))
{}

block::~block() {
    cleanup_connections();
}

block::block(block const& other, ::takatori::util::object_creator creator)
    : block(*other.front_, *other.back_, creator)
{}

block::block(block&& other, ::takatori::util::object_creator creator)
    : block(*other.front_, *other.back_, creator)
{}

block* block::clone(::takatori::util::object_creator creator) const& {
    return creator.create_object<block>(*this, creator);
}

block* block::clone(::takatori::util::object_creator creator) && {
    return creator.create_object<block>(std::move(*this), creator);
}

block::graph_type& block::owner() {
    return *owner_;
}

block::graph_type const& block::owner() const {
    return *owner_;
}

block::reference block::front() noexcept {
    return *front_;
}

block::const_reference block::front() const noexcept {
    return *front_;
}

block::reference block::back() noexcept {
    return *back_;
}

block::const_reference block::back() const noexcept {
    return *back_;
}

block::iterator block::begin() noexcept {
    return iterator { front_ };
}

block::const_iterator block::begin() const noexcept {
    return const_iterator { front_ };
}

block::iterator block::end() noexcept {
    return iterator { details::downstream_unambiguous(*back_).get() };
}

block::const_iterator block::end() const noexcept {
    return const_iterator { details::downstream_unambiguous(*back_).get() };
}

block& block::upstream(::takatori::relation::expression::input_port_type const& port) {
    return find(port);
}

block const& block::upstream(::takatori::relation::expression::input_port_type const& port) const {
    return find(port);
}

block& block::downstream(::takatori::relation::expression::output_port_type const& port) {
    return find(port);
}

block const& block::downstream(::takatori::relation::expression::output_port_type const& port) const {
    return find(port);
}

block::list_view<block> block::upstreams() {
    validate_inputs();
    return list_view<block> { upstreams_.data(), front_->input_ports().size() };
}

block::list_view<block const> block::upstreams() const {
    validate_inputs();
    return list_view<block const> { upstreams_.data(), front_->input_ports().size() };
}

block::list_view<block> block::downstreams() {
    validate_outputs();
    return list_view<block> { downstreams_.data(), back_->output_ports().size() };
}

block::list_view<block const> block::downstreams() const {
    validate_outputs();
    return list_view<block const> { downstreams_.data(), back_->output_ports().size() };
}

void block::on_join(block::graph_type* graph) noexcept {
    if (owner_ != nullptr && owner_ != graph) {
        cleanup_connections();
    }
    owner_ = graph;
}

void block::on_leave() noexcept {
    on_join(nullptr);
}

block& block::find(::takatori::relation::expression::input_port_type const& port) const {
    if (std::addressof(port.owner()) != front_) {
        throw std::invalid_argument(string_builder {}
                << "invalid port (must be a port of the front operator): "
                << port
                << string_builder::to_string);
    }
    if (!port.opposite()) {
        throw std::invalid_argument(string_builder {}
                << "no opposites: "
                << port
                << string_builder::to_string);
    }
    if (port.index() < upstreams_.size()) {
        if (auto* opposite_block = upstreams_[port.index()];
                opposite_block != nullptr
                && std::addressof(port.opposite()->owner()) == opposite_block->back_) {
            return *opposite_block;
        }
    }
    rebuild_connections();
    if (auto* opposite_block = upstreams_[port.index()]; opposite_block != nullptr) {
        return *opposite_block;
    }
    throw std::invalid_argument("unregistered port");
}

block& block::find(::takatori::relation::expression::output_port_type const& port) const {
    if (std::addressof(port.owner()) != back_) {
        throw std::invalid_argument(string_builder {}
                << "invalid port (must be a port of the back operator): "
                << port
                << string_builder::to_string);
    }
    if (!port.opposite()) {
        throw std::invalid_argument(string_builder {}
                << "no opposites: "
                << port
                << string_builder::to_string);
    }
    if (port.index() < downstreams_.size()) {
        if (auto* opposite_block = downstreams_[port.index()];
                opposite_block != nullptr
                && std::addressof(port.opposite()->owner()) == opposite_block->front_) {
            return *opposite_block;
        }
    }
    rebuild_connections();
    if (auto* opposite_block = downstreams_[port.index()]; opposite_block != nullptr) {
        return *opposite_block;
    }
    throw std::invalid_argument("unregistered port");
}

void block::validate_inputs() const {
    for (auto&& port : front_->input_ports()) {
        find(port);
    }
}

void block::validate_outputs() const {
    for (auto&& port : back_->output_ports()) {
        find(port);
    }
}

void block::rebuild_connections() const {
    if (owner_ == nullptr) {
        throw std::domain_error("orphaned block");
    }
    assert(upstreams_.size() <= front_->input_ports().size()); // NOLINT
    assert(downstreams_.size() <= back_->output_ports().size()); // NOLINT
    upstreams_.resize(front_->input_ports().size());
    downstreams_.resize(back_->output_ports().size());

    for (auto&& candidate : *owner_) {
        if (std::addressof(candidate) == this) {
            continue;
        }
        for (auto&& port : candidate.back_->output_ports()) {
            if (auto opposite = port.opposite();
                    opposite
                    && std::addressof(opposite->owner()) == front_) {
                connect(candidate, port, *this, *opposite);
            }
        }
        for (auto&& port : candidate.front_->input_ports()) {
            if (auto opposite = port.opposite();
                    opposite
                    && std::addressof(opposite->owner()) == back_) {
                connect(*this, *opposite, candidate, port);
            }
        }
    }
}

void block::cleanup_connections() const noexcept {
    for (auto&& port : front().input_ports()) {
        disconnect(port);
    }
    for (auto&& port : back().output_ports()) {
        disconnect(port);
    }
}

void block::connect(
        block const& upstream, ::takatori::relation::expression::output_port_type const& upstream_port,
        block const& downstream, ::takatori::relation::expression::input_port_type const& downstream_port) {
    assert(upstream_port.opposite().get() == std::addressof(downstream_port)); // NOLINT

    auto&& upstream_expr = upstream.back();
    auto&& downstream_expr = downstream.front();
    assert(std::addressof(upstream_expr) == std::addressof(upstream_port.owner())); // NOLINT
    assert(std::addressof(downstream_expr) == std::addressof(downstream_port.owner())); // NOLINT

    if (auto sz = upstream_expr.output_ports().size(); upstream.downstreams_.size() < sz) {
        upstream.downstreams_.resize(sz);
    }
    if (auto sz = downstream_expr.input_ports().size(); downstream.upstreams_.size() < sz) {
        downstream.upstreams_.resize(sz);
    }
    auto&& upstream_entry = upstream.downstreams_[upstream_port.index()];
    auto&& downstream_entry = downstream.upstreams_[downstream_port.index()];

    // clear existing connections
    upstream.disconnect(upstream_port);
    downstream.disconnect(downstream_port);

    // establish connection
    upstream_entry = std::addressof(const_cast<block&>(downstream)); // NOLINT
    downstream_entry = std::addressof(const_cast<block&>(upstream)); // NOLINT
}

void block::disconnect(::takatori::relation::expression::input_port_type const& port) const noexcept {
    assert(std::addressof(port.owner()) == front_); // NOLINT
    if (port.index() < upstreams_.size() && !port.opposite()) {
        auto&& entry = upstreams_[port.index()];
        if (entry != nullptr) {
            auto&& opposite_entry = entry->downstreams_[port.opposite()->index()];
            assert(opposite_entry == this); // NOLINT
            entry = nullptr;
            opposite_entry = nullptr;
        }
    }
}

void block::disconnect(::takatori::relation::expression::output_port_type const& port) const noexcept {
    assert(std::addressof(port.owner()) == back_); // NOLINT
    if (port.index() < downstreams_.size() && !port.opposite()) {
        auto&& entry = downstreams_[port.index()];
        if (entry != nullptr) {
            auto&& opposite_entry = entry->upstreams_[port.opposite()->index()];
            assert(opposite_entry == this); // NOLINT
            entry = nullptr;
            opposite_entry = nullptr;
        }
    }
}

} // namespace yugawara::analyzer
