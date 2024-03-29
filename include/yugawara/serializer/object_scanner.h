#pragma once

#include <memory>

#include <takatori/serializer/object_scanner.h>

#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/analyzer/expression_mapping.h>
#include <yugawara/analyzer/variable_mapping.h>

namespace yugawara::serializer {

/**
 * @brief an extension of takatori's 'object_scanner, which enables to analyze inside of descriptors.
 */
class object_scanner : public ::takatori::serializer::object_scanner {
public:
    /**
     * @brief creates a new instance, which does not inspect verbose information.
     */
    constexpr object_scanner() = default;

    /**
     * @brief creates a new instance.
     * @details The mapping information enables more information for elements.
     * @param variable_mapping the variable mapping (nullable)
     * @param expression_mapping the expression mapping (nullable)
     * @see ::yugawara::analyzer::expression_analyzer::shared_variables()
     * @see ::yugawara::analyzer::expression_analyzer::shared_expressions()
     */
    object_scanner(
            ::takatori::util::maybe_shared_ptr<analyzer::variable_mapping const> variable_mapping,
            ::takatori::util::maybe_shared_ptr<analyzer::expression_mapping const> expression_mapping) noexcept;

    /**
     * @brief sets the variable mapping.
     * @param mapping the mapping (nullable)
     * @return this
     */
    object_scanner& variable_mapping(::takatori::util::maybe_shared_ptr<analyzer::variable_mapping const> mapping) noexcept;

    /**
     * @brief sets the expression mapping.
     * @param mapping the mapping (nullable)
     * @return this
     */
    object_scanner& expression_mapping(::takatori::util::maybe_shared_ptr<analyzer::expression_mapping const> mapping) noexcept;

    /**
     * @brief scans an element and pass its structural elements into the given acceptor.
     * @param element the target element
     * @param acceptor the acceptor
     */
    using ::takatori::serializer::object_scanner::operator();

protected:
    using ::takatori::serializer::object_scanner::properties;

    /**
     * @brief scans properties of the given descriptor.
     * @param element the target element
     * @param acceptor the acceptor
     * @note This may be called only if verbose() is enabled.
     */
    void properties(::takatori::descriptor::variable const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /// @copydoc properties(::takatori::descriptor::variable const&, ::takatori::serializer::object_acceptor&) const
    void properties(::takatori::descriptor::relation const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /// @copydoc properties(::takatori::descriptor::variable const&, ::takatori::serializer::object_acceptor&) const
    void properties(::takatori::descriptor::function const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /// @copydoc properties(::takatori::descriptor::variable const&, ::takatori::serializer::object_acceptor&) const
    void properties(::takatori::descriptor::aggregate_function const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /// @copydoc properties(::takatori::descriptor::variable const&, ::takatori::serializer::object_acceptor&) const
    void properties(::takatori::descriptor::schema const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /// @copydoc properties(::takatori::descriptor::variable const&, ::takatori::serializer::object_acceptor&) const
    void properties(::takatori::descriptor::storage const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /**
     * @brief scans properties of the given type.
     * @param element the target element
     * @param acceptor the acceptor
     */
    void properties(::takatori::type::data const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /**
     * @brief scans properties of the given expression.
     * @param element the target element
     * @param acceptor the acceptor
     */
    void properties(::takatori::scalar::expression const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /**
     * @brief scans properties of the given statement.
     * @param element the target element
     * @param acceptor the acceptor
     */
    void properties(::takatori::statement::statement const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

private:
    ::takatori::util::maybe_shared_ptr<analyzer::variable_mapping const> variable_mapping_ {};
    ::takatori::util::maybe_shared_ptr<analyzer::expression_mapping const> expression_mapping_ {};

    template<::takatori::descriptor::descriptor_kind Kind>
    void accept_properties(::takatori::descriptor::element<Kind> const& element, ::takatori::serializer::object_acceptor& acceptor) const;

    template<class T>
    void accept_resolution(T const& element, ::takatori::serializer::object_acceptor& acceptor) const;
};

} // namespace yugawara::serializer
