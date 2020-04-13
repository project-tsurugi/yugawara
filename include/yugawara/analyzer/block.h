#pragma once

#include <boost/container/small_vector.hpp>

#include <takatori/relation/expression.h>
#include <takatori/graph/graph.h>
#include <takatori/util/reference_list_view.h>
#include <takatori/util/object_creator.h>

#include "details/block_expression_iterator.h"

namespace yugawara::analyzer {

/**
 * @brief represents a block of relational expressions.
 * @details To build a graph of blocks, use block_builder instead.
 * @see block_builder
 */
class block {
public:
    /// @brief the value type
    using value_type = ::takatori::relation::expression;

    /// @brief the L-value reference type
    using reference = std::add_lvalue_reference_t<value_type>;
    /// @brief the const L-value reference type
    using const_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;

    /// @brief the pointer type
    using pointer = std::add_pointer_t<value_type>;
    /// @brief the const pointer type
    using const_pointer = std::add_pointer_t<std::add_const_t<value_type>>;

    /// @brief the graph type
    using graph_type = ::takatori::graph::graph<block>;

    /**
     * @brief a list view type.
     * @tparam T the element type
     */
    template<class T>
    using list_view = ::takatori::util::reference_list_view<::takatori::util::double_pointer_extractor<T>>;

    /// @brief the iterator type
    using iterator = details::block_expression_iterator<value_type>;
    /// @brief the const iterator type
    using const_iterator = details::block_expression_iterator<std::add_const_t<value_type>>;

    /**
     * @brief creates a new instance.
     * @param front the first relational expression of the block
     * @param back the last relational expression of the block
     * @param creator the object creator
     */
    block(reference front, reference back, ::takatori::util::object_creator creator = {}) noexcept;
    
    ~block();

    block(block const& other) = delete;
    block& operator=(block const& other) = delete;
    block(block&& other) noexcept = delete;
    block& operator=(block&& other) = delete;

    /**
     * @brief creates a new object.
     * @param other the copy source
     * @param creator the object creator
     */
    explicit block(block const& other, ::takatori::util::object_creator creator);

    /**
     * @brief creates a new object.
     * @param other the move source
     * @param creator the object creator
     */
    explicit block(block&& other, ::takatori::util::object_creator creator);

    /**
     * @brief returns a clone of this object.
     * @param creator the object creator
     * @return the created clone
     */
    [[nodiscard]] block* clone(::takatori::util::object_creator creator) const&;

    /// @copydoc clone()
    [[nodiscard]] block* clone(::takatori::util::object_creator creator) &&;

    /**
     * @brief returns the graph which owns this vertex.
     * @return the owner
     * @throws std::domain_error if this vertex is orphaned
     * @see is_orphaned()
     * @see optional_owner()
     */
    [[nodiscard]] graph_type& owner();

    /// @copydoc owner()
    [[nodiscard]] graph_type const& owner() const;

    /**
     * @brief returns the first relational expression of this block.
     * @return the first relational expression
     */
    [[nodiscard]] reference front() noexcept;

    /// @copydoc front()
    [[nodiscard]] const_reference front() const noexcept;

    /**
     * @brief returns the last relational expression of this block.
     * @return the last relational expression
     */
    [[nodiscard]] reference back() noexcept;

    /// @copydoc back()
    [[nodiscard]] const_reference back() const noexcept;

    /**
     * @brief returns a forward iterator which points the beginning of this container.
     * @return the iterator of beginning (inclusive)
     */
    [[nodiscard]] iterator begin() noexcept;

    /// @copydoc begin()
    [[nodiscard]] const_iterator begin() const noexcept;

    /**
     * @brief returns a forward iterator which points the ending of this container.
     * @return the iterator of ending (exclusive)
     */
    [[nodiscard]] iterator end() noexcept;

    /// @copydoc end()
    [[nodiscard]] const_iterator end() const noexcept;

    /**
     * @brief returns the incoming block of the given input.
     * @param port the input port
     * @return the upstream block
     * @see add_upstream()
     * @warning undefined behavior if the upstream is not registered
     */
    [[nodiscard]] block& upstream(::takatori::relation::expression::input_port_type const& port);
    
    /// @copydoc upstream()
    [[nodiscard]] block const& upstream(::takatori::relation::expression::input_port_type const& port) const;

    /**
     * @brief returns the outgoing block of the given output.
     * @param port the output port
     * @return the downstream block
     * @see add_downstream()
     * @warning undefined behavior if the downstream is not registered
     */
    [[nodiscard]] block& downstream(::takatori::relation::expression::output_port_type const& port);

    /// @copydoc downstream()
    [[nodiscard]] block const& downstream(::takatori::relation::expression::output_port_type const& port) const;

    /**
     * @brief returns the view of incoming blocks.
     * @return the view of upstream blocks
     * @warning undefined behavior if any upstreams are not registered
     */
    [[nodiscard]] list_view<block> upstreams();

    /// @copydoc upstreams()
    [[nodiscard]] list_view<block const> upstreams() const;

    /**
     * @brief returns the view of outgoing blocks.
     * @return the view of downstream blocks
     * @warning undefined behavior if any downstreams are not registered
     */
    [[nodiscard]] list_view<block> downstreams();

    /// @copydoc downstreams()
    [[nodiscard]] list_view<block const> downstreams() const;

    /**
     * @brief handles when this step is joined into the given graph.
     * @param graph the owner graph
     */
    void on_join(graph_type* graph) noexcept;

    /**
     * @brief handles when this vertex is left from the joined graph.
     */
    void on_leave() noexcept;

private:
    static constexpr inline std::size_t neighbor_buffer_size = 2;

    pointer front_;
    pointer back_;
    graph_type* owner_ {};
    mutable ::boost::container::small_vector<block*, neighbor_buffer_size, ::takatori::util::object_allocator<block*>> upstreams_;
    mutable ::boost::container::small_vector<block*, neighbor_buffer_size, ::takatori::util::object_allocator<block*>> downstreams_;

    block& find(::takatori::relation::expression::input_port_type const& port) const;
    block& find(::takatori::relation::expression::output_port_type const& port) const;

    void validate_inputs() const;
    void validate_outputs() const;

    void rebuild_connections() const;
    void cleanup_connections() const noexcept;

    static void connect(
            block const& upstream, ::takatori::relation::expression::output_port_type const& upstream_port,
            block const& downstream, ::takatori::relation::expression::input_port_type const& downstream_port);

    void disconnect(::takatori::relation::expression::input_port_type const& port) const noexcept;
    void disconnect(::takatori::relation::expression::output_port_type const& port) const noexcept;
};

} // namespace yugawara::analyzer
